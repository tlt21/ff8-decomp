#include "common.h"
#include "battle.h"
#include "tripletriad.h"
#include "psxsdk/libc.h"
#include "psxsdk/libgpu.h"
#include "tripletriad/be_object1.h"
#include "tripletriad/be_object1b.h"
#include "tripletriad/be_object2.h"
#include "gamestate.h"

/** @brief Bump a Triple Triad save-record counter. The do/while is a scheduling
 *  barrier the original needs — plain @c counter++ lets gcc reorder the load. */
#define TT_TALLY(counter) do { (counter)++; } while (0)

/**
 * @brief Per-frame Triple Triad match controller (an update-list callback).
 *
 * Runs the post-game flow as a state machine over @c ctl->state (see
 * @ref MatchFlowState): each call advances until it must wait on an async
 * event (input, a sub-handler, or an animation), then returns 0 so the next
 * frame re-enters. A state of 10 or more hangs intentionally.
 *
 * @param ctl Handler node (state/counter/sub-handler/rule flags in its fields).
 * @return Always 0.
 */
s32 matchFlowHandler(HandlerNode *ctl) {
    s32 acc;
    while (1) {
        switch (ctl->state) {
            case MATCH_FLOW_INIT: {
                if (ctl->counter == 0) {
                    HandlerNode *sub = (HandlerNode *)allocObjNode(D_801D3028, (s32)cardFlipHandler);
                    sub->state = CARD_FLIP_INIT;
                    sub->counter = 0;
                    g_cardFlipPhase = -1;
                    D_801D3340[1].field2 = 0;
                    D_801D3340[2].field2 = 0;
                    ctl->counter++;
                }
                if (g_cardFlipPhase < 0) return 0;
                ctl->state = MATCH_FLOW_TURN;
                ctl->counter = 0;
                break;
            }
            case MATCH_FLOW_TURN: {
                if (ctl->counter == 0) {
                    u8 playerType = D_801A2C70[g_cardFlipPhase];
                    switch (playerType) {
                        case 0: ctl->subHandler = (void *)spawnCardSelectCursor(g_cardFlipPhase, 0); break;
                        case 1: ctl->subHandler = (void *)spawnCardSelectCursor(g_cardFlipPhase, 1); break;
                        case 2: ctl->subHandler = (void *)spawnCardSelectCursor(g_cardFlipPhase, 2); break;
                        case 3: ctl->subHandler = (void *)spawnAiTurn(g_cardFlipPhase); break;
                    }
                    ctl->counter++;
                    return 0;
                }
                if (updateObjectList(ctl->subHandler) == 0) {
                    ctl->state = MATCH_FLOW_RULES;
                    ctl->counter = 0;
                    break;
                }
                return 0;
            }
            case MATCH_FLOW_RULES: {
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
                    ctl->state = MATCH_FLOW_CHAIN;
                    ctl->counter = 0;
                }
                break;
            }
            case MATCH_FLOW_CHAIN: {
                if (anyCardEffectActive()) return 0;
                if (ctl->rulesFlags == 0) {
                    ctl->state = MATCH_FLOW_TURN_END;
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
            case MATCH_FLOW_TURN_END: {
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
                if (row == 0) ctl->state = MATCH_FLOW_TALLY;
                else {
                    g_cardFlipPhase ^= 1;
                    ctl->state = MATCH_FLOW_TURN;
                }
                ctl->counter = 0;
                break;
            }
            case MATCH_FLOW_TALLY: {
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
                ctl->state = MATCH_FLOW_RESULT;
                ctl->counter = 0;
                break;
            }
            case MATCH_FLOW_RESULT: {
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
                    initCardHands();
                    g_cardFlipPhase ^= 1;
                    ctl->state = MATCH_FLOW_TURN;
                } else {
                    ctl->state = MATCH_FLOW_FADE;
                }
                ctl->counter = 0;
                break;
            }
            case MATCH_FLOW_FADE: {
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
                    g_tripleTriadState = TT_STATE_CARD_CLAIM;
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
s32 updateCardObjects(s32 a0) {
    processCardObjects(a0);
    return 0;
}

/**
 * @brief Render each board cell's element icon, then queue the back-buffer flip.
 *
 * Per-frame renderer over the 3x3 play area: for each cell whose @c element
 * bitmask is set, it emits a textured sprite of that element's icon into the
 * primitive pool and links it into the ordering table (the icon's UV animates
 * with @c g_tripleTriadFrameCount). Finishes by queueing the back-buffer's
 * load for the next vblank.
 *
 * @return Always 0.
 */
s32 drawBoardElements(void) {
    TripleTriadCellPrim *prim = (TripleTriadCellPrim *)g_primCursor;
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
                prim->u0       = ((bit % 4) << 6) + ((g_tripleTriadFrameCount << 2) & 0x30);
                prim->v0       = ((bit / 4) << 4) + 0x10;
                prim->h        = 0xF;
                prim->w        = 0xF;

                AddPrim((s32 *)&g_otBase[0x1B], prim);
                prim++;
            }

            pixelX += 0x40;
            colByteOffset += 8;
        }

        pixelY += 0x40;
        rowByteOffset += 0x28;
    }

    g_primCursor = prim;
    queueLoadImage(&g_drawEnvs[g_drawBufferIndex ^ 1].clip, D_8012E66C);
    return 0;
}

/**
 * @brief Query the list at @p node->listPtr; return 2 if empty, 0 otherwise.
 *
 * Calls @c updateObjectList on the node's @c listPtr (a list-head pointer at
 * offset 0x0C) and returns @c 2 when that returns 0 (empty list), else 0.
 */
s32 updateChildList(CallbackNode *node) {
    return (updateObjectList(node->listPtr) == 0) << 1;
}

/**
 * @brief Tally each player's cards and draw the two end-of-game score digits.
 *
 * Counts the 10 @c g_tripleTriadCardHands slots by owner (low bit of
 * @c initFlags), then emits one digit sprite per player at its fixed screen
 * position.
 *
 * @return Always 0.
 */
s32 drawScoreDigits(void) {
    s32 i = 0;
    s32 cnt[2];
    TripleTriadCellPrim *prim;

    cnt[1] = 0;
    cnt[0] = 0;

    for (; i < 10; i++) {
        cnt[g_tripleTriadCardHands[i].initFlags & 1]++;
    }

    prim = (TripleTriadCellPrim *)g_primCursor;

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

        AddPrim((s32 *)&g_otBase[4], prim);
        prim++;
    }

    g_primCursor = prim;
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
u8 *initTripleTriadUpdateList(void) {
    u8 *list;
    u8 *node;
    resetTriadBoard();
    list = D_801D3028;
    initObjList(list, D_801D3038, 0x18, 8);
    node = (u8 *)allocObjNode(list, (s32)matchFlowHandler);
    node[0x10] = 0;
    node[0x11] = 0;
    node[0x14] = 0;
    allocObjNode(list, (s32)updateCardObjects);
    allocObjNode(list, (s32)drawBoardElements);
    allocObjNode(list, (s32)drawScoreDigits);
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
u8 *layoutCardSlot(u8 *a0, s16 *a1) {
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
s32 findCardSlot(s32 groupId, s32 fieldD, s32 priority) {
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
 * Looks up an entity index via findCardSlot, then sets bit 1 (0x2)
 * in the flags halfword at offset +4 of the entity's 36-byte entry
 * in g_tripleTriadCardHands.
 *
 * @param a0 Entity search key passed to findCardSlot.
 * @param a1 Secondary parameter passed as third arg to findCardSlot.
 */
void highlightCardSlot(s32 a0, s32 a1) {
    s32 idx = findCardSlot(a0, 0, a1);
    if (idx >= 0) {
        g_tripleTriadCardHands[idx].flags |= 2;
    }
}
