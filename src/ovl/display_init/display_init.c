#include "common.h"
#include "psxsdk/libgpu.h"
#include "psxsdk/libetc.h"
#include "psxsdk/libcd.h"

extern u32 D_800990B8[];
extern s32 D_8009928C;
extern s32 D_80099290;
extern s32 D_80099294;

/** @brief Display-init double-buffer context (184 bytes at 0x800991D8). */
typedef struct {
    u32     ot[2];       /**< 0x00: double-buffered single-entry OT. */
    DRAWENV draw;        /**< 0x08: 92-byte drawing environment. */
    DISPENV disp;        /**< 0x64: 20-byte display environment. */
    u8      pad78[0x30]; /**< 0x78: scratch / unidentified. */
    s32     currBuf;     /**< 0xA8: active buffer index (0 or 1). */
    s32     unkAC;
    u8      unkB0;       /**< 0xB0: flag byte. */
    u8      unkB1;
    u8      pad[6];
} DispCtx;

extern DispCtx D_800991D8;

extern void sndCmdF0(void);
extern void sndCmdF1(void);
extern s32  func_8004D174(void);
extern s32  func_8004D208(s32 a);
extern void func_8009818C(void);

/**
 * @brief Display-init overlay entry point — set up display + run 60-frame intro.
 *
 * Boot-time setup for the intro sequence (Squaresoft logo / opening movie
 * preface) before the main menu. Steps:
 *  - Ring the sound side (@c sndCmdF0/F1) and wait for the sound CPU to
 *    drain its pending command queue (@c func_8004D174 / @c func_8004D208).
 *  - Disable the display, sync the CD module to a known idle state, and
 *    issue @c CdControlB(0xE, ...) — CD command 0xE is @c CdlSetmode with
 *    param @c 0x80 (auto-pause + double-speed reporting).
 *  - Set up a 640x480 interlaced display: clear VRAM, build the
 *    @c DRAWENV / @c DISPENV in @c D_800991D8, mark dfe on the draw env
 *    and @c isinter on the disp env, install both with @c PutDrawEnv /
 *    @c PutDispEnv. Clear the active OT entry.
 *  - Run the per-frame intro tick @c func_8009818C 60 times (one second
 *    at 60 Hz).
 */
void func_80098000(void) {
    RECT clearRect;
    s32 status;
    s32 i;

    sndCmdF0();
    sndCmdF1();

    while ((status = func_8004D174()) == -1) {
        VSync(0);
    }
    if (status != 0) {
        while (func_8004D208(1) != 0) {}
    }

    SetDispMask(0);
    CdSync(0, 0);
    CdControlB(0xE, (u8 *)0x80, 0);
    VSync(4);

    clearRect.x = 0;
    clearRect.y = 0;
    clearRect.w = 0x280;
    clearRect.h = 0x1E0;
    ClearImage(&clearRect, 0, 0, 0);
    DrawSync(0);

    SetDefDispEnv(&D_800991D8.disp, 0, 0, 0x280, 0x1E0);
    SetDefDrawEnv(&D_800991D8.draw, 0, 0, 0x280, 0x1E0);

    D_800991D8.disp.isinter = 1;
    D_800991D8.draw.dfe = 1;
    PutDispEnv(&D_800991D8.disp);
    PutDrawEnv(&D_800991D8.draw);
    D_800991D8.currBuf = 0;
    D_800991D8.unkB0 = 0;
    D_800991D8.unkB1 = 0;
    D_80099294 = 0;
    VSync(0);
    D_8009928C = ((u32)GetODE() < 1);
    ClearOTagR(&D_800991D8.ot[D_800991D8.currBuf], 1);
    for (i = 0; i < 60; i++) {
        func_8009818C();
    }
}

INCLUDE_ASM("asm/ovl/display_init/nonmatchings/display_init", func_8009818C);

INCLUDE_ASM("asm/ovl/display_init/nonmatchings/display_init", func_80098338);

INCLUDE_ASM("asm/ovl/display_init/nonmatchings/display_init", func_80098378);

INCLUDE_ASM("asm/ovl/display_init/nonmatchings/display_init", func_80098440);

/**
 * @brief Load and play a specific music track for the display init overlay.
 *
 * Reads CD file entry at offset 0x110 (entry 34) from D_800990B8 and
 * calls cdReadAsyncSync to load it to address 0x8017D000.
 */
void func_800985B4(void) {
    u32 *base = D_800990B8;

    cdReadAsyncSync(*(u32 *)((u8 *)base + 0x110),
                  *(u32 *)((u8 *)base + 0x114),
                  (s32)0x8017D000, 0);
}

/**
 * @brief Transfer display rows from VRAM buffer at 0x8017D008.
 *
 * Computes a VRAM source offset using D_8009928C and copies scanlines
 * starting at y = (u16)D_8009928C + 0x26, incrementing by 2 each iteration,
 * until y >= 0x1BC.
 */
void func_800985EC(void) {
    s16 rect[4];
    s32 val;
    s16 y;
    u8 *ptr;

    val = D_8009928C;
    rect[0] = 0x1E;
    rect[2] = 0x244;
    rect[3] = 1;
    y = (u16)D_8009928C + 0x26;
    rect[1] = y;
    ptr = (u8 *)((s32)0x8017D008 + val * 0x488);

    if ((s16)y < 0x1BC) {
        do {
            LoadImage(&rect, ptr);
            y = rect[1] + 2;
            rect[1] = y;
            ptr += 0x910;
        } while ((s16)y < 0x1BC);
    }
}

/**
 * @brief Perform a display fade-out and fade-in transition.
 *
 * Fades display brightness from 255 down to 0 (by 4 each step),
 * waits 300 frames, then fades back up from 0 to 255. Updates
 * D_80099290 state flag and D_8009928C/D_80099294 during the transition.
 */
void func_8009869C(void) {
    s16 rect[4];
    s32 brightness;
    s32 i;

    D_80099290 = 2;
    VSync(0);

    i = GetODE();
    brightness = 0xFF;
    D_8009928C = (u32)i < 1;

    rect[0] = 0x1E;
    rect[1] = 0x26;
    rect[2] = 0x244;
    rect[3] = 0x196;

    /* Fade out: brightness 255 -> 0 by 4 */
    do {
        func_80098440(brightness, 2, &rect);
        brightness -= 4;
        func_8009818C();
        D_80099294 = 1;
        SetDispMask(1);
    } while (brightness >= 0);

    /* Wait 300 frames */
    i = 0;
    do {
        i++;
        func_8009818C();
    } while (i < 0x12C);

    /* Fade in: brightness 0 -> 255 by 4 */
    i = 0;
    do {
        func_80098440(i, 2, &rect);
        i += 4;
        func_8009818C();
    } while (i < 0xFF);

    SetDispMask(0);
    D_80099290 = 1;
}

INCLUDE_ASM("asm/ovl/display_init/nonmatchings/display_init", func_8009879C);

INCLUDE_ASM("asm/ovl/display_init/nonmatchings/display_init", func_80098974);

/**
 * @brief Display initialization entry point with mode selection.
 *
 * Configures display buffers, selects a display mode (0 or 1), runs
 * the corresponding intro sequence, then resets the display back.
 *
 * @param mode 0 for the standard intro sequence (func_80098974),
 *             1 for the alternate intro sequence (func_8009879C).
 */
void func_80098FD4(s32 mode) {
    s16 rect[4];

    ResetGraph(1);

    rect[0] = 0x100;
    rect[1] = 0xE0;
    rect[2] = 0x40;
    rect[3] = 0x20;
    MoveImage(&rect, 0, 0x1E0);
    DrawSync(0);
    VSync(0);

    switch (mode) {
    case 0:
        D_80099290 = 0;
        func_80098974();
        break;
    case 1:
        D_80099290 = mode;
        func_8009879C();
        break;
    }

    SetDispMask(0);

    rect[1] = 0x1E0;
    rect[2] = 0x40;
    rect[0] = 0;
    rect[3] = 0x20;
    MoveImage(&rect, 0x100, 0xE0);
    DrawSync(0);
    VSync(0);
}
