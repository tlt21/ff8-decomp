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
    initScratchHeap();
    initTriadTaskPool();
    func_80098B68();
    initGraphics();
    resetTriadMenuState();
    func_800A1BE0();
    func_800A2208();
    initTextBuffer();
    func_8009EB98();
    queueTimUpload(&g_tripleTriadCardArt);
    queueTimUpload(&g_tripleTriadCardFrames);

    g_tripleTriadInputFlags = 0;
    g_tripleTriadFrameCount = 0;
    g_tripleTriadState = TT_STATE_INIT;

    for (i = 0; i < 110; i++) {
        g_tripleTriadCardCounts[i] = func_80023B14(i);
    }
}

/**
 * @brief Triple Triad main loop — drives the game until it signals exit.
 *
 * Initializes the engine, then runs one iteration per frame: service input,
 * advance the state-machine handlers, and tick the per-frame subsystems. On
 * exit it fades out, runs two finalize frames, and restores the vsync rate.
 *
 * @return Always 0.
 */
s32 tripleTriadMainLoop(void) {
    s32 i;

    initTripleTriad();

    do {
        if (g_tripleTriadInputFlags & TT_INPUT_DISABLED) {
            if (g_padPressed[2] & 0x30) {
                closeMenu();
            }
            for (i = 0; i < 3; i++) {
                g_padPressed[i] = 0;
                g_padHeld[i] = g_padRepeat[i] = 0;
            }
        }

        while (g_tripleTriadState != TT_STATE_IDLE && g_tripleTriadState != TT_STATE_EXIT) {
            g_tripleTriadActiveList = g_tripleTriadStateHandlers[g_tripleTriadState]();
            if (g_tripleTriadActiveList != 0) {
                g_tripleTriadState = TT_STATE_IDLE;
            }
        }

        if (g_tripleTriadActiveList != 0) updateObjectList(g_tripleTriadActiveList);
        processTriadTasks();
        updateFadeEffects();
        updateTriadMenu();
        func_800A1C6C();
        flipBuffers();
        sampleInput();
        func_800A2214();
        g_tripleTriadFrameCount++;
    } while (g_tripleTriadState != TT_STATE_EXIT);

    g_fadeCounter = -1;
    for (i = 0; i < 2; i++) {
        func_800A1C6C();
        flipBuffers();
    }
    clearAllSfx();
    g_vsyncRate = 100;
    return 0;
}

/**
 * @brief Initialize the GPU draw/display environments and the GTE.
 *
 * Sets up the two double-buffered draw/display environments, clears the
 * framebuffer and texture VRAM, primes the ordering table and primitive
 * pool, then initializes the GTE and masks the display. Called once during
 * engine startup.
 */
void initGraphics(void) {
    DISPENV *disp;
    RECT *screen0;
    RECT *screen1;

    SetDefDrawEnv(&g_drawEnvs[0], 0, 0, TT_DRAW_W, TT_SCREEN_H);
    disp = g_dispEnvs;
    SetDefDispEnv(&disp[0], TT_DRAW_W, 0, TT_DRAW_W, TT_SCREEN_H);
    screen0 = &disp[0].screen;
    screen0->x = 0;
    screen0->y = TT_SCREEN_Y;
    screen0->w = TT_SCREEN_W;
    screen0->h = TT_SCREEN_H;

    SetDefDrawEnv(&g_drawEnvs[1], TT_DRAW_W, 0, TT_DRAW_W, TT_SCREEN_H);
    SetDefDispEnv(&disp[1], 0, 0, TT_DRAW_W, TT_SCREEN_H);
    disp[1].screen.x = 0;
    screen1 = &disp[1].screen;
    screen1->y = TT_SCREEN_Y;
    screen1->w = TT_SCREEN_W;
    screen1->h = TT_SCREEN_H;

    ClearImage(&g_fbClearRect, 0, 0, 0);
    g_fbClearRect.x = TT_DRAW_W;
    g_fbClearRect.y = 0;
    ClearImage(&g_fbClearRect, 0, 0, 0);

    ClearImage(&g_texClearRect, 0xFF, 0xFF, 0xFF);

    g_drawBufferIndex = 0;
    g_otBase = &g_orderingTables[0][0];
    ClearOTagR(&g_orderingTables[0][0], TT_OT_LEN);
    g_primCursor = &g_primPools[g_drawBufferIndex][0];

    InitGeom();
    SetGeomOffset(TT_DRAW_W / 2, TT_SCREEN_H / 2);
    SetGeomScreen(TT_PROJ_DIST);
    SetDispMask(0);

    g_fadeCounter = 2;
    g_vsyncMode = 0;
    resetVramQueue();
}

/**
 * @brief Flip the double-buffered draw/display environments — the per-frame present.
 *
 * Waits for vsync and the previous draw to finish, advances the fade counter
 * @c g_fadeCounter (toggling the display mask as it crosses zero), then flips the
 * active buffer index @c g_drawBufferIndex: the new buffer's draw/disp envs are pushed
 * to the GPU, the just-completed buffer's ordering table is drawn, and the new
 * buffer's OT and primitive pool are reset so the next frame accumulates into
 * it. @c g_activeDrawEnv tracks the buffer now being built.
 */
void flipBuffers(void) {
    flipTextBuffer();
    VSync(1);
    DrawSync(0);
    VSync(1);
    VSync(g_vsyncMode);

    if (g_fadeCounter > 0) {
        g_fadeCounter--;
        if (g_fadeCounter == 0) {
            SetDispMask(1);
        }
    } else if (g_fadeCounter < 0) {
        g_fadeCounter++;
        if (g_fadeCounter == 0) {
            SetDispMask(0);
        }
    }

    g_drawBufferIndex ^= 1;

    PutDrawEnv(&g_drawEnvs[g_drawBufferIndex]);
    PutDispEnv(&g_dispEnvs[g_drawBufferIndex]);
    flushVramTransfers();

    DrawOTag(&g_orderingTables[g_drawBufferIndex ^ 1][TT_OT_LEN - 1]);
    g_primCursor = &g_primPools[g_drawBufferIndex][0];
    g_otBase = &g_orderingTables[g_drawBufferIndex][0];
    ClearOTagR(&g_orderingTables[g_drawBufferIndex][0], TT_OT_LEN);

    g_activeDrawEnv = &g_drawEnvs[g_drawBufferIndex ^ 1];
}

/**
 * @brief Sample the controllers and publish pad 0's button masks.
 *
 * Reads both pads via @c readPads, then writes pad 0's held / pressed / repeat
 * masks into all three slots of @c g_padHeld / @c g_padPressed / @c g_padRepeat.
 */
void sampleInput(void) {
    s32 i;

    readPads();
    g_padHeld[0] = getPadHeld(0);
    g_padPressed[0] = getPadPressed(0);
    g_padRepeat[0] = getPadRepeat(0);

    for (i = 1; i < 3; i++) {
        g_padHeld[i] = g_padHeld[0];
        g_padPressed[i] = g_padPressed[0];
        g_padRepeat[i] = g_padRepeat[0];
    }
}

/**
 * @brief Clear g_vramQueueCount to zero.
 */
void resetVramQueue(void) {
    g_vramQueueCount = 0;
}

/**
 * @brief Dispatch all pending entries in the @c g_vramQueue GPU-transfer pool.
 *
 * Walks @c g_vramQueueCount entries and performs the requested operation on each
 * (LoadImage / LoadImage TIM / StoreImage / MoveImage), then resets the pool
 * count to zero. Entries are typically queued by @c flushVramTransfers's siblings
 * during a frame and flushed here once vsync has elapsed.
 */
void flushVramTransfers(void) {
    s32 i;
    PoolEntry *p = g_vramQueue;

    for (i = 0; i < g_vramQueueCount; p++, i++) {
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
    g_vramQueueCount = 0;
}

/**
 * @brief Enqueue a LoadImage transfer (@c active=0).
 *
 * Takes the next g_vramQueue slot, marks it as a LoadImage, copies the 8-byte
 * @p rect into the slot (unaligned-safe), and stores @p src as the source
 * pixel pointer.
 *
 * @param rect Destination RECT (8 bytes, may be unaligned).
 * @param src  Source pixel data.
 */
void queueLoadImage(u8 *rect, u8 *src) {
    PoolEntry *entry = &g_vramQueue[g_vramQueueCount++];
    entry->active = 0;
    memcpy(&entry->rect, rect, 8);
    entry->src = src;
}

/**
 * @brief Enqueue a TIM upload (@c active=1) and return its resolved data address.
 *
 * Takes the next g_vramQueue slot, marks it as a TIM upload, and stores @p res
 * as the source. Then walks @p res's two-step @c offset chain (each step adds a
 * relative @c s32 offset) to compute the resource's payload address.
 *
 * @param res Resource header whose @c offset field chains to its payload.
 * @return The resolved payload address at the end of the offset chain.
 */
u8 *queueTimUpload(ResHeader *res) {
    PoolEntry *entry;
    s32 *rel;

    entry = &g_vramQueue[g_vramQueueCount++];
    entry->active = 1;
    entry->src = res;

    rel = &res->offset;
    rel = (s32 *)((u8 *)rel + *rel);
    return (u8 *)rel + *rel;
}

/**
 * @brief Enqueue a StoreImage transfer (@c active=2).
 *
 * Variant of @c queueLoadImage that marks the slot as a StoreImage: copies the
 * 8-byte @p rect into the slot and stores @p dst as the destination buffer.
 *
 * @param rect Source RECT (8 bytes, may be unaligned).
 * @param dst  Destination buffer.
 */
void queueStoreImage(u8 *rect, u8 *dst) {
    PoolEntry *entry = &g_vramQueue[g_vramQueueCount++];
    entry->active = 2;
    memcpy(&entry->rect, rect, 8);
    entry->src = dst;
}

/**
 * @brief Enqueue a MoveImage transfer (@c active=3).
 *
 * Same pattern as @c queueLoadImage / @c queueStoreImage, but packs the
 * destination coordinates into the source field as @c dstY<<16 | dstX.
 *
 * @param rect Source RECT (8 bytes, may be unaligned).
 * @param dstX Destination X (lower 16 bits).
 * @param dstY Destination Y (upper 16 bits).
 */
void queueMoveImage(u8 *rect, s16 dstX, u16 dstY) {
    PoolEntry *entry = &g_vramQueue[g_vramQueueCount++];
    entry->active = 3;
    memcpy(&entry->rect, rect, 8);
    entry->src = (void *)(((s32)dstY << 16) | dstX);
}

void func_80098B68(void) {
}

/**
 * @brief Set g_scratchPtr to 0x1F800000 (scratchpad RAM base address).
 */
void initScratchHeap(void) {
    g_scratchPtr = 0x1F800000;
}

/**
 * @brief Allocate aligned memory from the scratchpad heap and return the new pointer.
 *
 * Rounds @p size up to the next multiple of 4, then advances g_scratchPtr
 * by that amount. This is the counterpart of scratchFree (which subtracts).
 *
 * @param size Size to allocate (will be aligned up to 4).
 * @return The previous value of g_scratchPtr (start of allocated block).
 */
s32 scratchAlloc(s32 size) {
    s32 old = g_scratchPtr;
    g_scratchPtr = old + ((size + 3) & ~3);
    return old;
}

/**
 * @brief Align a size up to 4 bytes and subtract from the allocation pointer.
 *
 * Rounds @p size up to the next multiple of 4 and decrements g_scratchPtr by that amount.
 *
 * @param size Size to free (will be aligned up to 4).
 */
void scratchFree(s32 size) {
    g_scratchPtr -= (size + 3) & ~3;
}

/**
 * @brief Initialize an @c ObjList header and mark every pool node free.
 *
 * @param listMem List header to initialize.
 * @param pool    Node-pool base.
 * @param stride  Size of each node in bytes.
 * @param count   Number of nodes in the pool.
 */
void initObjList(u8 *listMem, u8 *pool, s32 stride, s32 count) {
    ObjList *list = (ObjList *)listMem;
    s32 i;

    list->head = 0;
    list->tail = 0;
    list->pool = pool;
    list->stride = stride;
    list->count = count;

    for (i = 0; i < count; i++) {
        ((ObjListNode *)pool)->flags = 0;
        pool += stride;
    }
}

/**
 * @brief Find and return the first free node in the pool.
 *
 * Scans the pool for a node whose @c flags bit 0 is clear (inactive).
 *
 * @param listMem List header whose pool is scanned.
 * @return The first free node, or NULL if all nodes are in use.
 */
void *findFreeNode(u8 *listMem) {
    ObjList *list = (ObjList *)listMem;
    s32 count = list->count;
    u8 *node = list->pool;
    s32 i = 0;

    if (count > 0) {
        s32 n = count;
        do {
            if (!(((ObjListNode *)node)->flags & 1)) {
                return node;
            }
            i++;
            node += list->stride;
        } while (i < n);
    }
    return 0;
}

/**
 * @brief Allocate a node and append it to the list tail.
 *
 * Allocates a free node, marks it active, stores @p callback, and links it
 * onto the tail (or sets @c head if the list was empty).
 *
 * @param listMem  List header.
 * @param callback Per-frame callback stored in the new node.
 * @return The new node, or NULL if the pool is full.
 *
 * @note The header is reached via @c (ObjList *)listMem rather than a cached
 *       local so gcc keeps it in the saved arg register across the alloc call.
 */
void *allocObjNode(u8 *listMem, s32 callback) {
    ObjListNode *node = findFreeNode(listMem);

    if (node != 0) {
        ObjListNode *tail;
        node->flags |= 1;
        node->field02 = 0;
        node->next = 0;
        node->callback = callback;
        tail = ((ObjList *)listMem)->tail;
        if (tail != 0) {
            tail->next = node;
        } else {
            ((ObjList *)listMem)->head = node;
        }
        ((ObjList *)listMem)->tail = node;
    }
    return node;
}

/**
 * @brief Allocate a node and prepend it to the list head.
 *
 * @param listMem  List header.
 * @param callback Per-frame callback stored in the new node.
 * @return The new node, or NULL if the pool is full.
 */
void *allocObjNodeFront(u8 *listMem, s32 callback) {
    ObjList *list = (ObjList *)listMem;
    ObjListNode *node = findFreeNode(listMem);

    if (node != 0) {
        node->flags |= 1;
        node->field02 = 0;
        node->callback = callback;
        node->next = list->head;
        list->head = node;
    }
    return node;
}

/**
 * @brief Tick every node's callback, unlinking those that report completion.
 *
 * Walks the list, calling each node's @c callback; a return value with bit 1
 * set unlinks (and frees) that node. Surviving nodes are counted and the tail
 * pointer is rebuilt.
 *
 * @param listMem List header.
 * @return Number of nodes still live.
 */
s32 updateObjectList(u8 *listMem) {
    ObjList *list = (ObjList *)listMem;
    ObjListNode *prev = 0;
    ObjListNode *node = list->head;
    s32 count = 0;
    s32 result;

    while (node != 0) {
        result = ((ObjNodeFn)node->callback)(node);
        if (result & 2) {
            node->flags = 0;
            if (prev != 0) {
                prev->next = node->next;
            } else {
                list->head = node->next;
            }
        } else {
            prev = node;
            count++;
        }
        node = node->next;
    }
    list->tail = prev;
    return count;
}

/**
 * @brief Register @c g_textBufferRes and set up the current frame's draw targets.
 *
 * Registers the @c g_textBufferRes resource into the g_vramQueue pool via
 * @c queueTimUpload, kicks off @c ClearOTag for the current buffer's
 * drawenv slot, then rebases the framebuffer (@c g_textFbPtr) and drawenv
 * (@c g_textOtPtr) pointers to the slot indexed by @c g_textBufferIndex. Unlike
 * @c flipTextBuffer, this does not flip the buffer index first.
 */
void initTextBuffer(void) {
    queueTimUpload(&g_textBufferRes);
    ClearOTag(g_textOTs[g_textBufferIndex], 2);
    g_textFbPtr = g_textFrameBufs[g_textBufferIndex];
    g_textOtPtr = g_textOTs[g_textBufferIndex];
}

/**
 * @brief Set the debug-text cursor origin (each axis only if non-negative).
 *
 * Stores @p x to g_textLineX and g_textCursorX if @p x >= 0.
 * Stores @p y to g_textCursorY if @p y >= 0.
 *
 * @param x Cursor origin X (stored if >= 0).
 * @param y Cursor origin Y (stored if >= 0).
 */
void setTextOrigin(s32 x, s32 y) {
    if (x >= 0) {
        g_textLineX = x;
        g_textCursorX = x;
    }
    if (y >= 0) {
        g_textCursorY = y;
    }
}

/**
 * @brief Append one debug-text character as a 5x5 textured sprite primitive.
 *
 * Renders @p ch (uppercased on the fly) from the debug font atlas into a
 * fresh @c SPRT at the current packet-buffer tail (@c g_textFbPtr), then
 * advances the cursor and the buffer pointer. Newline resets the cursor
 * to the line origin and steps down 7 px; any non-printable byte (below
 * space) just advances the cursor without emitting a glyph.
 *
 * The U/V atlas math packs 8 glyphs per row across 64 px; biases by
 * @c -0x80 / @c -0x20 wrap the indices into the texture-page coordinate
 * space. Odd/even character codes alternate between two CLUTs.
 */
void drawTextChar(u8 ch) {
    SPRT *prim;
    s32 adjusted;
    s32 row;

    prim = (SPRT *)g_textFbPtr;

    if (ch == '\n') {
        g_textCursorX = g_textLineX;
        g_textCursorY += 7;
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

    prim->r0 = g_textColor.r;
    prim->g0 = g_textColor.g;
    prim->b0 = g_textColor.b;

    prim->x0 = g_textCursorX;
    prim->y0 = g_textCursorY;

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

    AddPrim((u32 *)g_textOtPtr, prim);

    g_textFbPtr = (u8 *)(prim + 1);
    g_textCursorX += 6;
}

/**
 * @brief Render a null-terminated text string, with @c '#<digit>' color escapes.
 *
 * Walks @p str byte by byte. Characters are passed to @c drawTextChar for
 * sprite rendering. A @c '#' begins an escape: if the next byte is an ASCII
 * digit @c '0'..'8', it indexes the palette table @c g_textPalette and the
 * matching 4-byte RGB entry is copied into the live @c g_textColor color;
 * any other byte after @c '#' is rendered as a plain character (the @c '#'
 * is consumed silently).
 *
 * @note The @c escape_marker variable holds @c '#' across iterations to
 * anchor the constant in a callee-saved register (matches target codegen).
 */
void drawText(u8 *str) {
    u8 ch;

    for (ch = *str++; ch != 0; ch = *str++) {
        s32 digitEscape = '#';
        if (ch == digitEscape) {
            s32 next = *str++;
            s32 digit = next & 0xFF;
            if (digit < '9') {
                if (digit >= '0') {
                    /* Set color from palette entry indexed by ASCII digit. */
                    memcpy(&g_textColor, &g_textPalette[digit], 4);
                } else {
                    drawTextChar(next);
                }
            } else {
                drawTextChar(next);
            }
        } else {
            drawTextChar(ch);
        }
    }
}

/**
 * Converts an integer to a decimal string representation.
 *
 * @param value The integer value to convert.
 * @param out   Pointer to the output buffer.
 * @return Pointer to the end of the written string.
 */
u8 *intToDecStr(s32 value, u8 *out) {
    u8 buf[36];
    u8 *dst = out;
    u8 *p;

    if (value < 0) {
        *dst = 0x2D;
        dst++;
        value = -value;
    }

    p = buf + 33;
    buf[33] = 0;

    do {
        p--;
        *p = (value % 10) + 0x30;
        value = value / 10;
    } while (value != 0);

    strcpy(dst, p);
    return dst + strlen(dst);
}

/**
 * Converts an integer to a hexadecimal string representation.
 *
 * @param value The integer value to convert.
 * @param out   Pointer to the output buffer.
 * @return Pointer to the end of the written string.
 */
u8 *intToHexStr(s32 value, u8 *out) {
    u8 buf[20];
    u8 *dst = out;
    u8 *p;
    u8 *table;

    if (value < 0) {
        *dst = 0x2D;
        dst++;
        value = -value;
    }

    p = buf + 17;
    buf[17] = 0;
    table = g_hexDigits;

    do {
        p--;
        *p = *(u8 *)((value & 0xF) + (s32)table);
        value >>= 4;
    } while (value != 0);

    strcpy(dst, p);
    return dst + strlen(dst);
}

/**
 * @brief Convert an integer to a binary string representation.
 *
 * Writes the binary digits of @p value into the buffer at @p out,
 * then returns a pointer to the end of the written string.
 *
 * @param value The integer value to convert.
 * @param out   Pointer to the output buffer.
 * @return Pointer to the end of the written string.
 */
u8 *intToBinStr(s32 value, u8 *out) {
    u8 buf[36];
    u8 *dst = out;
    u8 *p;

    p = buf + 33;
    buf[33] = 0;

    do {
        p--;
        *p = (value & 1) + 0x30;
        value >>= 1;
    } while (value != 0);

    strcpy(dst, p);
    return dst + strlen(dst);
}

/**
 * @brief printf-style format-into-buffer.
 *
 * Walks the format string given as @c args[0] and writes the formatted
 * output into @p dst. The remaining @c args[1..] are the values referenced
 * by format specifiers. Supported specifiers:
 *
 *   - @c %d, @c %D : signed decimal (via @c intToDecStr)
 *   - @c %x, @c %X : hexadecimal    (via @c intToHexStr)
 *   - @c %b, @c %B : binary         (via @c intToBinStr)
 *   - @c %c, @c %C : single character byte
 *   - @c %s, @c %S : null-terminated string
 *
 * Each @c % may be followed by an optional @c '0' to select zero-padding
 * (default is space-padding), then a decimal field width parsed by
 * @c strtol (strtol). Plain characters are copied verbatim. The
 * @c escape_marker local anchors the @c '%' constant in a callee-saved
 * register, matching the target's register allocation.
 *
 * @param dst   Output buffer; receives a null-terminated formatted string.
 * @param args  Variadic argument array: @c args[0] is the format string,
 *              subsequent words are pulled in order by format specifiers.
 * @return Length of the produced string (excluding null terminator).
 */
s32 formatString(char *dst, s32 *args) {
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
            width = strtol(fmt, &fmt, 0);
            ch = *fmt++;
            switch (ch) {
                case 'D': case 'd':
                    intToDecStr(*args++, buf);
                    break;
                case 'X': case 'x':
                    intToHexStr(*args++, buf);
                    break;
                case 'B': case 'b':
                    intToBinStr(*args++, buf);
                    break;
                case 'C': case 'c':
                    buf[0] = (u8)*args++;
                    buf[1] = 0;
                    break;
                case 'S': case 's':
                    strcpy(buf, (char *)*args++);
                    break;
                default:
                    buf[0] = ch;
                    buf[1] = 0;
                    break;
            }
            if (width > 0) {
                s32 len = strlen(buf);
                if (len < width) {
                    s32 i;
                    width -= len;
                    for (i = 0; i < width; i++) {
                        *out++ = padCh;
                    }
                }
            }
            strcpy(out, buf);
            out += strlen(buf);
        } else {
            *out++ = ch;
        }
        ch = *fmt++;
    } while (ch != 0);
end:
    *out = 0;
    return strlen(dst);
}

/**
 * @brief Variadic forwarder: passes @p dst and a pointer to the variadic
 *        args to @c formatString.
 *
 * The `...` is load-bearing — it triggers gcc's varargs prologue that saves
 * the a1-a3 argument registers to the caller's shadow area, letting @c &fmt
 * act as the start of a variadic argument list.
 *
 * @param dst Output buffer.
 * @param fmt Format string (first variadic word).
 */
s32 btlSprintf(s32 dst, s32 fmt, ...) {
    return formatString((char *)dst, &fmt);
}

/**
 * @brief Printf-style wrapper: formats variadic args into a 256-byte stack
 *        buffer via @c formatString, then outputs the result via
 *        @c drawText.
 *
 * The `...` triggers gcc's varargs prologue, saving the a1-a3 argument
 * registers to the caller's shadow save area so @c &fmt can act as the start
 * of a variadic arg list.
 *
 * @param fmt Format string (followed by variadic args).
 */
void drawTextf(s32 fmt, ...) {
    s8 buf[0x100];
    formatString((char *)buf, &fmt);
    drawText(buf);
}

/**
 * @brief Flip the secondary text/overlay double-buffer and reset its draw state.
 *
 * Separate from the main @c flipBuffers double-buffer: finalizes the current
 * text/overlay framebuffer, toggles its own buffer index @c g_textBufferIndex (0 <-> 1),
 * resets the text cursor/viewport, and rebases the framebuffer (32 KB per buffer)
 * and OT pointers to the new slot.
 */
void flipTextBuffer(void) {
    u8 *fb = g_textFbPtr;

    SetDrawTPage(fb, 0, 1, 3);
    AddPrim(g_textOtPtr, fb);
    DrawOTag(g_textOtPtr);

    g_textBufferIndex = g_textBufferIndex ^ 1;
    ClearOTag(g_textOTs[g_textBufferIndex], 2);

    g_textLineX = 8;
    g_textCursorX = 8;
    g_textCursorY = 8;

    g_textFbPtr = g_textFrameBufs[g_textBufferIndex];
    g_textOtPtr = g_textOTs[g_textBufferIndex];
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
void initCardHands(void) {
    s32 cnt[2];
    s32 i;
    s32 owner;
    TripleTriadCardObject *entry;

    resetTriadBoard();

    cnt[1] = 0;
    cnt[0] = 0;

    for (i = 0; i < 10; i++) {
        s32 seq;
        entry = &g_tripleTriadCardHands[i];
        owner = entry->initFlags & 1;
        entry->fieldD = 0;
        entry->groupId = owner;
        seq = cnt[owner];
        entry->priority = seq;
        cnt[owner] = seq + 1;
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
 * into screen space, then walks the 4 face descriptors in @c g_triadIconFaces.
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
POLY_G3 *drawTriadIcon(void *ot, POLY_G3 *prims) {
    POLY_G3 *out;
    s32 i;
    TriadFaceDesc *face;
    s32 *ptr;

    ptr = (s32 *)scratchAlloc(0x18);
    g_triadIconScratch = (u32 *)ptr;

    RotTransPers4(&g_triadIconVerts[0], &g_triadIconVerts[1], &g_triadIconVerts[2], &g_triadIconVerts[3],
                  &ptr[0], &ptr[1], &ptr[2], &ptr[3], &ptr[4], &ptr[5]);

    out = prims;
    for (i = 0; i < 4; i++) {
        face = &g_triadIconFaces[i];
        if (NormalClip(g_triadIconScratch[face->v0], g_triadIconScratch[face->v1], g_triadIconScratch[face->v2]) >= 0) {
            out->tag = 0x06000000;
            *(u32 *)&out->r0 = face->color0Word;
            *(u32 *)&out->r1 = face->color1Word;
            *(u32 *)&out->r2 = face->color2Word;
            *(u32 *)&out->x0 = g_triadIconScratch[face->v0];
            *(u32 *)&out->x1 = g_triadIconScratch[face->v1];
            *(u32 *)&out->x2 = g_triadIconScratch[face->v2];
            AddPrim(ot, out);
            out++;
        }
    }

    scratchFree(0x18);
    return out;
}

/**
 * @brief Triple Triad card-flip animation handler (battle-state table @c g_tripleTriadStateHandlers).
 *
 * Allocates a per-frame ::TransformBuf (@c g_cardFlipXform) and advances the card's
 * flip animation by @c node->state, then composes the GTE transform and emits
 * the card's @c POLY_G3 batch:
 *  - **State 0** (init): pick a random flip phase (0/1) via @c func_80023D04,
 *    set the spin delta @c g_cardFlipSpin (+/-0x400) and @c g_cardFlipTarget.vx (+/-0x8C)
 *    from the phase, seed the scratch vector from the +Z unit @c g_cardFlipUpVec,
 *    call @c func_800A233C(0x70), advance to state 1.
 *  - **State 1** (entry arc): counter 0..59 = sine-driven X spin / Y dip,
 *    60..69 = hold, 70..84 = ease back toward @c g_cardFlipTarget (a short-vector
 *    lerp via @c func_8003F884 with an oscillating Y dip); at >=85 latch the
 *    phase into @c g_cardFlipPhase and go to state 2.
 *  - **State 2** (idle): nudge @c g_cardFlipAngles.vy each frame and write the static
 *    pose; transition to state 3 when @c g_cardFlipPhase disagrees with the phase.
 *  - **State 3** (re-flip): a 10-frame sine swing, then toggle the phase bit
 *    and return to state 2.
 *
 * The common tail builds a YXZ rotation from @c g_cardFlipAngles, copies the scratch
 * vector into the matrix translation, applies a fixed +0x100 X tilt, loads the
 * GTE rotation/translation matrices, and emits one @c POLY_G3 batch via
 * @c drawTriadIcon into the active OT.
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

    g_cardFlipXform = (TransformBuf *)scratchAlloc(0x28);
    switch (node->state) {
    case 0:
        node->phase = func_80023D04() % 2;
        g_cardFlipSpin = !node->phase ? -0x400 : 0x400;
        g_cardFlipTarget.vx = !node->phase ? -0x8C : 0x8C;
        g_cardFlipXform->vec = g_cardFlipUpVec;
        func_800A233C(0x70);
        node->state = 1;
        node->counter = 0;
        break;
    case 1: {
        s32 frame = node->counter;
        if (frame < 60) {
            s32 t = (frame << 12) / 60;
            s32 sine = func_8003ED64(t / 4);
            g_cardFlipAngles.vx = (u32)t >> 2;
            g_cardFlipAngles.vy = ((g_cardFlipSpin + 0xA000) * sine) >> 12;
            g_cardFlipXform->vec = g_cardFlipUpVec;
        } else if ((frame -= 60) < 10) {
            g_cardFlipXform->vec = g_cardFlipUpVec;
        } else if ((frame -= 10) < 15) {
            s32 d = (frame << 12) / 15;
            tmp = d;
            g_cardFlipAngles.vx = (-(d << 10) >> 12) + 0x400;
            g_cardFlipAngles.vy = g_cardFlipSpin + (((-g_cardFlipSpin) * d) >> 12);
            func_8003F884(&g_cardFlipUpVec, &g_cardFlipTarget, 0x1000 - tmp, d, &g_cardFlipXform->vec);
            d = (func_8003ED64(d / 2) << 4) >> 12;
            g_cardFlipXform->vec.vy -= d;
        } else {
            g_cardFlipAngles.vx = 0;
            g_cardFlipXform->vec = g_cardFlipTarget;
            g_cardFlipPhase = node->phase;
            node->state = 2;
            node->counter = 0;
        }
        node->counter++;
        break;
    }
    case 2: {
        s32 xPos = 0x8C;
        g_cardFlipAngles.vy += 0x10;
        xPos = !node->phase ? -0x8C : xPos;
        g_cardFlipXform->vec.vy = -0x5C;
        g_cardFlipXform->vec.vz = 0x200;
        g_cardFlipXform->vec.vx = xPos;
        tmp = g_cardFlipPhase != node->phase;
        if (tmp) {
            node->state = 3;
            node->counter = 0;
        }
        break;
    }
    case 3: {
        s32 t = (node->counter << 12) / 10;
        s32 sine = func_8003ED64(t / 4);
        if (node->phase) {
            sine = 0x1000 - sine;
        }
        g_cardFlipXform->vec.vx = ((sine * 0x118) >> 12) - 0x8C;
        g_cardFlipXform->vec.vy = -0x5C;
        g_cardFlipXform->vec.vz = (-(func_8003ED64(t / 2) << 6) >> 12) + 0x200;
        node->counter++;
        if (node->counter >= 10) {
            node->state = 2;
            node->counter = 0;
            node->phase ^= 1;
        }
        break;
    }
    }
    func_80041274(&g_cardFlipAngles, &g_cardFlipXform->mat);
    g_cardFlipXform->mat.t[0] = g_cardFlipXform->vec.vx;
    g_cardFlipXform->mat.t[1] = g_cardFlipXform->vec.vy;
    g_cardFlipXform->mat.t[2] = g_cardFlipXform->vec.vz;
    func_80041794(0x100, &g_cardFlipXform->mat);
    SetRotMatrix(&g_cardFlipXform->mat);
    SetTransMatrix(&g_cardFlipXform->mat);
    ot = &g_otBase[4];
    g_primCursor = drawTriadIcon(ot, g_primCursor);
    scratchFree(0x28);
    return 0;
}
