#include "common.h"
#include "battle.h"
#include "tripletriad.h"
#include "psxsdk/libc.h"
#include "psxsdk/libgpu.h"
#include "tripletriad/be_object1.h"
#include "gamestate.h"


/**
 * @brief Initialize the Triple Triad engine: run the subsystem initializers,
 *        reset match state, and build the object-type table.
 */
void initTripleTriad(void) {
    s32 i;

    func_800A2D34();
    func_800981BC();
    func_80098B70();
    initTriadTaskPool();
    func_80098B68();
    func_800984DC();
    resetTriadMenuState();
    func_800A1BE0();
    func_800A2208();
    func_80098DD4();
    func_8009EB98();
    func_80098A6C(D_800B7638);
    func_80098A6C(D_800A45B8);

    g_tripleTriadInputFlags = 0;
    g_tripleTriadFrameCount = 0;
    g_tripleTriadState = 1;

    for (i = 0; i < 110; i++) {
        g_tripleTriadCardCounts[i] = func_80023B14(i);
    }
}

/**
 * @brief Battle engine main loop — runs until the state machine sentinel (6) is reached.
 *
 * After initializing all subsystems via @c initTripleTriad, repeatedly:
 *  - If the disable-input flag is set in @c g_tripleTriadInputFlags (bit 2) and bits 4 or 5
 *    of @c D_801C2EC0[2] are set, calls @c func_800A271C and clears the
 *    3-element controller-input mask arrays (@c D_801C2EC0/EB8/EC8).
 *  - Dispatches via @c D_800A4588 (function-pointer table indexed 1..5 by
 *    @c g_tripleTriadState) until the state returns non-zero (storing it in
 *    @c D_801A2C40 and clearing state) or state becomes 6 (exit).
 *  - Drains the active list at @c D_801A2C40 via @c func_80098D28, then runs
 *    per-frame sub-tick callbacks and increments the frame counter @c g_tripleTriadFrameCount.
 *
 * After the main loop, runs two frame-finalize iterations and stores 100 to
 * @c g_vsyncRate (D_8005F158) before returning.
 */
s32 func_80098304(void) {
    s32 i;

    initTripleTriad();

    do {
        if (g_tripleTriadInputFlags & 0x4) {
            if (D_801C2EC0[2] & 0x30) {
                func_800A271C();
            }
            for (i = 0; i < 3; i++) {
                D_801C2EC0[i] = 0;
                D_801C2EC8[i] = D_801C2EB8[i] = 0;
            }
        }

        while (g_tripleTriadState != 0 && g_tripleTriadState != 6) {
            D_801A2C40 = D_800A4588[g_tripleTriadState]();
            if (D_801A2C40 != 0) {
                g_tripleTriadState = 0;
            }
        }

        if (D_801A2C40 != 0) func_80098D28(D_801A2C40);
        processTriadTasks();
        func_8009EBCC();
        updateTriadMenu();
        func_800A1C6C();
        func_80098690();
        func_80098828();
        func_800A2214();
        g_tripleTriadFrameCount++;
    } while (g_tripleTriadState != 6);

    D_801C2DC9 = -1;
    for (i = 0; i < 2; i++) {
        func_800A1C6C();
        func_80098690();
    }
    func_800A21C4();
    D_8005F158 = 100;
    return 0;
}

/**
 * @brief Initialize the battle engine's GPU and GTE subsystems.
 *
 * Sets up double-buffered draw/display environments for a 384x224 screen
 * with the visible area at 256x224 offset by (0, 8), clears the two
 * framebuffer regions to black plus a 256x256 texture region to white,
 * primes the reverse-order primitive table and pool tail, then initializes
 * the GTE with screen offset (192, 112), projection distance 512, and
 * masks the display while finishing setup.
 */
void func_800984DC(void) {
    DISPENV *disp;
    RECT *screen0;
    RECT *screen1;

    SetDefDrawEnv(&D_801C2DD0[0], 0, 0, 0x180, 0xE0);
    disp = D_801C2E88;
    SetDefDispEnv(&disp[0], 0x180, 0, 0x180, 0xE0);
    screen0 = &disp[0].screen;
    screen0->x = 0;
    screen0->y = 8;
    screen0->w = 0x100;
    screen0->h = 0xE0;

    SetDefDrawEnv(&D_801C2DD0[1], 0x180, 0, 0x180, 0xE0);
    SetDefDispEnv(&disp[1], 0, 0, 0x180, 0xE0);
    disp[1].screen.x = 0;
    screen1 = &disp[1].screen;
    screen1->y = 8;
    screen1->w = 0x100;
    screen1->h = 0xE0;

    ClearImage(&D_800A45A8, 0, 0, 0);
    D_800A45A8.x = 0x180;
    D_800A45A8.y = 0;
    ClearImage(&D_800A45A8, 0, 0, 0);

    ClearImage(&D_800A45B0, 0xFF, 0xFF, 0xFF);

    D_801C2DCA = 0;
    D_801C2EB0 = &D_801A2CE8[0][0];
    ClearOTagR(&D_801A2CE8[0][0], BE_OT_LEN);
    D_801C2EB4 = &D_801A2DC8[D_801C2DCA][0];

    InitGeom();
    SetGeomOffset(0xC0, 0x70);
    SetGeomScreen(0x200);
    SetDispMask(0);

    D_801C2DC9 = 2;
    D_801C2DC8 = 0;
    func_800988D4();
}

/**
 * @brief Flip the battle engine's double-buffered draw/display environments.
 *
 * Called once per frame (paired with @c func_80099464). Synchronizes with
 * vsync and the previous draw, advances the fade-in/fade-out counter
 * @c D_801C2DC9 (decrements positives, increments negatives; on reaching
 * zero, toggles @c SetDispMask), then flips @c D_801C2DCA to the next
 * buffer index. The just-flipped buffer's draw and disp envs are pushed
 * to the GPU, the previous buffer's OT is drawn, and the new buffer's
 * OT base + primitive pool tail are reset so the next frame's primitives
 * accumulate into the fresh buffer. @c g_activeDrawEnv is updated to
 * point at the buffer being prepared (the one NOT just put on display).
 */
void func_80098690(void) {
    func_80099464();
    VSync(1);
    DrawSync(0);
    VSync(1);
    VSync(D_801C2DC8);

    if (D_801C2DC9 > 0) {
        D_801C2DC9--;
        if (D_801C2DC9 == 0) {
            SetDispMask(1);
        }
    } else if (D_801C2DC9 < 0) {
        D_801C2DC9++;
        if (D_801C2DC9 == 0) {
            SetDispMask(0);
        }
    }

    D_801C2DCA ^= 1;

    PutDrawEnv(&D_801C2DD0[D_801C2DCA]);
    PutDispEnv(&D_801C2E88[D_801C2DCA]);
    func_800988E0();

    DrawOTag(&D_801A2CE8[D_801C2DCA ^ 1][BE_OT_LEN - 1]);
    D_801C2EB4 = &D_801A2DC8[D_801C2DCA][0];
    D_801C2EB0 = &D_801A2CE8[D_801C2DCA][0];
    ClearOTagR(&D_801A2CE8[D_801C2DCA][0], BE_OT_LEN);

    g_activeDrawEnv = &D_801C2DD0[D_801C2DCA ^ 1];
}

/**
 * @brief Initialize controller input arrays for battle engine.
 *
 * Reads current controller state for three input types (digital, analog X, analog Y)
 * and fills their respective 3-element history arrays with the initial values.
 */
void func_80098828(void) {
    s32 i;

    func_800A2BD8();
    D_801C2EC8[0] = func_800A2B84(0);
    D_801C2EC0[0] = func_800A2BA0(0);
    D_801C2EB8[0] = func_800A2BBC(0);

    for (i = 1; i < 3; i++) {
        D_801C2EC8[i] = D_801C2EC8[0];
        D_801C2EC0[i] = D_801C2EC0[0];
        D_801C2EB8[i] = D_801C2EB8[0];
    }
}

/**
 * @brief Clear D_801C2FD0 to zero.
 */
void func_800988D4(void) {
    D_801C2FD0 = 0;
}

/**
 * @brief Dispatch all pending entries in the @c D_801C2ED0 GPU-transfer pool.
 *
 * Walks @c D_801C2FD0 entries and performs the requested operation on each
 * (LoadImage / LoadImage TIM / StoreImage / MoveImage), then resets the pool
 * count to zero. Entries are typically queued by @c func_800988E0's siblings
 * during a frame and flushed here once vsync has elapsed.
 */
void func_800988E0(void) {
    s32 i;
    PoolEntry *p = D_801C2ED0;

    for (i = 0; i < D_801C2FD0; p++, i++) {
        switch (p->active) {
            case POOL_LOAD_IMAGE:
                LoadImage(&p->rect, p->src);
                break;
            case POOL_LOAD_TIM: {
                Tim *tim = p->src;
                TimSection *image;
                LoadImage(&tim->clut.rect, tim->clut.data);
                /* Re-derive image block from p->src (not cached @c tim) so gcc
                   reloads the pointer — matches the original instruction order. */
                image = (TimSection *)((u8 *)&((Tim *)p->src)->clut + tim->clut.len);
                LoadImage(&image->rect, image->data);
                break;
            }
            case POOL_STORE_IMAGE:
                StoreImage(&p->rect, p->src);
                break;
            case POOL_MOVE_IMAGE:
                MoveImage(&p->rect, ((u32)p->src) & 0xFFFF, ((u32)p->src) >> 16);
                break;
        }
    }
    D_801C2FD0 = 0;
}

/**
 * @brief Register an inactive D_801C2ED0 pool entry with an 8-byte data copy.
 *
 * Takes the next slot in the D_801C2ED0 pool (indexed by D_801C2FD0),
 * marks it inactive, copies 8 bytes from @p a0 into the slot's data
 * region (offsets 4..0xB, via unaligned-safe copy), and stores @p a1
 * as the callback at offset 0xC.
 *
 * @param a0 Source of 8 bytes to copy (may be unaligned).
 * @param a1 Callback pointer stored at offset 0xC of the slot.
 */
void func_80098A1C(u8 *a0, u8 *a1) {
    PoolEntry *entry = &D_801C2ED0[D_801C2FD0++];
    entry->active = 0;
    memcpy(&entry->rect, a0, 8);
    entry->src = a1;
}

/**
 * @brief Register a callback into D_801C2ED0 and advance into @p a0.
 *
 * Takes the next free slot in D_801C2ED0 (indexed by the counter at
 * D_801C2FD0), marks it active, stores @p a0 as the callback pointer,
 * then follows a two-step offset chain from @p a0:
 *   1. Read an @c s32 offset at @c a0+8; advance @c a0 by 8 plus that offset.
 *   2. Read an @c s32 offset at the new @c a0; return (@c a0 + that offset).
 *
 * @param a0 Pointer to a header whose byte-offset fields chain into another
 *           location in the same record.
 * @return The final address resulting from the two-step offset chain.
 */
u8 *func_80098A6C(ResHeader *res) {
    PoolEntry *entry;
    s32 *rel;

    entry = &D_801C2ED0[D_801C2FD0++];
    entry->active = 1;
    entry->src = res;

    rel = &res->offset;
    rel = (s32 *)((u8 *)rel + *rel);
    return (u8 *)rel + *rel;
}

/**
 * @brief Register a D_801C2ED0 slot with @c active=2 and an 8-byte data copy.
 *
 * Variant of @c func_80098A1C: takes the next pool slot, marks it with flag
 * @c 2 (vs @c 0 in @c func_80098A1C), copies 8 bytes from @p a0 into the
 * data region (offsets 4..0xB, unaligned-safe), and stores @p a1 as the
 * callback at offset 0xC.
 */
void func_80098AB4(u8 *a0, u8 *a1) {
    PoolEntry *entry = &D_801C2ED0[D_801C2FD0++];
    entry->active = 2;
    memcpy(&entry->rect, a0, 8);
    entry->src = a1;
}

/**
 * @brief Register a D_801C2ED0 slot with @c active=3, data copy, and a
 *        packed 16-bit pair at offset 0xC.
 *
 * Same pool-registration pattern as @c func_80098A1C /
 * @c func_80098AB4, but stores a packed 32-bit value at offset 0xC
 * formed from two 16-bit args (@p a2 as the upper 16 bits, sign-extended
 * @p a1 as the lower 16 bits).
 */
void func_80098B08(u8 *a0, s16 a1, u16 a2) {
    PoolEntry *entry = &D_801C2ED0[D_801C2FD0++];
    entry->active = 3;
    memcpy(&entry->rect, a0, 8);
    entry->src = (void *)(((s32)a2 << 16) | a1);
}

void func_80098B68(void) {
}

/**
 * @brief Set D_801C2FD8 to 0x1F800000 (scratchpad RAM base address).
 */
void func_80098B70(void) {
    D_801C2FD8 = 0x1F800000;
}

/**
 * @brief Allocate aligned memory from the scratchpad heap and return the new pointer.
 *
 * Rounds @p a0 up to the next multiple of 4, then advances D_801C2FD8
 * by that amount. This is the counterpart of func_80098BA0 (which subtracts).
 *
 * @param a0 Size to allocate (will be aligned up to 4).
 * @return The previous value of D_801C2FD8 (start of allocated block).
 */
s32 func_80098B80(s32 a0) {
    s32 old = D_801C2FD8;
    D_801C2FD8 = old + ((a0 + 3) & ~3);
    return old;
}

/**
 * @brief Align a size up to 4 bytes and subtract from the allocation pointer.
 *
 * Rounds a0 up to the next multiple of 4 and decrements D_801C2FD8 by that amount.
 *
 * @param a0 Size to allocate (will be aligned up to 4).
 */
void func_80098BA0(s32 a0) {
    D_801C2FD8 -= (a0 + 3) & ~3;
}

/**
 * @brief Initialize a linked list header and clear all node slots.
 *
 * @param a0 Pointer to the list header (head, tail, pool ptr, size, count).
 * @param a1 Pointer to the node pool.
 * @param a2 Size of each node in bytes.
 * @param a3 Number of nodes in the pool.
 */
void func_80098BC0(u8 *a0, u8 *a1, s32 a2, s32 a3) {
    s32 i = 0;
    *(s32 *)(a0 + 0) = 0;
    *(s32 *)(a0 + 4) = 0;
    *(s32 *)(a0 + 8) = (s32)a1;
    *(s16 *)(a0 + 0xC) = a2;
    *(s16 *)(a0 + 0xE) = a3;
    if (a3 > 0) {
        do {
            *(s16 *)a1 = 0;
            i++;
            a1 += a2;
        } while (i < a3);
    }
}

/**
 * @brief Find and return the first free node in the pool.
 *
 * Scans the node pool referenced by the list header for a node whose
 * flags bit 0 is clear (inactive). Returns the first free node found,
 * or NULL if all nodes are in use.
 *
 * @param a0 Pointer to list header (+0x8=pool, +0xC=stride, +0xE=count).
 * @return Pointer to the first free node, or NULL if none available.
 */
void *func_80098BF8(u8 *a0) {
    s32 count = *(s16 *)(a0 + 0xE);
    u8 *pool = (u8 *)*(s32 *)(a0 + 0x8);
    s32 i = 0;
    if (count > 0) {
        s32 n = count;
        do {
            if (!(*(u16 *)pool & 1)) {
                return pool;
            }
            i++;
            pool += *(s16 *)(a0 + 0xC);
        } while (i < n);
    }
    return 0;
}

/**
 * @brief Allocate a node and append it to a doubly-tracked linked list.
 *
 * Calls func_80098BF8 to allocate a node. If successful, initializes
 * flags (sets bit 0), clears fields at +2 and +4, stores @p a1 at +8,
 * and appends the node: if the list is non-empty, links old tail to
 * new node; otherwise sets head. Always updates tail pointer.
 *
 * @param a0 Pointer to list header (head at +0, tail at +4).
 * @param a1 Value to store at offset +8 of the new node (callback pointer).
 * @return Pointer to the new node, or NULL if allocation failed.
 */
void *func_80098C44(u8 *a0, s32 a1) {
    u8 *node = (u8 *)func_80098BF8(a0);
    if (node != 0) {
        s32 tail;
        *(u16 *)(node + 0) |= 1;
        *(u16 *)(node + 2) = 0;
        *(s32 *)(node + 4) = 0;
        *(s32 *)(node + 8) = a1;
        tail = *(s32 *)(a0 + 4);
        if (tail != 0) {
            *(s32 *)(tail + 4) = (s32)node;
        } else {
            *(s32 *)a0 = (s32)node;
        }
        *(s32 *)(a0 + 4) = (s32)node;
    }
    return node;
}

/**
 * @brief Allocate and insert a node into a linked list.
 *
 * Calls func_80098BF8 to allocate a node. If allocation succeeds,
 * initializes flags (sets bit 0), clears field at +2, stores @p a1
 * at offset +8, and prepends the node to the list headed at @p a0.
 *
 * @param a0 Pointer to the list head pointer.
 * @param a1 Value to store at offset +8 of the new node (callback pointer).
 * @return Pointer to the new node, or NULL if allocation failed.
 */
void *func_80098CC0(u8 *a0, s32 a1) {
    u8 *node = (u8 *)func_80098BF8(a0);
    if (node != 0) {
        *(u16 *)(node + 0) |= 1;
        *(u16 *)(node + 2) = 0;
        *(s32 *)(node + 8) = a1;
        *(s32 *)(node + 4) = *(s32 *)a0;
        *(s32 *)a0 = (s32)node;
    }
    return node;
}

/**
 * @brief Iterate a linked list, calling each node's callback and removing finished nodes.
 *
 * Walks through the linked list at @p a0, calling the function pointer at
 * offset +8 of each node with the node as the argument. If the callback
 * returns a value with bit 1 set, the node is removed from the list
 * (flags cleared, unlinked). Otherwise the node is kept and the count
 * incremented. Updates the list tail pointer when done.
 *
 * @param a0 Pointer to the list header (head at +0, tail at +4).
 * @return Number of nodes remaining in the list.
 */
s32 func_80098D28(u8 *a0) {
    u8 *prev = 0;
    s32 count = 0;
    u8 *node = (u8 *)*(s32 *)(a0 + 0);
    s32 result;

    while (node != 0) {
        result = ((s32 (*)(u8 *))*(s32 *)(node + 8))(node);
        if (result & 2) {
            *(s16 *)(node + 0) = 0;
            if (prev != 0) {
                *(s32 *)(prev + 4) = *(s32 *)(node + 4);
            } else {
                *(s32 *)(a0 + 0) = *(s32 *)(node + 4);
            }
        } else {
            prev = node;
            count++;
        }
        node = (u8 *)*(s32 *)(node + 4);
    }
    *(s32 *)(a0 + 4) = (s32)prev;
    return count;
}

/**
 * @brief Register @c D_800B71D8 and set up the current frame's draw targets.
 *
 * Registers the @c D_800B71D8 resource into the D_801C2ED0 pool via
 * @c func_80098A6C, kicks off @c ClearOTag for the current buffer's
 * drawenv slot, then rebases the framebuffer (@c D_801D2FE0) and drawenv
 * (@c D_801D3000) pointers to the slot indexed by @c D_80182B54. Unlike
 * @c func_80099464, this does not flip the buffer index first.
 */
void func_80098DD4(void) {
    func_80098A6C(&D_800B71D8);
    ClearOTag(D_801D2FF0[D_80182B54], 2);
    D_801D2FE0 = D_801C2FE0[D_80182B54];
    D_801D3000 = D_801D2FF0[D_80182B54];
}

/**
 * @brief Set battle viewport dimensions if non-negative.
 *
 * Stores a0 to D_80182B5A and g_textCursorX if a0 >= 0.
 * Stores a1 to D_80182B58 if a1 >= 0.
 *
 * @param a0 Width value (stored if >= 0).
 * @param a1 Height value (stored if >= 0).
 */
void func_80098E54(s32 a0, s32 a1) {
    if (a0 >= 0) {
        D_80182B5A = a0;
        g_textCursorX = a0;
    }
    if (a1 >= 0) {
        D_80182B58 = a1;
    }
}

/**
 * @brief Append one debug-text character as a 5x5 textured sprite primitive.
 *
 * Renders @p ch (uppercased on the fly) from the debug font atlas into a
 * fresh @c SPRT at the current packet-buffer tail (@c D_801D2FE0), then
 * advances the cursor and the buffer pointer. Newline resets the cursor
 * to the line origin and steps down 7 px; any non-printable byte (below
 * space) just advances the cursor without emitting a glyph.
 *
 * The U/V atlas math packs 8 glyphs per row across 64 px; biases by
 * @c -0x80 / @c -0x20 wrap the indices into the texture-page coordinate
 * space. Odd/even character codes alternate between two CLUTs.
 */
void func_80098E7C(u8 ch) {
    SPRT *prim;
    s32 adjusted;
    s32 row;

    prim = (SPRT *)D_801D2FE0;

    if (ch == '\n') {
        g_textCursorX = D_80182B5A;
        D_80182B58 += 7;
        return;
    }

    /* Any space or control byte: just advance the cursor, no glyph. */
    if (ch <= 0x20) {
        g_textCursorX += 6;
        return;
    }

    /* Lowercase 'a'..'z' (0x61..0x7A): fold to uppercase by subtracting 0x20. */
    if ((u8)(ch - 0x61) < 0x1A) {
        ch -= 0x20;
    }

    SetSprt(prim);

    /* Drop down to glyph index relative to space (0x20 = first printable). */
    adjusted = ch - 0x20;

    prim->r0 = D_80182B5C.r;
    prim->g0 = D_80182B5C.g;
    prim->b0 = D_80182B5C.b;

    prim->x0 = g_textCursorX;
    prim->y0 = D_80182B58;

    prim->u0 = ((adjusted << 2) & 0x38) - 0x80;

    row = adjusted;
    if (adjusted < 0) {
        row = ch - 0x11;
    }
    prim->v0 = ((row >> 4) << 3) - 0x20;

    if (ch & 1) {
        prim->clut = 0x384F;
    } else {
        prim->clut = 0x380F;
    }

    prim->h = 5;
    prim->w = 5;

    AddPrim((u32 *)D_801D3000, prim);

    D_801D2FE0 = (u8 *)(prim + 1);
    g_textCursorX += 6;
}

/**
 * @brief Render a null-terminated text string, with @c '#<digit>' color escapes.
 *
 * Walks @p str byte by byte. Characters are passed to @c func_80098E7C for
 * sprite rendering. A @c '#' begins an escape: if the next byte is an ASCII
 * digit @c '0'..'8', it indexes the palette table @c D_80182AA0 and the
 * matching 4-byte RGB entry is copied into the live @c D_80182B5C color;
 * any other byte after @c '#' is rendered as a plain character (the @c '#'
 * is consumed silently).
 *
 * @note The @c escape_marker variable holds @c '#' across iterations to
 * anchor the constant in a callee-saved register (matches target codegen).
 */
void func_80098FD8(u8 *str) {
    u8 ch;

    for (ch = *str++; ch != 0; ch = *str++) {
        s32 digitEscape = '#';
        if (ch == digitEscape) {
            s32 next = *str++;
            s32 digit = next & 0xFF;
            if (digit < '9') {
                if (digit >= '0') {
                    /* Set color from palette entry indexed by ASCII digit. */
                    memcpy(&D_80182B5C, &D_80182AA0[digit], 4);
                } else {
                    func_80098E7C(next);
                }
            } else {
                func_80098E7C(next);
            }
        } else {
            func_80098E7C(ch);
        }
    }
}

/**
 * Converts an integer to a decimal string representation.
 *
 * @param a0 The integer value to convert.
 * @param a1 Pointer to the output buffer.
 * @return Pointer to the end of the written string.
 */
u8 *func_800990A0(s32 a0, u8 *a1) {
    u8 buf[36];
    u8 *dst = a1;
    u8 *p;

    if (a0 < 0) {
        *dst = 0x2D;
        dst++;
        a0 = -a0;
    }

    p = buf + 33;
    buf[33] = 0;

    do {
        p--;
        *p = (a0 % 10) + 0x30;
        a0 = a0 / 10;
    } while (a0 != 0);

    func_80047CA4(dst, p);
    return dst + func_80047CB4(dst);
}

/**
 * Converts an integer to a hexadecimal string representation.
 *
 * @param a0 The integer value to convert.
 * @param a1 Pointer to the output buffer.
 * @return Pointer to the end of the written string.
 */
u8 *func_80099134(s32 a0, u8 *a1) {
    u8 buf[20];
    u8 *dst = a1;
    u8 *p;
    u8 *table;

    if (a0 < 0) {
        *dst = 0x2D;
        dst++;
        a0 = -a0;
    }

    p = buf + 17;
    buf[17] = 0;
    table = D_80182B84;

    do {
        p--;
        *p = *(u8 *)((a0 & 0xF) + (s32)table);
        a0 >>= 4;
    } while (a0 != 0);

    func_80047CA4(dst, p);
    return dst + func_80047CB4(dst);
}

/**
 * @brief Convert an integer to a binary string representation.
 *
 * Writes the binary digits of @p a0 into the buffer at @p a1,
 * then returns a pointer to the end of the written string.
 *
 * @param a0 The integer value to convert.
 * @param a1 Pointer to the output buffer.
 * @return Pointer to the end of the written string.
 */
u8 *func_800991AC(s32 a0, u8 *a1) {
    u8 buf[36];
    u8 *dst = a1;
    u8 *p;

    p = buf + 33;
    buf[33] = 0;

    do {
        p--;
        *p = (a0 & 1) + 0x30;
        a0 >>= 1;
    } while (a0 != 0);

    func_80047CA4(dst, p);
    return dst + func_80047CB4(dst);
}

/**
 * @brief printf-style format-into-buffer.
 *
 * Walks the format string given as @c args[0] and writes the formatted
 * output into @p dst. The remaining @c args[1..] are the values referenced
 * by format specifiers. Supported specifiers:
 *
 *   - @c %d, @c %D : signed decimal (via @c func_800990A0)
 *   - @c %x, @c %X : hexadecimal    (via @c func_80099134)
 *   - @c %b, @c %B : binary         (via @c func_800991AC)
 *   - @c %c, @c %C : single character byte
 *   - @c %s, @c %S : null-terminated string
 *
 * Each @c % may be followed by an optional @c '0' to select zero-padding
 * (default is space-padding), then a decimal field width parsed by
 * @c func_80047C54 (strtol). Plain characters are copied verbatim. The
 * @c escape_marker local anchors the @c '%' constant in a callee-saved
 * register, matching the target's register allocation.
 *
 * @param dst   Output buffer; receives a null-terminated formatted string.
 * @param args  Variadic argument array: @c args[0] is the format string,
 *              subsequent words are pulled in order by format specifiers.
 * @return Length of the produced string (excluding null terminator).
 */
s32 func_80099204(char *dst, s32 *args) {
    char *fmt = (char *)*args++;
    char *out = dst;
    char buf[256];
    u8 ch;
    u8 padCh;
    s32 width;

    ch = *fmt++;
    if (ch == 0) goto end;

    do {
        s32 escape_marker = '%';
        if (ch == escape_marker) {
            padCh = ' ';
            if (*fmt == '0') {
                padCh = '0';
                fmt++;
            }
            width = func_80047C54(fmt, &fmt, 0);
            ch = *fmt++;
            switch (ch) {
                case 'D': case 'd':
                    func_800990A0(*args++, buf);
                    break;
                case 'X': case 'x':
                    func_80099134(*args++, buf);
                    break;
                case 'B': case 'b':
                    func_800991AC(*args++, buf);
                    break;
                case 'C': case 'c':
                    buf[0] = (u8)*args++;
                    buf[1] = 0;
                    break;
                case 'S': case 's':
                    func_80047CA4(buf, (char *)*args++);
                    break;
                default:
                    buf[0] = ch;
                    buf[1] = 0;
                    break;
            }
            if (width > 0) {
                s32 len = func_80047CB4(buf);
                if (len < width) {
                    s32 i;
                    width -= len;
                    for (i = 0; i < width; i++) {
                        *out++ = padCh;
                    }
                }
            }
            func_80047CA4(out, buf);
            out += func_80047CB4(buf);
        } else {
            *out++ = ch;
        }
        ch = *fmt++;
    } while (ch != 0);
end:
    *out = 0;
    return func_80047CB4(dst);
}

/**
 * @brief Variadic forwarder: passes @p a0 and a pointer to the variadic
 *        args to @c func_80099204.
 *
 * The `...` is load-bearing — it triggers gcc's varargs prologue that
 * saves a1-a3 to the caller's shadow area, letting @c &a1 act as the
 * start of a variadic argument list.
 */
s32 func_800993F4(s32 a0, s32 a1, ...) {
    return func_80099204((char *)a0, &a1);
}

/**
 * @brief Printf-style wrapper: formats variadic args into a 256-byte stack
 *        buffer via @c func_80099204, then outputs the result via
 *        @c func_80098FD8.
 *
 * The `...` triggers gcc's varargs prologue, saving a1-a3 to the caller's
 * shadow save area so @c &a0 can act as the start of a variadic arg list.
 */
void func_80099424(s32 a0, ...) {
    s8 buf[0x100];
    func_80099204((char *)buf, &a0);
    func_80098FD8(buf);
}

/**
 * @brief Flip the battle double-buffer and reinitialize per-frame state.
 *
 * Performs end-of-frame teardown on the current framebuffer/drawenv,
 * then toggles the active buffer index in @c D_80182B54 (0 <-> 1),
 * starts the new frame, resets viewport fields to 8, and rebases the
 * framebuffer (32 KB per buffer) and drawenv (8 bytes per buffer)
 * pointers to the new slot.
 */
void func_80099464(void) {
    u8 *fb = D_801D2FE0;

    SetDrawTPage(fb, 0, 1, 3);
    AddPrim(D_801D3000, fb);
    DrawOTag(D_801D3000);

    D_80182B54 = D_80182B54 ^ 1;
    ClearOTag(D_801D2FF0[D_80182B54], 2);

    D_80182B5A = 8;
    g_textCursorX = 8;
    D_80182B58 = 8;

    D_801D2FE0 = D_801C2FE0[D_80182B54];
    D_801D3000 = D_801D2FF0[D_80182B54];
}

/**
 * @brief Initialize the 10 Triple Triad battle-object slots for a new match.
 *
 * Calls @c resetTriadBoard for one-shot setup, then assigns each of the 10
 * @c g_tripleTriadCardHands slots to a player by tagging it with that player's index
 * (low bit of @c initFlags) and a sequence number (0..N) within that
 * player's hand. The @c fieldD reset clears any stale sub-state.
 *
 * For the "offset hand" layout (@c D_801A2C70[player] @c == @c 3) with
 * the corresponding rule bit clear, @c posData[1] is biased to @c 0x800
 * so the card draws shifted; otherwise it stays at zero.
 *
 * Finally, slots 1 and 2 of the substate parameter table @c D_801D3340
 * have their @c field2 halfword zeroed (idle substate state).
 *
 * @note The reversed @c cnt initialization order matches the compiler's
 *       scheduling for the original source — natural order produces the
 *       wrong store sequence.
 */
void func_8009953C(void) {
    s32 cnt[2];
    s32 i;
    s32 owner;
    TripleTriadCardObject *entry;

    resetTriadBoard();

    cnt[1] = 0;
    cnt[0] = 0;

    for (i = 0; i < 10; i++) {
        s32 c;
        entry = &g_tripleTriadCardHands[i];
        owner = entry->initFlags & 1;
        entry->fieldD = 0;
        entry->groupId = owner;
        c = cnt[owner];
        entry->priority = c;
        cnt[owner] = c + 1;
        if (D_801A2C70[owner] == 3 && !(g_tripleTriadRules & 1)) {
            entry->posData[1] = 0x800;
        }
    }

    D_801D3340[1].field2 = 0;
    D_801D3340[2].field2 = 0;
}

/**
 * @brief Emit @c POLY_G3 primitives for a 4-vertex tetrahedral icon model.
 *
 * Allocates a 24-byte scratch buffer (6 words: 4 packed sxy + p + flag),
 * batches the 4 model vertices through @c RotTransPers4 to project them
 * into screen space, then walks the 4 face descriptors in @c D_80182BB8.
 * For each face it picks 3 of the 4 transformed vertices, runs
 * @c NormalClip to drop back-facing triangles, and (if visible) writes a
 * @c POLY_G3 with the face's pre-packed colors + projected screen
 * positions, links it into @p ot via @c AddPrim, and advances @p prims.
 *
 * The packed @c color0/1/2Word fields are copied into the primitive via
 * @c sw-equivalent word stores to match the source's u32 copy pattern.
 *
 * @param ot     Display-list OT head this batch is chained into.
 * @param prims  Output buffer for @c POLY_G3 primitives (up to 4 emitted).
 * @return Pointer to the next free slot after the last emitted primitive.
 */
POLY_G3 *func_800995F8(void *ot, POLY_G3 *prims) {
    POLY_G3 *out;
    s32 i;
    TriadFaceDesc *face;
    s32 *ptr;

    ptr = (s32 *)func_80098B80(0x18);
    D_801D3008 = (u32 *)ptr;

    RotTransPers4(&D_80182B98[0], &D_80182B98[1], &D_80182B98[2], &D_80182B98[3],
                  &ptr[0], &ptr[1], &ptr[2], &ptr[3], &ptr[4], &ptr[5]);

    out = prims;
    for (i = 0; i < 4; i++) {
        face = &D_80182BB8[i];
        if (NormalClip(D_801D3008[face->v0], D_801D3008[face->v1], D_801D3008[face->v2]) >= 0) {
            out->tag = 0x06000000;
            *(u32 *)&out->r0 = face->color0Word;
            *(u32 *)&out->r1 = face->color1Word;
            *(u32 *)&out->r2 = face->color2Word;
            *(u32 *)&out->x0 = D_801D3008[face->v0];
            *(u32 *)&out->x1 = D_801D3008[face->v1];
            *(u32 *)&out->x2 = D_801D3008[face->v2];
            AddPrim(ot, out);
            out++;
        }
    }

    func_80098BA0(0x18);
    return out;
}

/**
 * @brief Triple Triad card-flip animation handler (battle-state table @c D_800A4588).
 *
 * Allocates a per-frame ::TransformBuf (@c D_801D3010) and advances the card's
 * flip animation by @c node->state, then composes the GTE transform and emits
 * the card's @c POLY_G3 batch:
 *  - **State 0** (init): pick a random flip phase (0/1) via @c func_80023D04,
 *    set the spin delta @c D_801D300C (+/-0x400) and @c D_80182C00.vx (+/-0x8C)
 *    from the phase, seed the scratch vector from the +Z unit @c D_80182BF8,
 *    call @c func_800A233C(0x70), advance to state 1.
 *  - **State 1** (entry arc): counter 0..59 = sine-driven X spin / Y dip,
 *    60..69 = hold, 70..84 = ease back toward @c D_80182C00 (a short-vector
 *    lerp via @c func_8003F884 with an oscillating Y dip); at >=85 latch the
 *    phase into @c D_801D30F8 and go to state 2.
 *  - **State 2** (idle): nudge @c D_80182C08.vy each frame and write the static
 *    pose; transition to state 3 when @c D_801D30F8 disagrees with the phase.
 *  - **State 3** (re-flip): a 10-frame sine swing, then toggle the phase bit
 *    and return to state 2.
 *
 * The common tail builds a YXZ rotation from @c D_80182C08, copies the scratch
 * vector into the matrix translation, applies a fixed +0x100 X tilt, loads the
 * GTE rotation/translation matrices, and emits one @c POLY_G3 batch via
 * @c func_800995F8 into the active OT.
 *
 * @param node Battle-state handler node (state/counter/phase at 0x10..0x12).
 * @return Always 0 (the handler keeps running).
 *
 * @note Two locals pin the original build's register allocation: @c tmp is a
 *       single scratch reused for the state-1 lerp weight and the state-2
 *       transition test (two separate locals do not match), and the OT slot
 *       pointer is cached in @c ot before the emit call (inlining it reorders
 *       the @c %hi loads).
 */
s32 cardFlipHandler(HandlerNode *node) {
    s32 tmp;
    u32 *ot;

    D_801D3010 = (TransformBuf *)func_80098B80(0x28);
    switch (node->state) {
    case 0:
        node->phase = func_80023D04() % 2;
        D_801D300C = !node->phase ? -0x400 : 0x400;
        D_80182C00.vx = !node->phase ? -0x8C : 0x8C;
        D_801D3010->vec = D_80182BF8;
        func_800A233C(0x70);
        node->state = 1;
        node->counter = 0;
        break;
    case 1: {
        s32 c = node->counter;
        if (c < 60) {
            s32 t = (c << 12) / 60;
            s32 sv = func_8003ED64(t / 4);
            D_80182C08.vx = (u32)t >> 2;
            D_80182C08.vy = ((D_801D300C + 0xA000) * sv) >> 12;
            D_801D3010->vec = D_80182BF8;
        } else if ((c -= 60) < 10) {
            D_801D3010->vec = D_80182BF8;
        } else if ((c -= 10) < 15) {
            s32 d = (c << 12) / 15;
            tmp = d;
            D_80182C08.vx = (-(d << 10) >> 12) + 0x400;
            D_80182C08.vy = D_801D300C + (((-D_801D300C) * d) >> 12);
            func_8003F884(&D_80182BF8, &D_80182C00, 0x1000 - tmp, d, &D_801D3010->vec);
            d = (func_8003ED64(d / 2) << 4) >> 12;
            D_801D3010->vec.vy -= d;
        } else {
            D_80182C08.vx = 0;
            D_801D3010->vec = D_80182C00;
            D_801D30F8 = node->phase;
            node->state = 2;
            node->counter = 0;
        }
        node->counter++;
        break;
    }
    case 2: {
        s32 xPos = 0x8C;
        D_80182C08.vy += 0x10;
        xPos = !node->phase ? -0x8C : xPos;
        D_801D3010->vec.vy = -0x5C;
        D_801D3010->vec.vz = 0x200;
        D_801D3010->vec.vx = xPos;
        tmp = D_801D30F8 != node->phase;
        if (tmp) {
            node->state = 3;
            node->counter = 0;
        }
        break;
    }
    case 3: {
        s32 t = (node->counter << 12) / 10;
        s32 sinv = func_8003ED64(t / 4);
        if (node->phase) {
            sinv = 0x1000 - sinv;
        }
        D_801D3010->vec.vx = ((sinv * 0x118) >> 12) - 0x8C;
        D_801D3010->vec.vy = -0x5C;
        D_801D3010->vec.vz = (-(func_8003ED64(t / 2) << 6) >> 12) + 0x200;
        node->counter++;
        if (node->counter >= 10) {
            node->state = 2;
            node->counter = 0;
            node->phase ^= 1;
        }
        break;
    }
    }
    func_80041274(&D_80182C08, &D_801D3010->mat);
    D_801D3010->mat.t[0] = D_801D3010->vec.vx;
    D_801D3010->mat.t[1] = D_801D3010->vec.vy;
    D_801D3010->mat.t[2] = D_801D3010->vec.vz;
    func_80041794(0x100, &D_801D3010->mat);
    SetRotMatrix(&D_801D3010->mat);
    SetTransMatrix(&D_801D3010->mat);
    ot = &D_801C2EB0[4];
    D_801C2EB4 = func_800995F8(ot, D_801C2EB4);
    func_80098BA0(0x28);
    return 0;
}
