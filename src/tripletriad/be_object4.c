#include "common.h"
#include "psxsdk/libgpu.h"
#include "tripletriad/be_object1.h"
#include "tripletriad/be_object4.h"

/** @brief One 0x0C-byte entry of the D_80182E70 per-SFX configuration table. */
typedef struct {
    /* 0x00 */ u8 flags;   /**< Bit 0 selects the stop-vs-start fade (see func_800A2054). */
    /* 0x01 */ u8 field2F; /**< Value written to each SFX entry's field 0x2F. */
    /* 0x02 */ u8 pitch;   /**< Pitch value. */
    /* 0x03 */ u8 pad03[9];
} SfxConfig;

/** @brief The Triple Triad board view + cursor state at D_801D49C8.
 *
 * func_800A4504 initializes the whole block; func_800A4478 sets the color
 * fields; func_800A44CC resets the cursor; func_800A390C runs the per-frame
 * cursor/timer state machine. */
typedef struct {
    /* 0x00 */ s16 x;            /**< View X position. */
    /* 0x02 */ s16 y;            /**< View Y position. */
    /* 0x04 */ s16 w;            /**< View width. */
    /* 0x06 */ s16 h;            /**< View height. */
    u8 pad08[8];
    /* 0x10 */ s32 packedColor;  /**< Packed RGB+0x64 brightness (see func_800A4478). */
    /* 0x14 */ s16 slideOffset;  /**< Fixed-point row-slide animation offset. */
    /* 0x16 */ u8 row;           /**< Current cursor row. */
    /* 0x17 */ u8 prevRow;       /**< Previous cursor row (saved on a move). */
    /* 0x18 */ s16 timer;        /**< Fixed-point step timer; fires at 0x1000. */
    /* 0x1A */ s16 timerStep;    /**< Amount added to @ref timer each frame. */
    /* 0x1C */ s16 brightness;   /**< Raw brightness (scaled by 32; see func_800A4478). */
    /* 0x1E */ s16 cursorPos;    /**< Grid position, row*11 + column. */
    /* 0x20 */ u8 unk20;
    /* 0x21 */ u8 unk21;
    /* 0x22 */ u8 state;         /**< State-machine state (0..5). */
    /* 0x23 */ u8 frameCounter;  /**< Incremented every frame. */
    /* 0x24 */ u8 unk24;
} CursorState;

extern void *func_8002FF34(s32 *otBase, void *pkt, s32 ch, s32 yPos, s32 w, s32 col);
extern void sendSpuCommand(s32 idx);
extern s16 D_801D49E2;
extern s16 D_801D49F8[];
extern s16 D_801D4B18;
extern s16 D_801D4B1A;
extern s32 D_801D4560;
extern s32 D_801D4B20[];
extern s32 D_801D4B28[];
extern s32 D_801D4B30[];
extern s32 g_menuColor[];
extern SfxConfig D_80182E70[];
extern u8 D_80182EC8[];
extern u8 D_801D4500[];
extern u8 D_801D4568[];
extern u8 D_801D4968[];
extern u8 D_801D4978[];
extern CursorState D_801D49C8;
extern u8 D_801D49EC;
extern u8 D_801D4A88[]; /**< Per-cell lookup value, indexed by cursor grid position. */
extern u8 D_801D4AF6;   /**< Total cell count of the cursor grid (rows of 11). */
extern s32 func_800A238C();
extern s32 func_800A279C();
extern void func_800A4504(s32 a0, s32 a1); /**< SFX (60, 32) init; defined below. */

/**
 * @brief Reset and reconfigure the seven battle SFX channels.
 *
 * Resets all sound effects, runs a (60, 32) init via @c func_800A4504, then for
 * each of the seven channels applies the per-channel settings from the
 * @c D_80182E70 config table: reverb mode = channel index, field 0x2F and pitch
 * from the table entry, and zeroed entry params.
 */
void func_800A1BE0(void)
{
    s32 i;

    resetAllSfx();
    func_800A4504(0x3C, 0x20);
    for (i = 0; i < 7; i++) {
        setSfxReverbMode(i, i);
        setSfxField2F(i, D_80182E70[i].field2F);
        setSfxPitch(i, D_80182E70[i].pitch);
        setSfxEntryParams(i, 0, 0);
    }
}

INCLUDE_ASM("asm/ovl/tripletriad/nonmatchings/be_object4", func_800A1C6C);

INCLUDE_ASM("asm/ovl/tripletriad/nonmatchings/be_object4", func_800A1D68);

/**
 * @brief Start or stop an SFX entry based on its type flag.
 *
 * Looks up the entry at D_80182E70[a0 * 12], checks bit 0 of byte 0.
 * If set, calls fadeOutSfxFast (stop). Otherwise calls fadeOutSfxSlow (start).
 *
 * @param a0 Object index.
 */
void func_800A2054(s32 a0) {
    u8 *base = (u8 *)D_80182E70;
    u8 *entry;

    entry = base + a0 * 12;
    if (entry[0] & 1) {
        fadeOutSfxFast();
    } else {
        fadeOutSfxSlow();
    }
}

/**
 * @brief Reset all 7 SFX entries and finalize.
 *
 * Calls fadeOutSfxFast for each of the 7 objects (indices 0-6),
 * then calls func_800A44BC to set D_801D49E2.
 */
void func_800A20B0(void) {
    s32 i = 0;
    do {
        fadeOutSfxFast(i);
        i++;
    } while (i < 7);
    func_800A44BC();
}

/**
 * @brief Wrapper that calls func_8002CE84, passing through all arguments.
 */
void func_800A20F4(void) {
    func_8002CE84();
}

INCLUDE_ASM("asm/ovl/tripletriad/nonmatchings/be_object4", func_800A2114);

/**
 * @brief Clear all 7 SFX entries by calling setSfxEntryParams with zero params.
 *
 * Iterates indices 0-6, calling setSfxEntryParams(i, 0, 0) for each.
 */
void clearAllSfx(void) {
    s32 i = 0;
    do {
        setSfxEntryParams(i, 0, 0);
        i++;
    } while (i < 7);
}

/**
 * @brief Clear D_801D4560 to zero.
 */
void func_800A2208(void) {
    D_801D4560 = 0;
}

INCLUDE_ASM("asm/ovl/tripletriad/nonmatchings/be_object4", func_800A2214);

/**
 * @brief Add an entry to the D_801D4500 effects queue if not full.
 *
 * The queue holds up to 8 entries (index 0..7), tracked by D_801D4560.
 * Each entry is 12 bytes: byte[0]=active, byte[1]=a1, byte[2]=a2,
 * byte[3]=a0, word[4]=a3.
 *
 * @param a0 Effect type (stored at byte 3).
 * @param a1 Parameter 1 (stored at byte 1).
 * @param a2 Parameter 2 (stored at byte 2).
 * @param a3 Parameter 3 (stored at word offset 4).
 */
void func_800A22E8(s32 a0, s32 a1, s32 a2, s32 a3) {
    s32 count = D_801D4560;
    if (count < 7) {
        u8 *entry;
        D_801D4560 = count + 1;
        entry = D_801D4500 + count * 12;
        entry[0] = 1;
        entry[3] = a0;
        entry[1] = a1;
        entry[2] = a2;
        *(s32 *)(entry + 4) = a3;
    }
}

/** @brief Call func_800A22E8 with a0, 0x80, 0x7F, 0. */
void func_800A233C(s32 a0) {
    func_800A22E8(a0, 0x80, 0x7F, 0);
}

/** @brief Call func_800A22E8 with a0, 0x80, 0x7F, a1. */
void func_800A2364(s32 a0, s32 a1) {
    func_800A22E8(a0, 0x80, 0x7F, a1);
}

INCLUDE_ASM("asm/ovl/tripletriad/nonmatchings/be_object4", func_800A238C);

/**
 * @brief Initialize animation handler and attach to D_801D3C58 list.
 *
 * Loads animation data from D_80182EC8 via sndProcessAudio, then
 * allocates a node in D_801D3C58 with func_800A238C as callback.
 * Clears the node's fields at offsets 0xC and 0xE.
 */
void func_800A247C(void) {
    u8 *node;
    sndProcessAudio(D_80182EC8, 0);
    node = (u8 *)func_8009E248((ObjNodeFn)func_800A238C);
    *(s16 *)(node + 0xC) = 0;
    *(s16 *)(node + 0xE) = 0;
}

INCLUDE_ASM("asm/ovl/tripletriad/nonmatchings/be_object4", func_800A24B4);

/**
 * @brief Initialize D_801D4568 and set flag bit 2 in g_tripleTriadInputFlags.
 *
 * Calls func_800A24B4 to initialize D_801D4568, then func_800A1D68
 * with mode 4. Finally sets bit 2 (0x4) in g_tripleTriadInputFlags.
 */
void func_800A26C8(void) {
    u8 *base = D_801D4568;
    func_800A24B4(base);
    func_800A1D68(4, base, 0);
    g_tripleTriadInputFlags |= TT_INPUT_DISABLED;
}

/**
 * @brief Close the in-match menu overlay and re-enable card input.
 *
 * Tears down the menu object @c D_801D4568 and clears @c TT_INPUT_DISABLED so
 * card input resumes. Counterpart to the menu-open handler @c func_800A26C8.
 */
void closeMenu(void) {
    func_800A2054(4);
    g_tripleTriadInputFlags &= ~TT_INPUT_DISABLED;
}

/**
 * @brief Add a rendering command entry based on the alternate screen index.
 *
 * Reads g_drawBufferIndex, XORs with 1 to get the alternate index, computes
 * an offset of index * 92 into g_drawEnvs, and calls queueLoadImage
 * with the resulting pointer and D_8012E66C.
 *
 * @return Always 0.
 */
s32 func_800A274C(void) {
    s32 idx = g_drawBufferIndex ^ 1;
    queueLoadImage(&g_drawEnvs[idx].clip, D_8012E66C);
    return 0;
}

INCLUDE_ASM("asm/ovl/tripletriad/nonmatchings/be_object4", func_800A279C);

/**
 * @brief Initialize the D_801D4968 linked list with two render callbacks.
 *
 * Sets up D_801D4968 as a linked list (node size 0x10, capacity 4),
 * then appends func_800A279C and func_800A274C as callbacks.
 * Clears bytes 0xC and 0xD on the first node.
 *
 * @return Pointer to D_801D4968 list header.
 */
u8 *initTripleTriadRenderList(void) {
    ObjList *list = D_801D4968;
    u8 *node;
    initObjList(list, D_801D4978, 0x10, 4);
    node = (u8 *)allocObjNode(list, (ObjNodeFn)func_800A279C);
    node[0xC] = 0;
    node[0xD] = 0;
    allocObjNode(list, (ObjNodeFn)func_800A274C);
    return list;
}

INCLUDE_ASM("asm/ovl/tripletriad/nonmatchings/be_object4", func_800A29D4);

INCLUDE_ASM("asm/ovl/tripletriad/nonmatchings/be_object4", func_800A2A8C);

/** @brief Held-button mask for pad @p a0 (buttons currently down). */
s32 getPadHeld(s32 a0) {
    return D_801D4B20[a0];
}

/** @brief Newly-pressed (rising-edge) button mask for pad @p a0. */
s32 getPadPressed(s32 a0) {
    return D_801D4B30[a0];
}

/** @brief Auto-repeat button mask for pad @p a0. */
s32 getPadRepeat(s32 a0) {
    return D_801D4B28[a0];
}

INCLUDE_ASM("asm/ovl/tripletriad/nonmatchings/be_object4", readPads);

INCLUDE_ASM("asm/ovl/tripletriad/nonmatchings/be_object4", func_800A2D34);

INCLUDE_ASM("asm/ovl/tripletriad/nonmatchings/be_object4", func_800A2E44);

/**
 * @brief Build the geometric falloff table @c D_801D49F8 (each step * 9/10).
 *
 * Fills the s16 entries @c D_801D49F8[0..64], writing index 64 first then
 * 63 down to 0. Index 64 starts at @c 0x1000; the running value is stored and
 * then scaled by @c 9/10 (a ~0.9 attenuation) for the next lower index — so the
 * table decays from @c 0x1000 at the top toward index 0.
 */
void func_800A2F78(void) {
    s16 *p;
    s32 v;
    s32 i;

    p = D_801D49F8;
    v = 0x1000;
    p += 64;
    *p = v;
    for (i = 0; i < 64; i++) {
        p--;
        *p = v;
        v = v * 9 / 10;
    }
}

INCLUDE_ASM("asm/ovl/tripletriad/nonmatchings/be_object4", func_800A2FCC);

INCLUDE_ASM("asm/ovl/tripletriad/nonmatchings/be_object4", func_800A30C8);

/**
 * @brief Wrapper for func_800A30C8 that appends flag=1 as the 7th argument.
 *
 * Passes through a0-a3 plus two stack arguments, appending 1 as
 * the final argument to form a 7-arg call to func_800A30C8.
 */
void func_800A31B8(s32 a0, s32 a1, s32 a2, s32 a3, s32 stack0, s32 stack1) {
    func_800A30C8(a0, a1, a2, a3, stack0, stack1, 1);
}

/**
 * @brief Build a draw-area primitive from the active draw environment and link it.
 *
 * Packs the @c SetDrawArea GP0 command for @c g_activeDrawEnv's clip rectangle
 * into the @c DR_AREA at @p prim, links @p prim into the ordering-table slot
 * @p ot via @c addPrimFast (hand-picked temp @c $s2), and returns the next
 * packet slot.
 *
 * @param ot   Ordering-table slot to link the primitive into.
 * @param prim Storage for the @c DR_AREA primitive.
 * @return Cursor for the next primitive (@c prim @c + @c 1).
 *
 * @note @c ++prim (advance-then-return), rather than @c prim @c + @c 1, is what
 *       matches: reassigning @c prim splits its live range so it drops its
 *       cross-call references and @c ot wins the lower saved register.
 */
DR_AREA *func_800A31EC(P_TAG *ot, DR_AREA *prim) {
    SetDrawArea((u8 *)prim, &g_activeDrawEnv->clip);
    addPrimFast(ot, prim, s2);
    return ++prim;
}

INCLUDE_ASM("asm/ovl/tripletriad/nonmatchings/be_object4", func_800A3248);

/**
 * @brief Temporarily adjust a rect's position/size, call func_800A3248, then restore.
 *
 * Saves the original 8 bytes of the rect at a2, increments x/y by 1,
 * decrements w/h by 2, calls func_800A3248(a0, a1), then restores
 * the original values.
 *
 * @param a0 First parameter passed to func_800A3248.
 * @param a1 Second parameter passed to func_800A3248.
 * @param a2 Pointer to rect structure (4 halfwords: x, y, w, h).
 */
void func_800A3320(s32 a0, s32 a1, u8 *a2) {
    u8 *base = a2;
    s32 saved0 = *(s32 *)(base + 0);
    s32 saved1 = *(s32 *)(base + 4);
    *(u16 *)(base + 0) = *(u16 *)(base + 0) + 1;
    *(u16 *)(base + 2) = *(u16 *)(base + 2) + 1;
    *(u16 *)(base + 4) = *(u16 *)(base + 4) - 2;
    *(u16 *)(base + 6) = *(u16 *)(base + 6) - 2;
    func_800A3248(a0, a1);
    *(s32 *)(base + 0) = saved0;
    *(s32 *)(base + 4) = saved1;
}

/**
 * @brief Scale a RECT about its center by a fixed-point factor, into @p out.
 *
 * @p scale is a signed fixed-point multiplier where @c 0x1000 (4096) represents
 * 1.0. When @c abs(scale) is exactly 1.0 the rectangle is copied verbatim.
 * Otherwise each axis is recentered: the half-extent is @c (scale * dimension) /
 * 8192 (unscale the Q12 product by 4096, then halve), the output origin is
 * @c center - halfExtent, and the output dimension is @c halfExtent * 2.
 *
 * Written in the in-place register-reuse style shared by the other FF8 RECT
 * helpers (cf. @c func_8002B080): each field is read once, then progressively
 * transformed in the same variable (x → centerX, w → halfWidth) so gcc 2.7.2
 * keeps it in one register rather than spilling copies.
 *
 * @param scale Fixed-point scale factor (@c 0x1000 == 1.0).
 * @param in    Source rectangle.
 * @param out   Destination rectangle.
 */
void func_800A3398(s32 scale, RECT *in, RECT *out) {
    if (abs(scale) == 0x1000) {
        *out = *in;
    } else {
        s32 x = in->x;
        s32 y = in->y;
        u32 w = in->w;
        u32 h = in->h;
        u32 prodW = scale * w;
        u32 prodH = scale * h;

        x = x + (w / 2);  /* x -> center x */
        w = prodW / 8192; /* w -> half the scaled width */
        out->x = x - w;
        out->w = w * 2;

        y = y + (h / 2);  /* y -> center y */
        h = prodH / 8192; /* h -> half the scaled height */
        out->y = y - h;
        out->h = h * 2;
    }
}

INCLUDE_ASM("asm/ovl/tripletriad/nonmatchings/be_object4", func_800A343C);

INCLUDE_ASM("asm/ovl/tripletriad/nonmatchings/be_object4", func_800A3528);

/**
 * @brief Step a wrapping cursor by the D-pad input, playing a click per move.
 *
 * @param pad   Controller button word: @c 0x4000 (down) advances the cursor,
 *              @c 0x1000 (up) retreats it. Each accepted press plays
 *              @c sendSpuCommand(1).
 * @param count Number of selectable entries (the cursor wraps over this range).
 * @param index Current cursor index.
 * @return The updated cursor index.
 *
 * @note @p pad must be a 16-bit type — with a 32-bit param gcc reads the saved
 *       copy for the first mask test instead of the incoming argument register.
 */
s32 func_800A3884(u16 pad, s32 count, s32 index) {
    if (pad & 0x4000) {
        sendSpuCommand(1);
        index++;
        if (index >= count) {
            index = 0;
        }
    }
    if (pad & 0x1000) {
        sendSpuCommand(1);
        index--;
        if (index < 0) {
            index = count - 1;
        }
    }
    return index;
}

/** @brief Per-frame cursor/timer state machine for the Triple Triad grid cursor.
 *
 * Bumps a frame counter and advances a fixed-point timer; the cursor only acts
 * on the frame the timer reaches 0x1000 (1.0). The grid is laid out in rows of
 * 11 cells and the 6-state machine drives it:
 *   - state 0: falls through to state 1.
 *   - state 1: idle. With at least 0xC cells, a 0x8000/0x2000 bit in @p flags0
 *     begins a vertical move (-> state 2/4); otherwise the column is stepped via
 *     func_800A3884 and the cursor position rewritten.
 *   - state 2/4: begin moving up/down one row (wrapping), seed the slide offset
 *     to -/+0xE67, and advance to state 3/5.
 *   - state 3/5: ease the slide offset by +/-0x199 until it reaches 0 (back to
 *     state 1); a 0x8000/0x2000 bit in @p flags1 re-triggers a move.
 *
 * @param flags0 Input flags sampled while idle (state 1).
 * @param flags1 Input flags sampled while sliding (state 3/5).
 * @return D_801D4A88[cursorPos] (with bit 0x100 set while sliding), -1 if the
 *         timer has not fired, or -2 if the cursor position is out of range.
 */
s32 func_800A390C(s32 flags0, s32 flags1) {
    CursorState *p = &D_801D49C8;
    u8 *statePtr;
    s16 acc;
    s16 pos;
    s32 result;

    p->frameCounter++;
    p->timer += p->timerStep;
    acc = (s16)p->timer;
    if (acc <= 0) {
        p->timer = 0;
        return -1;
    }
    if (acc < 0x1000) {
        return -1;
    }
    statePtr = &p->state;
    p->timer = 0x1000;

    switch (p->state) {
    case 0:
        *statePtr = 1;
        /* fall through */
    case 1:
        if (D_801D4AF6 >= 0xC) {
            if (flags0 & 0x8000) {
                *statePtr = 2;
                break;
            }
            if (flags0 & 0x2000) {
                *statePtr = 4;
                break;
            }
        }
        {
            s16 idx = (s16)p->cursorPos;
            s32 row = idx / 11;
            s32 col = func_800A3884(flags0 & 0xFFFF, 11, idx % 11);
            p->cursorPos = row * 11 + col;
        }
        break;
    case 2: {
        s16 idx;
        s32 row, col;
        sendSpuCommand(1);
        p->prevRow = p->row;
        idx = (s16)p->cursorPos;
        row = idx / 11;
        row = row - 1;
        col = idx % 11;
        if (row < 0) {
            row = (D_801D4AF6 + 0xA) / 11 - 1;
        }
        p->cursorPos = row * 11 + col;
        p->row = row;
        p->slideOffset = -0xE67;
        *statePtr = 3;
        break;
    }
    case 3:
        p->slideOffset += 0x199;
        if ((s16)p->slideOffset >= 0) {
            p->slideOffset = 0;
            *statePtr = 1;
        }
        if (flags1 & 0x8000) {
            *statePtr = 2;
        }
        if (flags1 & 0x2000) {
            *statePtr = 4;
        }
        break;
    case 4: {
        s16 idx;
        s32 row, col;
        sendSpuCommand(1);
        p->prevRow = p->row;
        idx = (s16)p->cursorPos;
        row = idx / 11;
        row = row + 1;
        col = idx % 11;
        if (row >= (D_801D4AF6 + 0xA) / 11) {
            row = 0;
        }
        p->cursorPos = row * 11 + col;
        p->row = row;
        p->slideOffset = 0xE67;
        *statePtr = 5;
        break;
    }
    case 5:
        p->slideOffset -= 0x199;
        if ((s16)p->slideOffset <= 0) {
            p->slideOffset = 0;
            *statePtr = 1;
        }
        if (flags1 & 0x8000) {
            *statePtr = 2;
        }
        if (flags1 & 0x2000) {
            *statePtr = 4;
        }
        break;
    }

    pos = p->cursorPos;
    if (pos < D_801D4AF6) {
        result = D_801D4A88[pos];
        if (p->slideOffset != 0) {
            result |= 0x100;
        }
        return result;
    }
    return -2;
}

/**
 * @brief Build a 12x12 font-glyph sprite and prepend it to the ordering table.
 *
 * Fills a free-size @c SPRT (code 0x64, carried in @c g_menuColor) for tile
 * @p tileIdx of a 21-tile-per-row font texture: CLUT from @p palArg's low 3 bits,
 * menu color chosen by its high bits, 12x12 size, position @p xy, and UV from the
 * tile's column/row. Links the primitive at the head of the OT carried in @p ot
 * (addr<<8 form) via @c swl and returns the new head.
 *
 * Hand-optimized in the FF8 sprite/RECT style (cf. @c func_8002B080): batched
 * 32-bit stores, and in-place register reuse (the head register becomes the
 * palette-page test; the palette register becomes the color) to steer gcc 2.7.2.
 *
 * @param ot      Current OT head (packed @c addr<<8).
 * @param prim    Sprite primitive to fill.
 * @param tileIdx Glyph/tile index into the 21-per-row font texture.
 * @param palArg  Bits 2:0 = CLUT palette; bits 7:3 select the menu color.
 * @param xy      Packed screen position (x | y<<16).
 * @return The new OT head (@c prim<<8).
 */
u32 func_800A3C7C(u32 ot, SPRT *prim, s32 tileIdx, s32 palArg, u32 xy) {
    u32 head;
    s32 quo;
    s32 rem;

    ((u8 *)&prim->tag)[3] = 4; /* SPRT length = 4 words */
    __asm__ __volatile__("" : : : "memory"); /* keep the head shift after the len store */
    head = (u32)prim << 8;     /* new OT head */
    __asm__ __volatile__("swl %0, 2(%1)" : : "r"(ot), "r"(prim) : "memory"); /* link */
    ot = head;

    head = (u32)palArg >> 3;   /* head register reused: now the palette page */
    palArg = palArg & 7;
    prim->clut = (palArg << 6) + 0x3812; /* getClut(288, 224 + palette) */
    if (head) {
        palArg = g_menuColor[1]; /* palette register reused: now the color */
    } else {
        palArg = g_menuColor[0];
    }

    *(u32 *)&prim->w = 0xC000C; /* 12 x 12 */
    *(u32 *)&prim->r0 = palArg; /* r, g, b, code (0x64) */
    *(u32 *)&prim->x0 = xy;

    quo = tileIdx / 21;
    rem = tileIdx % 21;
    *(u16 *)&prim->u0 = (rem | (quo << 8)) * 12; /* tile UV, 21/row, 12px cells */

    return ot;
}

INCLUDE_ASM("asm/ovl/tripletriad/nonmatchings/be_object4", func_800A3D2C);

INCLUDE_ASM("asm/ovl/tripletriad/nonmatchings/be_object4", func_800A3EE0);

/**
 * @brief Wrapper for func_800A3EE0 that selects a lookup table entry based on the 5th argument.
 *
 * If stack0 >= 8, uses g_menuColor[1] and subtracts 8 from stack0.
 * Otherwise uses g_menuColor[0] with stack0 unchanged.
 * Passes the lookup value and adjusted stack0 as extra args to func_800A3EE0.
 *
 * @param a0-a3 Parameters passed through to func_800A3EE0.
 * @param stack0 Index parameter; if >= 8, adjusted by -8 and table index 1 is used.
 */
void func_800A4098(s32 a0, s32 a1, s32 a2, s32 a3, s32 stack0) {
    s32 idx;
    if (stack0 >= 8) {
        stack0 -= 8;
        idx = 1;
    } else {
        idx = 0;
    }
    func_800A3EE0(a0, a1, a2, a3, g_menuColor[idx], stack0);
}

INCLUDE_ASM("asm/ovl/tripletriad/nonmatchings/be_object4", func_800A40F0);

/** @brief Render context for @c func_800A4250 (fields named by offset — role inferred). */
typedef struct {
    /* 0x00 */ s16 unk00;     /**< Forwarded (−0x13) as @c func_8002FF34's @c yPos. */
    /* 0x02 */ s16 unk02;     /**< Base for the @c w arg (+0xB, then +column*13). */
    /* 0x04 */ u8  pad04[0xC];
    /* 0x10 */ s32 unk10;     /**< Forwarded as @c func_8002FF34's @c col. */
} func_800A4250_arg2;

/**
 * @brief Emit one glyph into the OT at a grid cell derived from a linear index.
 *
 * Forwards to the glyph emitter @c func_8002FF34 (glyph 0) with the position
 * taken from @p ctx and the column @c a3 @c % @c 11 at a 13px pitch. Returns
 * the advanced packet cursor.
 *
 * @note @c w (@c ctx->unk02 @c + @c 0xB) must be a separate local — inlining it
 *       lets gcc reassociate the @c +0xB into the @c *13 term and reorder the
 *       adds away from the target.
 */
void *func_800A4250(s32 *otBase, void *pkt, func_800A4250_arg2 *ctx, s32 a3) {
    s32 w = ctx->unk02 + 0xB;
    return func_8002FF34(otBase, pkt, 0,
                         ctx->unk00 - 0x13,
                         w + (a3 % 11) * 13,
                         ctx->unk10);
}

INCLUDE_ASM("asm/ovl/tripletriad/nonmatchings/be_object4", func_800A42D0);

/**
 * @brief Apply func_800A42D0 with VSync lock/unlock.
 *
 * @param a0 Parameter passed through to func_800A42D0.
 * @return Result of storeGpuPacket (VSync unlock).
 */
s32 func_800A443C(s32 a0) {
    s32 saved = a0;
    s32 lock = getDisplayListHead();
    s32 result = func_800A42D0(saved, lock);
    return storeGpuPacket(result);
}

/**
 * @brief Compute and store a packed color/brightness value for the battle camera.
 *
 * Stores @p a0 to @ref CursorState::brightness, then computes a packed 32-bit
 * value by dividing a0 by 32 (rounding toward zero) and replicating the result
 * into bytes 0-2 with byte 3 set to 0x64, storing it to
 * @ref CursorState::packedColor.
 *
 * @param a0 Brightness value (scaled by 32).
 */
void func_800A4478(s32 a0) {
    s32 val;
    D_801D49C8.brightness = a0;
    if (a0 < 0) {
        a0 += 0x1F;
    }
    val = a0 >> 5;
    {
        s32 hi = (val << 8) | (val << 16);
        D_801D49C8.packedColor = (val | hi) | 0x64000000;
    }
}

/**
 * @brief Store a byte value to D_801D49EC.
 *
 * @param a0 Byte value to store.
 */
void func_800A44B0(s32 a0) {
    D_801D49EC = a0;
}

/**
 * @brief Set D_801D49E2 to -256 (0xFF00).
 */
void func_800A44BC(void) {
    D_801D49E2 = -256;
}

/**
 * @brief Reset D_801D49C8 fields and call func_800A2E44.
 *
 * Sets @ref CursorState::timerStep to 0x100 and clears @ref CursorState::state,
 * @ref CursorState::cursorPos and @ref CursorState::row, then calls func_800A2E44
 * for further reset.
 */
void func_800A44CC(void) {
    D_801D49C8.timerStep = 0x100;
    D_801D49C8.state = 0;
    D_801D49C8.cursorPos = 0;
    D_801D49C8.row = 0;
    func_800A2E44();
}

/**
 * @brief Initialize D_801D49C8 battle camera structure.
 *
 * Sets position, dimensions, mode fields, clears various flags,
 * then calls func_800A4478 and func_800A2F78 for further init.
 * Also initializes D_801D4B18 to 0 and D_801D4B1A to 0x180.
 *
 * @param a0 View X position.
 * @param a1 View Y position.
 */
void func_800A4504(s32 a0, s32 a1) {
    D_801D49C8.x = a0;
    D_801D49C8.w = 0xA1;
    D_801D49C8.h = 0x9F;
    D_801D49C8.unk21 = 0xB;
    D_801D49C8.y = a1;
    D_801D49C8.timerStep = 0;
    D_801D49C8.timer = 0;
    D_801D49C8.cursorPos = 0;
    D_801D49C8.slideOffset = 0;
    D_801D49C8.unk20 = 0;
    D_801D49C8.row = 0;
    D_801D49C8.prevRow = 0;
    D_801D49C8.frameCounter = 0;
    D_801D49C8.unk24 = 1;
    func_800A4478(0x1000);
    func_800A2F78();
    D_801D4B18 = 0;
    D_801D4B1A = 0x180;
}

