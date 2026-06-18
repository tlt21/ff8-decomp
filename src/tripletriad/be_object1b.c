#include "common.h"
#include "battle.h"
#include "tripletriad.h"
#include "psxsdk/libc.h"
#include "psxsdk/libgpu.h"
#include "tripletriad/be_object1.h"
#include "gamestate.h"

/*
 * be_object1b.c — tail of be_object1 (func_80099C78 onward), split into its own
 * translation unit so func_80099C78's jump table is the FIRST .rodata in this
 * object. gcc emits each jtbl in a fresh ".section .rodata / .align 3" block;
 * when two jtbls share one object the second is 8-aligned, inserting a 4-byte
 * pad. The original places func_80099C78's table at 0xDC (4-aligned, no pad)
 * directly after func_80099204's 55-entry table that fills 0x00..0xDC. Keeping
 * this table first in its own object (placed by the linker's SUBALIGN(2)) lands
 * it at 0xDC with no pad. See [[func_80099C78_state]] / the be_object3b split.
 */

/* func_80099C78 (Triple Triad match flow) hold-frame durations + result-screen input bit. */
#define TT_HOLD_FRAMES_RULE  0x1E  /**< Frames to hold after a combo capture (case 4). */
#define TT_HOLD_FRAMES_TALLY 0x0C  /**< Frames to hold on the card-count tally (case 7). */
#define TT_HOLD_FRAMES_FADE  0x0F  /**< Frames to hold during the result fade (case 9). */
#define PAD_UP               0x4000 /**< D_801C2EC4 result-screen cancel bit. */

/**
 * @brief Bump a win/loss/draw counter in the Triple Triad save record (case 9).
 *
 * The @c do/while(0) wrapper is a scheduling barrier: it keeps the counter's
 * read-modify-write (@c lhu / @c addiu / @c sh) in its own basic block so
 * gcc 2.7.2 cannot hoist the @c lhu ahead of the preceding @c D_80082C9C
 * store. That reproduces the original's store-then-tally order (one extra
 * load-delay @c nop per branch); writing @c counter++ inline lets gcc hoist
 * the load and drops 3 instructions.
 */
#define TT_TALLY(counter) do { (counter)++; } while (0)

/* Match-result / result-screen state (be_object1b-local). */
extern u8  D_80082C9C;              /* match-result category byte */
extern u16 D_801C2EC4;              /* result-screen pad input */
extern s32 D_801D3018;              /* result-screen SFX handle */
extern u8  D_801D30FC;              /* match winner (0/1) or 2 for draw */

/* Functions defined in be_object1.c. */
extern u8  *func_80098A1C(u8 *drawEnv, u8 *cb);
extern void func_80098BC0(u8 *list, u8 *pool, s32 nodeSize, s32 capacity);
extern s32  func_80098D28(u8 *sub);
extern s32  cardFlipHandler(HandlerNode *node);
extern void func_8009953C(void);

/* Functions defined in other tripletriad TUs. */
extern void processCardObjects(s32 a0);
extern void resetTriadBoard(void);
extern void setupTripleTriadHands(void);

/**
 * @brief Triple Triad match controller — 10-state machine driving the post-game
 *        result flow (rule resolution → card counting → reward → cleanup).
 *
 * Called as a per-frame handler off the battle-object dispatch chain. Each
 * call advances the state machine until it must wait for an asynchronous
 * event (input poll, sub-handler completion, animation finish), at which
 * point it returns @c 0 and the next frame re-enters. The state byte at
 * @c ctl->state (@c 0..9) is dispatched via a jumptable; states @c 2 and
 * @c 3 are unused and re-loop the dispatch (gcc-default fill for unused
 * cases). State @c >= 10 hangs intentionally.
 *
 * State outline:
 *   - **0** (init): on first frame allocate a sub-handler running
 *     @c cardFlipHandler, mark @c D_801D30F8 = @c -1, clear the substate
 *     parameter slots. Stays here until @c D_801D30F8 becomes non-negative
 *     (the sub-handler picks a starting player), then advances to state 1.
 *   - **1** (player turn): on first frame dispatch on
 *     @c D_801A2C70[D_801D30F8] (player type: 0/1 human, 2 AI, 3 demo) into
 *     @c spawnCardSelectCursor / @c spawnAiTurn to create a per-turn handler,
 *     stashed at @c ctl->subHandler. Subsequent frames poll
 *     @c func_80098D28(subHandler); on completion advance to state 4.
 *   - **4** (rule resolution, first pass): wait for @c anyCardEffectActive (UI
 *     idle). On entry call @c applyCardRules to evaluate Same/Plus chains.
 *     If the result flags either bit 2 (Same) or bit 3 (Plus), play the
 *     matching SFX (@c func_8009EB30 0/1), then sweep the 3x3 play area
 *     for cells marked with the corresponding flag bit and animate them via
 *     @c setCardEntityType. After @c counter == 1, finalize and advance to 5.
 *   - **5** (rule resolution, chain pass): keep applying @c applyCardRules
 *     until the result returns 0, playing the chain SFX (@c 5) once. Then
 *     advance to state 6.
 *   - **6** (empty-cell tally): count occupied cells in the play area. If
 *     all 9 are filled, advance to state 7. Otherwise flip @c D_801D30F8
 *     and loop back to state 1 (next player's turn).
 *   - **7** (card count): wait one frame; then tally cards in
 *     @c g_tripleTriadCardHands by owner (low bit of @c initFlags). Stash the winner
 *     in @c D_801D30FC (@c 0/1 for winner, @c 2 for draw). Wait until
 *     @c counter >= 12, then advance to state 8.
 *   - **8** (result SFX + input wait): on first frame pick the result SFX
 *     based on winner/draw and start it via @c func_8009EB30, saving the
 *     handle in @c D_801D3018. Wait for @c D_801C2EC4 button input; on
 *     cancel (@c 0x4000) silence the SFX and stay; on confirm play
 *     @c func_800A2054 and advance to state 9 (or in Sudden Death mode
 *     loop back to state 1 via @c func_8009953C reset).
 *   - **9** (fade out): call @c func_800A0370 once, wait 15 frames, then
 *     stop the result SFX, update the Triple Triad win/loss/draw counters
 *     in @c TripleTriadData, set @c D_80082C9C category byte, and trigger
 *     battle exit via @c D_801A2CE6 = @c 4.
 *
 * @param ctl  Battle-object handler context (state at +0x10, counter at
 *             +0x11, sub-handler pointer at +0x0C, rule flags at +0x13,
 *             retry flag at +0x15).
 * @return Always @c 0.
 *
 * @note Reaching 100% needs four register/scheduling constructs (a clean
 *       rewrite with separate, well-named locals and plain @c ++ compiles to
 *       ~99%): (1) ONE function-scope scratch @c acc, reused as the case-4
 *       combo cell-mask AND the case-6 empty-cell count — two separate locals
 *       do not coalesce into the single saved register the original reuses
 *       (otherwise a ~13-instr a3/t0 constant-temp cascade appears). (2)
 *       @c row=acc before the case-6 test reuses the dead loop counter,
 *       reproducing gcc's IV-vs-counter allocation. (3) case 8 tests @c >=3
 *       first and shares the literal @c 3 through @c mode
 *       (@c mode=3;func_800A247C(mode);mode=2) so gcc materializes @c a0=3
 *       once. (4) the case-9 result tally goes through @c TT_TALLY (a
 *       @c do/while(0) scheduling barrier) so the counter load is not hoisted
 *       ahead of the @c D_80082C9C store. The intentional hang on
 *       @c state @c >= @c 10 falls out of gcc's switch bounds check (no
 *       @c default case).
 */
s32 func_80099C78(HandlerNode *ctl) {
    s32 acc;
    while (1) {
        switch (ctl->state) {
            case 0: {
                if (ctl->counter == 0) {
                    HandlerNode *sub = (HandlerNode *)func_80098C44(D_801D3028, (s32)cardFlipHandler);
                    sub->state = 0;
                    sub->counter = 0;
                    D_801D30F8 = -1;
                    D_801D3340[1].field2 = 0;
                    D_801D3340[2].field2 = 0;
                    ctl->counter++;
                }
                if (D_801D30F8 < 0) return 0;
                ctl->state = 1;
                ctl->counter = 0;
                break;
            }
            case 1: {
                if (ctl->counter == 0) {
                    u8 playerType = D_801A2C70[D_801D30F8];
                    switch (playerType) {
                        case 0: ctl->subHandler = (void *)spawnCardSelectCursor(D_801D30F8, 0); break;
                        case 1: ctl->subHandler = (void *)spawnCardSelectCursor(D_801D30F8, 1); break;
                        case 2: ctl->subHandler = (void *)spawnCardSelectCursor(D_801D30F8, 2); break;
                        case 3: ctl->subHandler = (void *)spawnAiTurn(D_801D30F8); break;
                    }
                    ctl->counter++;
                    return 0;
                }
                if (func_80098D28(ctl->subHandler) == 0) {
                    ctl->state = 4;
                    ctl->counter = 0;
                    break;
                }
                return 0;
            }
            case 4: {
                if (anyCardEffectActive()) return 0;
                if (ctl->counter == 0) {
                    u8 rules = applyCardRules(&D_801D3398, 1);
                    ctl->rulesFlags = rules;
                    if (rules & TT_RESULT_COMBO_MASK) {
                        s32 row, col;
                        acc = 0; /* combo cell-mask; also the SFX arg (0) in the Same path */
                        if (rules & TT_RESULT_SAME_FIRED) {
                            func_8009EB30(acc);
                            acc = TT_CELL_SAME_MATCHED;
                        } else if (rules & TT_RESULT_PLUS_FIRED) {
                            func_8009EB30(1);
                            acc = TT_CELL_PLUS_COMBO;
                        }
                        for (row = 1; row <= 3; row++) {
                            for (col = 1; col <= 3; col++) {
                                if (D_801D3398.cells[row][col].flags & acc)
                                    setCardEntityType(D_801D3398.cells[row][col].entityIdx, 6);
                            }
                        }
                        func_800A233C(TT_HOLD_FRAMES_RULE);
                    }
                    ctl->counter++;
                } else {
                    ctl->retryFlag = 1;
                    resolveCaptures(&D_801D3398);
                    ctl->state = 5;
                    ctl->counter = 0;
                }
                break;
            }
            case 5: {
                if (anyCardEffectActive()) return 0;
                if (ctl->rulesFlags == 0) {
                    ctl->state = 6;
                    ctl->counter = 0;
                    break;
                }
                {
                    u8 next = applyCardRules(&D_801D3398, ctl->rulesFlags);
                    ctl->rulesFlags = next;
                    if (next != 0 && ctl->retryFlag != 0) func_8009EB30(5);
                    resolveCaptures(&D_801D3398);
                }
                break;
            }
            case 6: {
                s32 row;
                acc = 0; /* empty-cell count; reuses the case-4 scratch register */
                for (row = 1; row <= 3; row++) {
                    s32 col;
                    for (col = 1; col <= 3; col++) {
                        if (!(D_801D3398.cells[row][col].flags & TT_CELL_OCCUPIED))
                            acc++;
                    }
                }
                row = acc; /* reuse the dead counter for the test (IV/counter alloc) */
                if (row == 0) ctl->state = 7;
                else {
                    D_801D30F8 ^= 1;
                    ctl->state = 1;
                }
                ctl->counter = 0;
                break;
            }
            case 7: {
                ctl->counter++;
                if (ctl->counter == 1) {
                    u8 cnt[2];
                    s32 i;
                    cnt[1] = 0;
                    cnt[0] = 0;
                    for (i = 0; i < 10; i++) cnt[g_tripleTriadCardHands[i].initFlags & 1]++;
                    if (cnt[0] > cnt[1])      D_801D30FC = 0;
                    else if (cnt[1] > cnt[0]) D_801D30FC = 1;
                    else                      D_801D30FC = 2;
                }
                if ((u8)ctl->counter < TT_HOLD_FRAMES_TALLY) return 0;
                ctl->state = 8;
                ctl->counter = 0;
                break;
            }
            case 8: {
                if (ctl->counter == 0) {
                    s32 mode;
                    /* test >=3 first and pass the shared 3 via mode so gcc materializes a0=3 once */
                    if (D_801D30FC == 2) mode = 4;
                    else if (D_801A2C70[D_801D30FC] >= 3) mode = 3;
                    else { mode = 3; func_800A247C(mode); mode = 2; }
                    D_801D3018 = func_8009EB30(mode);
                    ctl->counter++;
                }
                if (D_801C2EC4 & PAD_UP) {
                    if (D_801D3018 != 0) { func_8009EB90(D_801D3018, 1); D_801D3018 = 0; }
                    return 0;
                }
                if (D_801C2EC4 == 0) return 0;
                func_800A2054(3);
                if (D_801D30FC == 2 && (g_tripleTriadRules & TT_RULE_SUDDEN_DEATH)) {
                    if (D_801D3018 != 0) func_8009EB90(D_801D3018, 1);
                    func_8009953C();
                    D_801D30F8 ^= 1;
                    ctl->state = 1;
                } else {
                    ctl->state = 9;
                }
                ctl->counter = 0;
                break;
            }
            case 9: {
                if (ctl->counter == 0) func_800A0370(TT_HOLD_FRAMES_FADE);
                ctl->counter++;
                if ((u8)ctl->counter < TT_HOLD_FRAMES_FADE) return 0;
                if (D_801D3018 != 0) func_8009EB90(D_801D3018, 1);
                {
                    TripleTriadData *inv = getTripleTriadData();
                    if (D_801D30FC == 2) {
                        D_80082C9C = D_801D30FC;
                        TT_TALLY(inv->draws);
                    } else if (D_801A2C70[D_801D30FC] == 3) {
                        D_80082C9C = 1;
                        TT_TALLY(inv->defeats);
                    } else {
                        D_80082C9C = 0;
                        TT_TALLY(inv->victories);
                    }
                    D_801A2CE6 = 4;
                }
                return 0;
            }
        }
    }
}

/**
 * @brief Execute cleanup via processCardObjects and return success.
 *
 * Calls processCardObjects to perform cleanup/finalization, then always
 * returns 0 to indicate success.
 *
 * @param a0 Entity or context pointer passed to processCardObjects.
 * @return Always 0.
 */
s32 func_8009A2F4(s32 a0) {
    processCardObjects(a0);
    return 0;
}

/**
 * @brief Triple Triad: emit per-cell capture-direction marker sprites and flip
 *        the back-buffer's draw-env entry.
 *
 * Per-frame renderer that walks the 3x3 active play area (rows/cols 1..3).
 * Each board slot carries an @c element bitmask whose set bits select the
 * element icon(s) to render at that cell.
 * The function finds the lowest set bit of @c element, packs a 6-word combined
 * @c DR_TPAGE + @c SPRT primitive (24 bytes) into the active primitive pool
 * via @c D_801C2EB4, and links it into the OT at @c D_801C2EB0[0x1B] (the
 * 0x6C byte slot in the sort tree) using @c AddPrim. The U/V coordinates are
 * computed from the bit position: @c U = ((bit&3)<<6) + ((frameTick<<2)&0x30)
 * (animated by @c D_801A2C6C frame counter modulo 4), @c V = (bit>>2)<<4 + 0x10.
 * Cell pixel positions step by 0x40 in both axes (cell size).
 *
 * After all visible cells emit, @c D_801C2EB4 is bumped to the new tail and
 * @c func_80098A1C is called with @c &D_801C2DD0[!activeBuffer] (the OTHER
 * draw-env) plus @c D_8012E66C as the callback — registering the back-buffer
 * for the next vblank flip.
 *
 * @return Always @c 0.
 *
 * @note Matching tricks: (1) @c TripleTriadCellPrim is a 6-word combined
 *       primitive (P_TAG + DR_TPAGE + SPRT). (2) The bit-scan loop uses a
 *       redundant @c saved = colByteOffset[boardBase+5] re-load (instead of
 *       @c saved = mask) — this re-emits the @c addu for the cell address,
 *       letting gcc allocate @c v1 (matching target) instead of reusing
 *       @c v0. (3) @c v = saved >> bit at the top of the while-loop body
 *       AND in the tail (line 76 below is redundant after the bottom-of-body
 *       shift, but the compiler emits a srav for it in the back-edge delay
 *       slot for the next iter's check). (4) @c volatile s32 D_801A2C6C
 *       prevents gcc from narrowing the global load to @c lbu — the target
 *       uses a 32-bit @c lw. (5) @c i[arr] form @c colByteOffset[boardBase+5]
 *       forces the @c addu operand order @c (s2, s8) matching target.
 *       (6) @c row=1 / @c col=1 in declaration init (not the @c for header)
 *       fixes the s-reg allocation so that @c row→s7 and @c col→s4.
 *       (7) @c rowByteOffset = pixelY chains the init so gcc emits
 *       @c addu s6, s5, $0 (matching target's @c addu s6, s5, zero).
 */
typedef struct {
    /* 0x00 */ u32 tag;        /**< @c P_TAG: len=5, next=0 (six-dword primitive). */
    /* 0x04 */ u32 tpageCmd;   /**< GP0(0xE1) Draw Mode + tpage attribute = @c 0xE100060C. */
    /* 0x08 */ u32 sprtCmd;    /**< GP0(0x66) textured sprite + grey-tint RGB = @c 0x66808080. */
    /* 0x0C */ s16 x0, y0;     /**< Screen-space sprite origin (top-left). */
    /* 0x10 */ u8  u0, v0;     /**< Texture-page UV (bit-position derived). */
    /* 0x12 */ u16 clut;       /**< CLUT id (varies by lowest set bit of @c element). */
    /* 0x14 */ s16 w, h;       /**< Sprite size in pixels (15x15). */
} TripleTriadCellPrim;

s32 func_8009A314(void) {
    TripleTriadCellPrim *prim = (TripleTriadCellPrim *)D_801C2EB4;
    s32 row = 1;
    u8 *boardBase = (u8 *)&D_801D3398;
    s32 pixelY = 0x28;
    s32 rowByteOffset = pixelY;

    for (; row <= 3; row++) {
        s32 col = 1;
        s32 pixelX = 0x78;
        s32 colByteOffset = rowByteOffset + 8;

        for (; col <= 3; col++) {
            u8 mask = colByteOffset[boardBase + 5];

            if (mask != 0) {
                s32 bit = 0;
                s32 saved = colByteOffset[boardBase + 5];
                s32 v;

                while (1) {
                    v = saved >> bit;
                    if (v & 1) break;
                    bit++;
                    if (bit >= 8) break;
                    v = saved >> bit;
                }

                prim->tag      = 0x05000000;
                prim->tpageCmd = 0xE100060C;
                prim->clut     = (bit + 0xE1) << 6;
                prim->sprtCmd  = 0x66808080;
                prim->x0       = pixelX;
                prim->y0       = pixelY;
                prim->u0       = ((bit % 4) << 6) + ((D_801A2C6C << 2) & 0x30);
                prim->v0       = ((bit / 4) << 4) + 0x10;
                prim->h        = 0xF;
                prim->w        = 0xF;

                AddPrim((s32 *)&D_801C2EB0[0x1B], prim);
                prim++;
            }

            pixelX += 0x40;
            colByteOffset += 8;
        }

        pixelY += 0x40;
        rowByteOffset += 0x28;
    }

    D_801C2EB4 = prim;
    func_80098A1C((u8 *)&D_801C2DD0[D_801C2DCA ^ 1], D_8012E66C);
    return 0;
}

/**
 * @brief Query the list at @p node->listPtr; return 2 if empty, 0 otherwise.
 *
 * Calls @c func_80098D28 on the node's @c listPtr (a list-head pointer at
 * offset 0x0C) and returns @c 2 when that returns 0 (empty list), else 0.
 */
s32 func_8009A4E0(CallbackNode *node) {
    return (func_80098D28(node->listPtr) == 0) << 1;
}

/**
 * @brief Triple Triad: tally per-player card ownership and render the two
 *        score-digit sprites for the end-of-game tally screen.
 *
 * Walks all 10 @c TripleTriadCardObject slots in @c g_tripleTriadCardHands and bins each by the
 * low bit of @c initFlags (owner: player 0 or player 1) into a 2-element
 * stack-local @c cnt[]. Then emits two 6-word combined @c DR_TPAGE + @c SPRT
 * primitives — one per player — placed at fixed screen positions
 * (player 0 at @c x=0x28, player 1 at @c x=0x140; both at @c y=0xC0). The
 * @c u0 coordinate selects a digit glyph from a tex-page strip via the
 * formula @c u0 = (cnt[p] * 24) - 24 = (cnt[p] - 1) * 24 (each glyph is
 * 24 pixels wide). Sprites are 24x24, CLUT @c 0x3A40, @c v0 = 0x30.
 *
 * @return Always @c 0.
 *
 * @note Matching tricks: (1) @c cnt[1] = 0 then @c cnt[0] = 0 (reversed
 *       init order) matches the target's @c sw zero sequence — same trick
 *       used in @c func_80099C78's case 7. (2) The branch form
 *       @c if (!(i & 1)) ... else ... — with the @c i==0 (player 0) path
 *       in the @c if body — drives gcc to emit @c bnez to the player-1
 *       (@c 0x140) branch and fall through (in delay slot) to the
 *       player-0 (@c 0x28) value, matching target's @c bnez+j layout.
 *       The @c & 1 mask is reused for both the @c x0 branch and the
 *       @c cnt[i & 1] lookup so gcc CSEs the @c andi.
 */
s32 func_8009A508(void) {
    s32 i = 0;
    s32 cnt[2];
    TripleTriadCellPrim *prim;

    cnt[1] = 0;
    cnt[0] = 0;

    for (; i < 10; i++) {
        cnt[g_tripleTriadCardHands[i].initFlags & 1]++;
    }

    prim = (TripleTriadCellPrim *)D_801C2EB4;

    for (i = 0; i < 2; i++) {
        prim->tag      = 0x05000000;
        prim->tpageCmd = 0xE100060C;
        prim->sprtCmd  = 0x64808080;
        if (!(i & 1)) prim->x0 = 0x28;
        else          prim->x0 = 0x140;
        prim->y0       = 0xC0;
        prim->u0       = (cnt[i & 1] * 3 * 8) - 0x18;
        prim->v0       = 0x30;
        prim->clut     = 0x3A40;
        prim->h        = 0x18;
        prim->w        = 0x18;

        AddPrim((s32 *)&D_801C2EB0[4], prim);
        prim++;
    }

    D_801C2EB4 = prim;
    return 0;
}

/**
 * @brief Initialize the D_801D3028 linked list with battle update callbacks.
 *
 * Calls resetTriadBoard for initial setup, then initializes D_801D3028
 * as a linked list (pool at D_801D3038, node size 0x18, capacity 8).
 * Registers four callback functions as nodes. Clears fields on the
 * first callback's node. Finally calls setupTripleTriadHands and returns
 * the list pointer.
 *
 * @return Pointer to D_801D3028 list header.
 */
u8 *func_8009A650(void) {
    u8 *list;
    u8 *node;
    resetTriadBoard();
    list = D_801D3028;
    func_80098BC0(list, D_801D3038, 0x18, 8);
    node = (u8 *)func_80098C44(list, (s32)func_80099C78);
    node[0x10] = 0;
    node[0x11] = 0;
    node[0x14] = 0;
    func_80098C44(list, (s32)func_8009A2F4);
    func_80098C44(list, (s32)func_8009A314);
    func_80098C44(list, (s32)func_8009A508);
    setupTripleTriadHands();
    return list;
}

/**
 * Sets up animation rectangle parameters based on the entity type.
 *
 * Configures position and size values in a 4-halfword output structure
 * based on the type byte at a0[0]. Type 0 and 1 use vertical strips,
 * type 2 uses a tile grid layout.
 *
 * @param a0 Pointer to entity data (byte 0 = type, byte 1 = column, byte 2 = row).
 * @param a1 Pointer to output rectangle (4 s16 values: x, y, w, h).
 * @return Pointer to the output rectangle, or a1 unchanged if type is unknown.
 */
u8 *func_8009A6EC(u8 *a0, s16 *a1) {
    u8 type = a0[0];

    switch (type) {
    case 0:
        a1[0] = -0x8C;
        {
            u8 r = a0[2];
            s32 w = 0x200;
            a1[2] = w;
            a1[1] = r * 32 - 0x40;
        }
        a1[3] = -(s32)a0[2] + 0xE;
        break;
    case 1:
        a1[0] = 0x8C;
        {
            u8 r = a0[2];
            s32 w = 0x200;
            a1[2] = w;
            a1[1] = r * 32 - 0x40;
        }
        a1[3] = -(s32)a0[2] + 0xE;
        break;
    case 2: {
        s32 col = a0[1];
        a1[0] = (col - 1) * 64;
        {
            u8 row = a0[2];
            a1[2] = 0x200;
            a1[3] = 0x12;
            a1[1] = (row - 1) * 64;
        }
        break;
    }
    default:
        return (u8 *)a1;
    }
    return (u8 *)a1;
}

/**
 * @brief Find a battle-object slot in @c g_tripleTriadCardHands matching a search key.
 *
 * Three search modes are dispatched off @p groupId:
 *  - @c groupId @c <0: invalid — returns @c -1.
 *  - @c groupId @c 0 or @c 1: scan the 10 slots for an @b active entry
 *    (@c flags @c & @c 1) whose @c groupId matches and whose @c priority
 *    matches @p priority. @p fieldD is ignored.
 *  - @c groupId @c ==2: scan for an entry (active or not) whose
 *    @c groupId is @c 2, @c fieldD matches @p fieldD, and @c priority
 *    matches @p priority.
 *  - @c groupId @c >2: invalid — returns @c -1.
 *
 * @param groupId  Search-mode selector and target @c TripleTriadCardObject.groupId.
 * @param fieldD   Target @c TripleTriadCardObject.fieldD (only used in mode 2).
 * @param priority Target @c TripleTriadCardObject.priority.
 * @return Slot index @c 0..9 of the first matching entry, or @c -1 if
 *         none found / invalid mode.
 */
s32 func_8009A7A4(s32 groupId, s32 fieldD, s32 priority) {
    s32 i;

    if (groupId < 0) return -1;

    switch (groupId) {
    case 0:
    case 1:
        for (i = 0; i < 10; i++) {
            if (g_tripleTriadCardHands[i].flags & 1) {
                if (g_tripleTriadCardHands[i].groupId == groupId) {
                    if (g_tripleTriadCardHands[i].priority == priority) {
                        return i;
                    }
                }
            }
        }
        return -1;

    case 2:
        for (i = 0; i < 10; i++) {
            if (g_tripleTriadCardHands[i].groupId == groupId) {
                if (g_tripleTriadCardHands[i].fieldD == fieldD) {
                    if (g_tripleTriadCardHands[i].priority == priority) {
                        return i;
                    }
                }
            }
        }
        return -1;
    }

    return -1;
}

/**
 * @brief Mark a battle entity's flags with bit 2.
 *
 * Looks up an entity index via func_8009A7A4, then sets bit 1 (0x2)
 * in the flags halfword at offset +4 of the entity's 36-byte entry
 * in g_tripleTriadCardHands.
 *
 * @param a0 Entity search key passed to func_8009A7A4.
 * @param a1 Secondary parameter passed as third arg to func_8009A7A4.
 */
void func_8009A878(s32 a0, s32 a1) {
    s32 idx = func_8009A7A4(a0, 0, a1);
    if (idx >= 0) {
        g_tripleTriadCardHands[idx].flags |= 2;
    }
}
