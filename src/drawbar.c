#include "common.h"
#include "psxsdk/libgpu.h"
#include "drawbar.h"

// This is likely something like drawTextBox.

/*
 * Clipped colour-bar GPU primitive builders.
 *
 * These were originally split into the psxsdk/libgcc.c translation unit because they sit
 * immediately before the GCC 64-bit-math runtime (__udivdi3) and splat groups by address.
 * They are GPU drawers, not math helpers: func_8002B3A0 unpacks its RECT argument and emits
 * GP0 packets (e.g. the 0xE100041E draw-mode word), so they were re-split out into this file.
 */

INCLUDE_ASM("asm/nonmatchings/drawbar", func_8002B3A0);

/**
 * @brief Draw a clipped colour bar with fill mode 3 — a thin wrapper around func_8002B3A0.
 *
 * @param ot    Ordering table the bar links into.
 * @param prim  Running primitive cursor.
 * @param rect  Bar rectangle.
 * @param color Fill colour.
 * @return The advanced primitive cursor.
 */
DR_AREA *func_8002B898(P_TAG *ot, DR_AREA *prim, RECT *rect, s32 color) {
    return func_8002B3A0(ot, prim, rect, color, 3);
}

INCLUDE_ASM("asm/nonmatchings/drawbar", func_8002B8BC);
