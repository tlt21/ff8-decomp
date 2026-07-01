#ifndef MAIN_H
#define MAIN_H

#include "common.h"
#include "cd.h"            /* CdFileDesc */
#include "psxsdk/libgpu.h" /* TILE */

/* =============================================================== *
 *  Public interface — functions in main.c called by other units. *
 * =============================================================== */

/** @brief Initializes two full-screen black TILE primitives for screen clearing. */
void InitClearTiles(void);

/** @brief Copies the framebuffer from the current display buffer to the other. */
void copyFramebuffer(void);

/* =============================================================== *
 *  Private — types, globals and helpers internal to main.c.       *
 * =============================================================== */

/** @brief Fade effect mode for the rendering system. */
typedef enum {
    FADE_NONE = 0,
    FADE_IN   = 1,
    FADE_OUT  = 2
} FadeMode;

/** @brief VSync rendering dispatch mode. */
typedef enum {
    RENDER_IDLE    = 0,
    RENDER_FADE    = 1,
    RENDER_BATTLE  = 2,
    RENDER_OVERLAY = 3,
    RENDER_GAME    = 4
} RenderMode;

/** @brief Layout of the snapshot region within g_gameState (offsets 0xD40-0xD5C).
 *  Used by func_80011870 (save) and RestoreSnapshot (restore).
 */
typedef struct {
    u8  pad0[0xD40];      /* 0x000..0xD3F */
    u16 vsync_rate;       /* 0xD40 */
    u16 music_track;      /* 0xD42 */
    u16 field_120;        /* 0xD44 */
    u16 positions_x[3];   /* 0xD46..0xD4B */
    u16 positions_y[3];   /* 0xD4C..0xD51 */
    u16 rotations[3];     /* 0xD52..0xD57 */
    u8  anim_states[3];   /* 0xD58..0xD5A */
    u8  fade1;            /* 0xD5B */
    u8  fade0;            /* 0xD5C */
} SnapshotBuf;

/* Display / render state owned by main.c. */
extern u16            g_currentMusicTrack;
extern TILE           g_clearTiles[];
extern volatile u16   g_bufferIndex; /* volatile for codegen match (forces sign extension, prevents CSE) */
extern volatile u8    g_fadeMode;    /* volatile for codegen match (forces reload each access) */
extern u32            g_orderingTablePtrs[];

/* VSync callback state owned by main.c. */
extern u8             g_vsyncSkip;
extern volatile s32   D_8005F154; /**< VSync frame-counter timing accumulator (+0x88F/VSync). */
extern volatile s32   D_8005F15C; /**< VSync countdown-timer timing accumulator (+0x88F/VSync). */
extern u16            D_8005F11E; /**< VSync-done / status flag. */

/* CD file-table descriptors + scratch buffers loaded/managed by main.c. */
extern CdFileDesc     g_fileTableDesc[];
extern CdFileDesc     D_80097400[];
extern CdFileDesc     D_80097410[];
extern CdFileDesc     D_800974D0[];
extern CdFileDesc     D_800974D8[];
extern CdFileDesc     D_80097808[];
extern u8             D_80067468[];
extern u8             D_8006A468[];
extern u8             D_8005F188[];
extern u8             D_80070468[];

void flushGpuOt(void);
void VsyncHandler(void);
void InitHardware(void);
void func_80011870(void);
void RestoreSnapshot(void);
void loadFieldDataA(void);
void loadFieldDataB(void);
void loadSecondaryData(void);
void initSoundEngine(void);
void loadCdOverlay(void);
void loadAndUploadTextures(void);
void initFieldModule(void);
void initBattleAssets(void);
void loadFileTable(void);
void SetupDrawMode(s32 mode);
void BuildPrimList(s32 brightness);
void ProcessFade(void);

#endif /* MAIN_H */
