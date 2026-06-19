#include "common.h"
#include "battle.h"
#include "tripletriad/be_object3.h"

/* Card-claim flow globals (defined in the tripletriad data segments). */
extern s32 D_801D4448;   /**< Claim selector: >=0 normal, -1 capture-only, <-1 skip. */
extern u8  D_801D444C;   /**< Set when the phase-1 claim handler finishes. */
extern u8  D_801D444D;   /**< Set when the phase-2 cleanup handler finishes. */
extern u8  D_801A2C44;   /**< Rule/mode selector for the post-game card-claim flow. */
extern u8  D_801D30FC;   /**< Seat whose claim is being resolved (2 = none). */
extern s32 D_801D4450;   /**< Acting seat index (0 or 1) for the capture/cleanup sweeps. */
extern u8  D_801D42F8[]; /**< Card-claim handler object pool. */
extern u8  D_801D42A8[]; /**< Backing element storage for the D_801D42F8 pool. */
extern s32 D_801D4454;   /**< Cleared at the start of each card-claim setup. */

/* Fade spawners and claim handlers (defined in be_object3.c). */
extern void func_800A030C(s32 duration);
extern void func_800A0370(s32 duration);
extern void func_800A2054(s32 a0);
extern s32  func_800A20F4(s32 a0);
extern s32  func_800A0B24();
extern s32  func_800A0F0C();
extern s32  func_800A1080();
extern s32  func_800A1260();
extern s32  func_800A1374();
extern s32  func_800A03DC();  /**< Per-frame board render/update loop (be_object3.c). */
extern s32  func_800A0AD4();  /**< Post-claim handler (be_object3.c). */

/* Pool/item helpers defined in other tripletriad TUs. */
extern void markItemPresent(s32 cardId);
extern void initObjList(u8 *list, u8 *pool, s32 nodeSize, s32 count);

/**
 * @brief Per-frame state node for the card-claim transition controller (@c func_800A15C8).
 *
 * Allocated from a @c allocObjNode pool. @c state at 0x0C selects the phase and
 * @c result at 0x10 carries the outcome code (2 or 6) staged into @c g_tripleTriadState
 * when the whole claim sequence finishes.
 */
typedef struct {
    /* 0x00 */ u8  pad00[0x0C];
    /* 0x0C */ u8  state;     /**< Phase: 0 warmup, 1/2 spawn handlers, 3 poll gate, 4 fade-out. */
    /* 0x0D */ u8  subState;  /**< Per-phase frame counter / sub-step. */
    /* 0x0E */ u8  pad0E[2];
    /* 0x10 */ s32 result;    /**< Outcome code (2 or 6) staged into g_tripleTriadState on completion. */
} ClaimCtrlNode;

/**
 * @brief Per-frame card-claim transition controller.
 *
 * Drives the post-game card-claim sequence as a five-phase state machine on
 * @c node->state:
 *  - 0: fade to black (@c func_800A030C), wait 0xF frames, then branch on the
 *       claim selector @c D_801D4448 to phase 1 (>= 0), 2 (== -1), or 3 (< -1).
 *  - 1: spawn the claim handler chosen by the rule/mode selector @c D_801A2C44
 *       (@c func_800A1080 / @c func_800A0B24 / @c func_800A0F0C / @c func_800A1260)
 *       onto the @c D_801D42F8 pool for seat @c D_801D30FC; wait for it to signal
 *       via @c D_801D444C, then advance to phase 2. Modes 0 and >4 spawn nothing.
 *  - 2: spawn the capture/cleanup handler @c func_800A1374 for acting seat
 *       @c D_801D4450; wait for @c D_801D444D, then advance to phase 3.
 *  - 3: poll the message gate @c func_800A20F4(6); stage result 6 (init / accept)
 *       or 2 (gate result 0), or keep waiting (gate < 0). Advance to phase 4.
 *  - 4: fade to white (@c func_800A0370), wait 0xF frames, stage @c result into
 *       @c g_tripleTriadState and return 2.
 *
 * @param node Controller state node.
 * @return 0 while the sequence is still running, 2 once it has finished.
 */
s32 func_800A15C8(ClaimCtrlNode *node)
{
    ScriptStateNode *spawned;
    s32 mode;
    s32 r;
    u8 ss;
    u8 seat;
    u8 actingSeat;
    u8 nextState;
    short modeMax;

    while (1) {
        switch (node->state) {
        case 0:
            if (node->subState == 0) {
                func_800A030C(0xF);
            }
            node->subState++;
            if ((node->subState & 0xFF) < 0xF) {
                return 0;
            }
            if (D_801D4448 >= 0) {
                nextState = 1;
            } else if (D_801D4448 == -1) {
                nextState = 2;
            } else {
                node->state = 3;
                node->subState = 0;
                break;
            }
            node->state = nextState;
            node->subState = 0;
            break;
        case 1:
            if (node->subState == 0) {
                mode = D_801A2C44;
                modeMax = 4;
                if (mode == 3) {
                    goto spawn1080;
                }
                if (mode >= modeMax) {
                    if (mode == 4) {
                        goto spawn1260;
                    }
                    goto fieldSetup;
                }
                if (mode == 0) {
                    goto fieldSetup;
                }
                if (D_801D4448 < 5) {
                    if (D_801A2C70[D_801D30FC] < 3) {
                        spawned = (ScriptStateNode *)allocObjNode(D_801D42F8, (s32)func_800A0B24);
                    } else {
                        spawned = (ScriptStateNode *)allocObjNode(D_801D42F8, (s32)func_800A0F0C);
                    }
                    goto fieldSetup;
                }
                goto spawn1260;
            spawn1080:
                spawned = (ScriptStateNode *)allocObjNode(D_801D42F8, (s32)func_800A1080);
                goto fieldSetup;
            spawn1260:
                spawned = (ScriptStateNode *)allocObjNode(D_801D42F8, (s32)func_800A1260);
            fieldSetup:
                seat = D_801D30FC;
                spawned->state = 0;
                spawned->field0D = 0;
                spawned->field0E = seat;
                D_801D444C = 0;
                node->subState++;
            }
            if (D_801D444C == 0) {
                return 0;
            }
            node->state = 2;
            node->subState = 0;
            break;
        case 2:
            if (node->subState == 0) {
                spawned = (ScriptStateNode *)allocObjNode(D_801D42F8, (s32)func_800A1374);
                actingSeat = D_801D4450;
                spawned->state = 0;
                spawned->field0D = 0;
                spawned->field0E = actingSeat;
                D_801D444D = 0;
                node->subState++;
            }
            if (D_801D444D == 0) {
                return 0;
            }
            node->state = 3;
            node->subState = 0;
            break;
        case 3:
            if (node->subState == 0) {
                node->result = 6;
            } else {
                r = func_800A20F4(6);
                switch (r) {
                case 0:
                    func_800A2054(6);
                    node->result = 2;
                    break;
                case 1:
                    func_800A2054(6);
                    break;
                default:
                    return 0;
                }
            }
            node->state = 4;
            node->subState = 0;
            break;
        case 4:
            if (node->subState == 0) {
                func_800A0370(0xF);
            }
            ss = node->subState;
            node->subState = ss + 1;
            if (ss < 0xF) {
                return 0;
            }
            g_tripleTriadState = node->result;
            return 2;
        }
    }
}

/**
 * @brief Card-claim setup: choose the claim amount and build the hand display.
 *
 * Run once when the post-game card-claim flow starts. It:
 *  1. Clears @c D_801D4454 and picks the acting seat @c D_801D4450 (the player
 *     whose hand uses the non-offset layout, @c D_801A2C70[seat] < 3).
 *  2. Computes the claim selector @c D_801D4448 (default -1 = capture-only). Unless
 *     the "no claim" rule bit (0x20000000) is set, the rule/mode @c D_801A2C44 maps to:
 *       1 → claim one card; 2 → claim 2 per owned card on the board (offset from -10);
 *       3 → claim none (0); 4 → claim five. Seat 2 (none) leaves it at -1.
 *  3. If @c D_801D4448 < 0, returns every card in the acting seat's hand to its owner
 *     (@c markItemPresent), stages result 6 into @c g_tripleTriadState, and returns 0.
 *  4. Otherwise initializes the @c D_801D42F8 handler pool, spawns the claim controller
 *     @c func_800A15C8, fills the @c D_801D4308 display-object array with both five-card
 *     hands (positions/sort keys mirrored per seat), spawns the board renderer
 *     @c func_800A03DC and @c func_800A0AD4, and returns the pool.
 *
 * @return 0 on the no-claim path, otherwise the @c D_801D42F8 pool pointer.
 */
s32 setupTripleTriadCardClaim(void)
{
    s32 i;
    s32 col;
    s32 xpos;
    s32 scratch[2];   /* reserves the original 8-byte stack slot (frame is 0x28); purpose unclear */
    ActiveObj *cell;
    ScriptStateNode *node;

    D_801D4454 = 0;
    if (D_801A2C70[0] < 3) {
        D_801D4450 = 0;
    } else if (D_801A2C70[1] < 3) {
        D_801D4450 = 1;
    }
    D_801D4448 = -1;
    if (!(g_tripleTriadRules & 0x20000000)) {
        switch (D_801A2C44) {
        case 0:
            break;
        case 1:
            if (D_801D30FC != 2) {
                D_801D4448 = 1;
            }
            break;
        case 2:
            if (D_801D30FC != 2) {
                D_801D4448 = -10;
                for (i = 0; i < 10; i++) {
                    if ((g_tripleTriadCardHands[i].initFlags & 1) == D_801D30FC) {
                        D_801D4448 += 2;
                    }
                }
            }
            break;
        case 3:
            D_801D4448 = 0;
            break;
        case 4:
            if (D_801D30FC != 2) {
                D_801D4448 = 5;
            }
            break;
        }
    }
    if (D_801D4448 < 0) {
        for (i = 0; i < 5; i++) {
            markItemPresent(D_801A2C48[D_801D4450][i]);
        }
        g_tripleTriadState = TT_STATE_EXIT;
        return 0;
    }
    initObjList(D_801D42F8, D_801D42A8, 0x14, 4);
    node = (ScriptStateNode *)allocObjNode(D_801D42F8, (s32)func_800A15C8);
    cell = D_801D4308;
    node->state = 0;
    node->field0D = 0;
    for (i = 0; i < 2; i++) {
        for (col = 0, xpos = -0x80; col < 5; col++) {
            cell->cardId = D_801A2C48[i][col];
            cell->flags = 1;
            cell->rotX = 0;
            cell->rotY = 0;
            cell->rotZ = 0;
            cell->baseX = xpos;
            if (i != 0) {
                cell->baseY = 0x28;
            } else {
                cell->baseY = -0x28;
            }
            cell->baseZ = 0x200;
            if (i == 0) {
                cell->sort = col + 5;
            } else {
                cell->sort = 9 - col;
            }
            cell->fieldA = i;
            cell->field8 = 1;
            cell->field9 = 0;
            cell++;
            xpos += 0x40;
        }
    }
    allocObjNode(D_801D42F8, (s32)func_800A03DC);
    allocObjNode(D_801D42F8, (s32)func_800A0AD4);
    return (s32)D_801D42F8;
}

/**
 * @brief Unconditional hang.
 *
 * @note Not referenced by the @c g_tripleTriadStateHandlers battle-state table or any caller —
 *       appears to be an unused/stub state that simply spins forever.
 */
void hangForever(void) {
    while (1) {}
}

/**
 * @brief Battle state-5 handler (@c g_tripleTriadStateHandlers[5]).
 *
 * Sets the next state to @c TT_STATE_SCRIPT and returns 0 so the
 * state-dispatch loop keeps running.
 */
s32 tripleTriadRestartScript(void) {
    g_tripleTriadState = TT_STATE_SCRIPT;
    return 0;
}
