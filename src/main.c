#include "common.h"
#include "cd.h"
#include "intro.h"
#include "field/fe_object1.h"
#include "field/fe_object6.h"
#include "field/fe_object12.h"
#include "field.h"
#include "gamestate.h"
#include "battle.h"
#include "thread.h"
#include "btl_anim.h"
#include "overlay.h"
#include "game.h"
#include "cdrom.h"
#include "snd_init.h"
#include "psxsdk/libapi.h"
#include "psxsdk/libspu.h"
#include "psxsdk/crt0.h"
#include "psxsdk/libgpu.h"
#include "psxsdk/libgte.h"
#include "psxsdk/libetc.h"
#include "main.h"


/** @brief Clears the GPU ordering tables and flushes the GPU pipeline.
 *
 *  Builds a minimal OT, submits it via DrawOTag, and spins on DrawSync
 *  until rendering completes. Used to ensure the display is blank during
 *  transitions (e.g. waiting for a fade to finish).
 */

void flushGpuOt(void) {
    u32 prim[4];
    u32 ot[2];

    ClearOTag(ot, 2);
    SetDrawStp(prim, 0);
    addPrim(ot, prim);
    DrawOTag(ot);

    do {
    } while (DrawSync(1) != 0);
}

/** @brief VSync callback handler, registered via VSyncCallback in InitHardware.
 *
 *  Dispatches per-frame rendering based on the game mode (g_renderMode):
 *  - 0: no action (rendering idle)
 *  - 1: calls ProcessFade (normal frame draw/swap with fade)
 *  - 2: calls func_80026D8C (battle VSync handler)
 *  - 3: calls func_800D0608 (overlay-loaded VSync handler)
 *  - 4: calls vsyncGameHandler (game code VSync handler)
 *
 *  Also maintains two fixed-point frame timing accumulators (D_8005F154 and
 *  D_8005F15C) that increment by 0x88F per VSync. On bit-17 rollover (~every
 *  12 frames), one increments a game frame counter at g_gameState+0xCD0
 *  and the other decrements a countdown timer at g_gameState+0xCD4.
 */

/* --- VSync ISR helpers and timing globals (main-private) --- */

void VsyncHandler(void) {
    switch (g_renderMode) {
    case 0:
        break;
    case 1:
        ProcessFade();
        break;
    case 2:
        func_80026D8C();
        break;
    case 3:
        func_800D0608();
        break;
    case 4:
        vsyncGameHandler();
        break;
    }

    if (getAnimGlobalState() != 0) {
        D_8005F11E = 1;
        return;
    }
    if (g_vsyncSkip != 0) {
        D_8005F11E = 1;
        return;
    }

    D_8005F154 += 0x88F;
    if (D_8005F154 >> 17) {
        g_gameState.mainData.frameCounter++;
        D_8005F154 &= 0xFFFF;
    }

    D_8005F15C += 0x88F;
    if (D_8005F15C >> 17) {
        if (g_gameState.mainData.state.countdownTimer == 0) {
            D_8005F11E = 1;
            return;
        }
        g_gameState.mainData.state.countdownTimer--;
        D_8005F15C &= 0xFFFF;
    }

    D_8005F11E = 1;
}


/** @brief Initializes PS1 hardware subsystems.
 *
 *  Called once at the start of main. Sets up memory mode, resets the callback
 *  system, initializes GPU and GTE, starts SPU via InitSpu (SpuInit
 *  wrapper), registers the VSync callback, and disables display output until
 *  the game is ready to show content.
 */
void InitHardware(void) {
    SetMem(2);
    StopCallback();
    ResetCallback();
    ResetGraph(0);
    InitSpu();
    g_vsyncSkip = 0;
    g_renderMode = RENDER_IDLE;
    VSyncCallback(VsyncHandler);
    SetGraphDebug(0);
    SetDispMask(0);
    InitGeom();
}

/** @brief Saves the current battle/field camera state to a snapshot buffer.
 *
 *  Records party member positions (fixed-point 20.12 -> integer), rotations,
 *  animation states, and display parameters from the battle entity array
 *  (stride 612 at D_80085224) into g_gameState+0xD40..0xD5C. The snapshot
 *  is later restored by RestoreSnapshot when returning from battle.
 */
void func_80011870(void) {
    s16 i;

    if (D_8005F14C == 2) {
        ((SnapshotBuf *)&g_gameState)->vsync_rate = 2;
    } else {
        ((SnapshotBuf *)&g_gameState)->vsync_rate = 1;
    }

    ((SnapshotBuf *)&g_gameState)->music_track = g_currentMusicTrack;
    ((SnapshotBuf *)&g_gameState)->field_120 = g_fieldEntity.field_0x120;

    for (i = 0; i < 3; i++) {
        ((SnapshotBuf *)&g_gameState)->positions_x[i] = D_80085224[g_fieldEntity.entityIndex[i]].posX >> 12;
        ((SnapshotBuf *)&g_gameState)->positions_y[i] = D_80085224[g_fieldEntity.entityIndex[i]].posY >> 12;
        ((SnapshotBuf *)&g_gameState)->rotations[i]   = D_80085224[g_fieldEntity.entityIndex[i]].field_0x1FA;
        ((SnapshotBuf *)&g_gameState)->anim_states[i] = D_80085224[g_fieldEntity.entityIndex[i]].field_0x241;
    }

    ((SnapshotBuf *)&g_gameState)->fade1 = D_8005F151;
    ((SnapshotBuf *)&g_gameState)->fade0 = D_8005F150;
}

/** @brief Restores a previously saved camera/field state snapshot.
 *
 *  Inverse of func_80011870. Reads the snapshot from g_gameState+0xD40..0xD5C
 *  and writes values back into g_currentMusicTrack (music track), g_fieldEntity (field
 *  state structure), g_vsyncRate (VSync rate), and fade state variables.
 */
void RestoreSnapshot(void) {
    SnapshotBuf *buf;
    SystemState *entity;

    buf = (SnapshotBuf *)(s32)&g_gameState;

    g_vsyncRate = buf->vsync_rate;
    if (g_vsyncRate == 0) {
        g_vsyncRate = 1;
    }

    g_currentMusicTrack = buf->music_track;

    entity = &g_fieldEntity;

    entity->field_0x120 = buf->field_120;
    entity->position_x = buf->positions_x[0];
    entity->position_y = buf->positions_y[0];
    entity->rotation = buf->rotations[0];
    entity->anim_state = buf->anim_states[0];

    D_8005F151 = buf->fade1;
    D_8005F150 = buf->fade0;
}


/** @brief Loads field data set A from CD into 0x80098000.
 *
 *  Uses D_80097410[0] as the CD file descriptor (.sector, .size). Skips
 *  loading if D_8005F14C is 6 (menu) or 0xA (special mode), since field data
 *  is not needed in those states. Calls func_8001F5C8 (sound state reset)
 *  afterward.
 */
void loadFieldDataA(void) {
    if ((s16)D_8005F14C != 6 && (s16)D_8005F14C != 0xA) {
        func_80038868(D_80097410[0].sector, D_80097410[0].size, 0x80098000, 0);
        while (func_800393C8() != 0)
            ;
    }
    func_8001F5C8();
}

/** @brief Loads field data set B from CD into 0x80098000.
 *
 *  Identical to loadFieldDataA but uses D_800974D0[0] as the source
 *  descriptor. Same state-skip logic and post-load sound reset.
 *
 *  @note Purpose uncertain — the difference between data sets A and B may
 *        correspond to different field maps or disc configurations.
 */
void loadFieldDataB(void) {
    if ((s16)D_8005F14C != 6 && (s16)D_8005F14C != 0xA) {
        func_80038868(D_800974D0[0].sector, D_800974D0[0].size, 0x80098000, 0);
        while (func_800393C8() != 0)
            ;
    }
    func_8001F5C8();
}

/** @brief Loads a secondary data block from CD into 0x80097940.
 *
 *  Uses D_80097410[1] as the CD file descriptor (.sector, .size) with
 *  synchronous CD read (cdRead). No state-skip check — always loads.
 *
 *  @note Purpose uncertain — destination is near the file table area,
 *        possibly a secondary asset table used during initialization.
 */
void loadSecondaryData(void) {
    cdRead(D_80097410[1].sector, D_80097410[1].size, 0x80097940, 0);
    while (func_800393C8() != 0)
        ;
}

/** @brief Initializes the sound/music engine by loading audio assets from CD.
 *
 *  1. Calls sndInit to initialize the SPU hardware.
 *  2. Loads the sound bank header from CD (D_800974D8[0]) into D_80067468
 *     and parses it via sndLoadBank.
 *  3. Loads two sample banks from CD (D_800974D8[1], D_800974D8[2]) into a
 *     scratch buffer at 0x801B0000 and uploads each to SPU RAM via
 *     sndUploadSamples.
 */
void initSoundEngine(void) {
    sndInit();

    cdRead(D_800974D8[0].sector, D_800974D8[0].size, (s32)D_80067468, 0);
    while (func_800393C8() != 0)
        ;

    sndLoadBank((s32)D_80067468);

    cdRead(D_800974D8[1].sector, D_800974D8[1].size, 0x801B0000, 0);
    while (func_800393C8() != 0)
        ;

    sndUploadSamples(0x801B0000, 1);

    cdRead(D_800974D8[2].sector, D_800974D8[2].size, 0x801B0000, 0);
    while (func_800393C8() != 0)
        ;

    sndUploadSamples(0x801B0000, 1);
}


/** @brief Loads an overlay from CD into 0x80098000.
 *
 *  Uses D_80097400[0] as the CD file descriptor (.sector, .size). The
 *  specific overlay loaded depends on what D_80097400 was populated with
 *  (set by loadFileTable which loads the master file table).
 */
void loadCdOverlay(void) {
    cdRead(D_80097400[0].sector, D_80097400[0].size, 0x80098000, 0);
    while (func_800393C8() != 0)
        ;
}


/** @brief Loads texture data from CD and uploads it to VRAM.
 *
 *  Reads data from CD using D_80097808[0] (.sector, .size) into scratch
 *  buffer 0x801B0000, then calls func_8002C3AC to process and upload the
 *  textures to GPU VRAM. Spins on DrawSync until the GPU transfer completes.
 */
void loadAndUploadTextures(void) {
    cdRead(D_80097808[0].sector, D_80097808[0].size, 0x801B0000, 0);
    while (func_800393C8() != 0)
        ;
    func_8002C3AC(0x801B0000, 0);
    while (DrawSync(1) != 0)
        ;
}

/** @brief Loads an overlay and initializes the field/battle module.
 *
 *  Calls loadCdOverlay to load the overlay at 0x80098000, then runs its
 *  init function (func_80098390) and calls initBattleDisplay for additional setup.
 */
void initFieldModule(void) {
    loadCdOverlay();
    func_80098390();
    initBattleDisplay();
}

/** @brief Extended battle/visual initialization — loads multiple CD assets.
 *
 *  1. Loads overlay via loadCdOverlay.
 *  2. Initializes a 0x6000-byte buffer at D_8006A468 via initBattleAnimSystem.
 *  3. Loads and uploads textures to VRAM via loadAndUploadTextures.
 *  4. Loads battle entity/model data and processes it via loadBattleTimImage.
 *  5. Loads a lookup table and copies 0x200 bytes via func_80039678.
 *  6. Loads additional data and stores its pointer via setBattleEntityBase.
 */
void initBattleAssets(void) {
    loadCdOverlay();
    initBattleAnimSystem((s32)D_8006A468, 0x6000);
    loadAndUploadTextures();

    cdRead(D_80097808[1].sector, D_80097808[1].size, 0x801B0000, 0);
    while (func_800393C8() != 0)
        ;

    loadBattleTimImage(0x801B0000);

    cdRead(D_80097808[3].sector, D_80097808[3].size, 0x80090000, 0);
    while (func_800393C8() != 0)
        ;

    func_80039678((s32)D_8005F188, 0x80090000, 0x200);

    cdRead(D_80097808[2].sector, D_80097808[2].size, 0x80090000, 0);
    while (func_800393C8() != 0)
        ;

    setBattleEntityBase(0x80090000);
}


/** @brief Loads the master file descriptor table from CD into 0x80097400.
 *
 *  Uses the hard-coded bootstrap descriptor g_fileTableDesc[0] (.sector,
 *  .size). This must be called before any other CD loading functions, since
 *  all other load descriptors (D_80097410, D_800974D0, D_800974D8,
 *  D_80097808) reside within the table loaded here.
 */
void loadFileTable(void) {
    cdRead(g_fileTableDesc[0].sector, g_fileTableDesc[0].size, 0x80097400, 0);
    while (func_800393C8() != 0)
        ;
}

/* --- main state-machine helpers (main-binary internal, declared where used) --- */
/* Remaining external deps that resist a clean owner header:
 *  - func_80098000 / func_800987D8: field_init overlay entry points, invoked
 *    after the overlay is loaded (cross-overlay boundary; no shared header).
 *  - sndStopAll / sndCmd21: owned by snd_init.h, but including it conflicts
 *    (that header redeclares sndCmd10/toggleSoundBank vs other snd headers). */
extern void func_80098000(void);
extern s32  func_800987D8(void);

/**
 * @brief Battle-map transition entry (stride 0x18) in the table loaded at
 *        0x80097940 by loadSecondaryData. Indexed by @c D_80082C8C.unk02 when
 *        the field engine requests a scripted map jump (@c D_80082C8C.mode == 1).
 */
typedef struct {
    /* 0x00 */ u16 position_x;
    /* 0x02 */ u16 position_y;
    /* 0x04 */ u16 rotation;
    /* 0x06 */ s16 musicTrack;
    /* 0x08 */ u8  anim_state;
    /* 0x09 */ u8  pad09[0xF];
} BattleMapEntry; /* 0x18 */

/* The post-battle result handling reads several fields of the field-vars
 * block through @ref g_fieldVars (0x800562C4): @c stateFlags (bit 0x40 cleared
 * on a "continue" result), @c fieldB6 (bits 0x100 / 0x200 gate the game-over
 * music/state paths), @c expectedDiscId (compared against @c getDiscId to
 * detect a disc change), and @c fieldD1 (OR'ed with 1 into the disc-change
 * transition argument). */


/** @brief Game main function and primary state machine loop.
 *
 *  Initialization sequence: hardware init, CD-ROM init, loads file table,
 *  sound engine, overlays, and field/battle data.
 *
 *  Main loop runs until g_fieldEntity.mode == 4 (game exit). The inner
 *  dispatcher uses (g_vsyncRate - 1) as the state selector:
 *  - State 1: Load field audio / run overlay update; stage a map jump.
 *  - State 2/3: Field-to-battle transition, battle execution, result handling.
 *  - State 5/9: Post-battle fade-out and disc-change handling.
 *  - State 7/8: Overlay reload / disc swap.
 *
 *  On exit (g_fieldEntity.mode == 4), shuts down sound, resets the GPU, and
 *  reinitializes everything from the top.
 */
/* gcc 2.7.2 injects a __main() C-runtime-init call into any function literally
 * named `main`, which the shipped executable does not have. Writing it as
 * ff8main and aliasing the symbol to `main` emits the correct `main` symbol
 * without that call. */
void ff8main(void) __asm__("main");
void ff8main(void) {
    BattleMapEntry *entry;
    s16 i;
    s16 mode;
    s16 musicTrack;
    s32 result;
    s32 mapAddr;
    preInitStub();
    InitHardware();
    initCdSubsystem(1);
    loadFileTable();
    initSoundEngine();
    loadCdOverlay();
    func_80098000();
    initFieldModule();
    loadSecondaryData();
    while (1) {
        func_80038868(D_80097400[1].sector, D_80097400[1].size, 0x80098000, 0);
        while (func_800393C8() != 0) {}

        func_80098FD4(0);
        initBattleAssets();
        g_vsyncRate = 1;
        D_8005F14C = 0;
        g_fieldEntity.unk1A3 = 0;
        D_8005F14A = 0;
        D_8005F100 = 0x49;
        g_fieldEntity.mode = 0;
        g_fieldEntity.unk1A2 = 0;
        result = func_8003646C(25);
        if (!(result & 2)) {
            RestoreSnapshot();
            loadFieldDataA();
            func_800C00C8(0);
        } else {
            g_currentMusicTrack = 0x4A;
            g_gameState.battleParty[0] = 0;
            g_gameState.battleParty[1] = 0xFF;
            g_gameState.battleParty[2] = 0xFF;
            g_gameState.battleParty[3] = 0xFF;
            loadFieldDataA();
            func_800C00C8(1);
        }
        if (g_fieldVars->expectedDiscId != getDiscId()) {
            func_80038868(D_80097400[1].sector, D_80097400[1].size, 0x80098000, 0);
            while (func_800393C8() != 0) {}

            func_80098FD4(1);
            loadFileTable();
            loadSecondaryData();
        }
        do {
            mode = g_vsyncRate - 1;
            do {
            switch (mode) {
            case 0:
                loadFieldDataA();
                func_8009895C();
                break;
            case 1:
                switch (D_8005F14C) {
                case 1:
                    func_800B4F40();
                    D_80082C8C.mode = 0;
                    break;
                case 3:
                    D_80082C8C.mode = 2;
                    break;
                case 6:
                    D_80082C8C.mode = 4;
                    break;
                case 0:
                    D_80082C8C.mode = 6;
                    break;
                }
                if (((D_8005F14C == 1) && (D_8005F14A != 0)) && (D_8005F100 < 0x4A)) {
                    while (func_800393C8() != 0) { do {} while (0); }
                    func_80038490(D_8005F104, 0x80098000);
                } else {
                    loadFieldDataB();
                }
                D_8005F14A = 0;
                if (func_800987D8() == 1) {
                    g_fieldEntity.counter = 0;
                    g_fieldEntity.mode = 4;
                    g_fieldEntity.rotation = 0x7FFF;
                    sndStopAll();
                    break;
                }
                D_8005F14C = 2;
                switch (D_80082C8C.mode) {
                case 1:
                    entry = & ((BattleMapEntry * ) 0x80097940)[D_80082C8C.unk02];
                    musicTrack = entry->musicTrack;
                    g_fieldEntity.position_x = entry->position_x;
                    g_fieldEntity.position_y = entry->position_y;
                    g_fieldEntity.rotation = entry->rotation;
                    g_currentMusicTrack = musicTrack;
                    g_fieldEntity.anim_state = entry->anim_state;
                    g_vsyncRate = 1;
                    sndCmd21(-1, 0);
                    break;
                case 3:
                    g_vsyncRate = 3;
                    break;
                case 5:
                    g_vsyncRate = 6;
                    break;
                }
                break;
            case 7:
                gameStateLoop();
                D_8005F14C = 3;
                g_vsyncRate = 1;
                break;
            case 2:
            case 3:
                if (D_8005F14C == 1) {
                    g_battleConfig.battleSceneId = g_fieldEntity.counter;
                    mapAddr = D_8005F13C;
                } else {
                    g_battleConfig.battleSceneId = ((u8) D_80082C8C.unk02) | (((u8) D_80082C8C.unk03) << 8);
                    mapAddr = 0x801D0000;
                }
                func_80038030(mapAddr);
                gameStateLoop();
                if (g_renderMode != 4) {
                    flushGpuOt();
                }
                if (g_battleConfig.result == 5) {
                    g_fieldEntity.counter = 0;
                    g_fieldEntity.mode = 4;
                    g_fieldEntity.rotation = 0x7FFF;
                    break;
                }
                if ((g_battleConfig.result == 1) && (!(g_fieldVars->fieldB6 & 0x200))) {
                    g_currentMusicTrack = 0x4B;
                    D_8005F14C = 0;
                    g_vsyncRate = 1;
                    break;
                }
                if (g_battleConfig.result == 3) {
                    if (g_fieldVars->fieldB6 & 0x100) {
                        g_fieldVars->stateFlags &= ~0x40;
                    } else {
                        g_currentMusicTrack = 0x4B;
                        D_8005F14C = 0;
                        g_vsyncRate = 1;
                        break;
                    }
                }
                if (D_8005F14C == 1) {
                    D_8005F14C = 3;
                    g_vsyncRate = 1;
                } else {
                    D_8005F14C = 3;
                    g_vsyncRate = 2;
                }
                for (i = 0; i < 3; i++) {
                    if (g_battleConfig.unk4[i] != 0xFF) {
                        do {} while (g_renderMode == 4);
                        while (DrawSync(1) != 0) {}
                        func_8003646C(g_battleConfig.unk4[i] + 5);
                    }
                }
                break;
            case 5:
            case 9:
                while (g_renderMode != 0) {}
                SetDispMask(0);
                func_80011870();
                if (D_8005F14C == 1) {
                    result = func_8003646C(g_fieldEntity.counter, g_fieldEntity.unk1AB);
                    if (result & 1) {
                        clearEntityFlags();
                        if (g_vsyncRate == 6) {
                            D_8005F14C = 0xA;
                        } else {
                            D_8005F14C = 6;
                        }
                    } else {
                        D_8005F14C = 6;
                    }
                    g_vsyncRate = 1;
                } else {
                    result = func_8003646C(0x80000000, g_fieldVars->fieldD1 | 1);
                    D_8005F14C = 6;
                    g_vsyncRate = 2;
                }
                if (!(result & 4)) {
                    break;
                }
                D_8005F14C = g_vsyncRate;
                g_vsyncRate = 3;
                g_battleConfig.battleSceneId = (u16)(result >> 16);
                if (g_vsyncRate == 1) {
                    mapAddr = D_8005F13C;
                } else {
                    result = 0x801D0000;
                    mapAddr = result;
                }
                func_80038030(mapAddr);
                gameStateLoop();
                if (g_renderMode != 4) {
                    flushGpuOt();
                }
                if (g_battleConfig.result == 5) {
                    do { } while (0);
                    g_fieldEntity.counter = 0;
                    g_fieldEntity.mode = 4;
                    g_fieldEntity.rotation = 0x7FFF;
                    break;
                }
                if ((g_battleConfig.result == 1) && (!(g_fieldVars->fieldB6 & 0x200))) {
                    g_currentMusicTrack = 0x4B;
                    D_8005F14C = 0;
                    g_vsyncRate = 1;
                    break;
                }
                if (g_battleConfig.result == 3) {
                    if (g_fieldVars->fieldB6 & 0x100) {
                        g_fieldVars->stateFlags &= ~0x40;
                    } else {
                        g_currentMusicTrack = 0x4B;
                        D_8005F14C = 0;
                        g_vsyncRate = 1;
                        break;
                    }
                }
                if (D_8005F14C == 1) {
                    D_8005F14C = 3;
                    g_vsyncRate = 1;
                } else {
                    D_8005F14C = 3;
                    g_vsyncRate = 2;
                }
                for (i = 0; i < 3; i++) {
                    if (g_battleConfig.unk4[i] != 0xFF) {
                        do {} while (g_renderMode == 4);
                        while (DrawSync(1) != 0) {}
                        func_8003646C(g_battleConfig.unk4[i] + 5);
                    }
                }
                break;
            case 8:
                func_80038868(D_80097400[1].sector, D_80097400[1].size, 0x80098000, 0);
                while (func_800393C8() != 0) {}
                func_80098FD4(g_fieldEntity.unk1A0);
                mode = g_fieldEntity.unk1A0;
                if (mode == 1) {
                    loadFileTable();
                    loadSecondaryData();
                    D_8005F14C = 0;
                    g_vsyncRate = mode;
                    g_currentMusicTrack = g_fieldEntity.counter;
                }
                break;
            }
            } while (0);
        }
        while (g_fieldEntity.mode != 4);
        sndStopAll();
        setAnimGlobalState(0);
        func_80027448();
        ResetGraph(3);
    }

}

/** @brief Sets up the GPU draw mode for both display buffers.
 *
 *  Computes a TPage value via GetTPage and applies it to both DR_MODE
 *  primitives (D_80070468 and D_80070468+0x20) for the double-buffered
 *  rendering system.
 *
 *  @param a0 Texture page semi-transparency mode (passed to GetTPage).
 *            Value 2 (8-bit) is used from ProcessFade.
 */
void SetupDrawMode(s32 a0) {
    u16 tpage;

    tpage = GetTPage(0, a0, 0, 0);
    SetDrawMode(D_80070468, 0, 0, tpage, 0);
    SetDrawMode(D_80070468 + 0x20, 0, 0, tpage, 0);
}

/** @brief Initializes two full-screen black TILE primitives for screen clearing.
 *
 *  Sets up a 320x224 black fill rectangle in each of the two double-buffered
 *  primitive slots at g_clearTiles (32 bytes apart). These are used for screen
 *  clearing during frame rendering.
 */
void InitClearTiles(void) {
    s32 i = 0;
    TILE *tile = &g_clearTiles[0];
top:
    SetTile(tile);
    SetSemiTrans(tile, 1);
    SetShadeTex(tile, 1);
    tile->w = 320;
    tile->r0 = 0;
    tile->g0 = 0;
    tile->b0 = 0;
    tile->h = 224;
    tile->x0 = 0;
    tile->y0 = 0;
    tile += 2;
    if (++i < 2) goto top;
}

/** @brief Copies the framebuffer from the current display buffer to the other.
 *
 *  Uses MoveImage to synchronize both framebuffers so they contain the same
 *  content. Reads source/destination coordinates from the DISPENV array at
 *  g_dispEnvs. Spins on DrawSync until the GPU transfer completes.
 *
 *  Logically equivalent to:
 *    DISPENV *src = &g_dispEnvs[g_bufferIndex];
 *    DISPENV *dst = &g_dispEnvs[(g_bufferIndex + 1) & 1];
 *    MoveImage(&src->disp, dst->disp.x, dst->disp.y);
 *  Raw pointer arithmetic is used for codegen matching (volatile triple-load,
 *  implicit ptr-to-int, explicit stride).
 */
void copyFramebuffer(void) {
    s32 base;
    s32 ofs1;

    ofs1 = (s16)g_bufferIndex * 20;
    base = g_dispEnvs;

    MoveImage(
        (void *)(ofs1 + base),
        *(s16 *)(((s16)g_bufferIndex + 1 & 1) * 20 + base),
        *(s16 *)(((s16)g_bufferIndex + 1 & 1) * 20 + base + 2)
    );

    do {
    } while (DrawSync(1) != 0);
}

/** @brief Builds and chains the per-frame GPU primitive list for rendering.
 *
 *  Clears the ordering table for the current buffer, sets the TILE fill
 *  color to the given brightness, and chains the TILE and DR_MODE primitives
 *  into the OT for submission via DrawOTag.
 *
 *  @param brightness Fill brightness (uniform RGB). 16 (dark grey) is used
 *                    during fade effects.
 */
void BuildPrimList(s32 brightness) {
    ClearOTag(&g_orderingTablePtrs[(s16)g_bufferIndex], 1);
    g_clearTiles[(s16)g_bufferIndex * 2].r0 = brightness;
    g_clearTiles[(s16)g_bufferIndex * 2].g0 = brightness;
    g_clearTiles[(s16)g_bufferIndex * 2].b0 = brightness;
    addPrim(&g_orderingTablePtrs[(s16)g_bufferIndex], &g_clearTiles[(s16)g_bufferIndex * 2]);
    addPrim(&g_orderingTablePtrs[(s16)g_bufferIndex], (DR_MODE *)&g_clearTiles[(s16)g_bufferIndex * 2] - 1);
}

/** @brief Main frame rendering function with double-buffer swap and fade effects.
 *
 *  Called from the VSync callback (VsyncHandler, case 0). Toggles the
 *  double buffer index, manages fade-in/fade-out frame counters, and submits
 *  the GPU primitive list for the current frame.
 *
 *  Fade behavior depends on g_fadeMode:
 *  - FADE_NONE: rendering disabled, returns immediately.
 *  - FADE_IN: fade-in mode — updates every 4th frame for 128 frames total.
 *  - FADE_OUT: fade-out mode — updates every frame for 34 frames total.
 *
 *  When the fade counter expires, sets g_renderMode=RENDER_IDLE and g_fadeMode=FADE_NONE to
 *  signal completion to the main loop.
 */
void ProcessFade(void) {
    if (g_fadeMode == FADE_NONE) {
        return;
    }

    g_bufferIndex = g_bufferIndex + 1;
    g_bufferIndex = g_bufferIndex & 1;

    if (g_fadeMode == FADE_IN) {
        s32 fc;
        fc = (u8)g_fadeCounter + 1;
        g_fadeCounter = fc;
        if ((fc & 0xFF) == 0x80) {
            g_renderMode = RENDER_IDLE;
            g_fadeMode = FADE_NONE;
        }
        if (fc & 0x6) {
            return;
        }
    } else {
        g_fadeCounter = (u8)g_fadeCounter + 1;
        if ((u8)g_fadeCounter == 0x22) {
            g_renderMode = RENDER_IDLE;
            g_fadeMode = FADE_NONE;
        }
    }

    SetupDrawMode(2);
    BuildPrimList(16);
    PutDispEnv(&g_dispEnvs[(s16)g_bufferIndex]);
    PutDrawEnv(&g_drawEnvs[(s16)g_bufferIndex]);
    DrawOTag(&g_orderingTablePtrs[(s16)g_bufferIndex]);
}
