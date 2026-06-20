#ifndef TRIPLETRIAD_BE_OBJECT3B_H
#define TRIPLETRIAD_BE_OBJECT3B_H

#include "common.h"
#include "tripletriad.h"

/* Declarations for be_object3b.c (the post-game card-claim transition
   controller and its setup / restart entry points). */

/* ───────────────────────────── Public ──────────────────────────────────── */

/* Public prototypes — Triple Triad state-handler entry points (dispatched by
   address from @c g_tripleTriadStateHandlers). */
extern s32  setupTripleTriadCardClaim(void);
extern s32  tripleTriadRestartScript(void);
extern void hangForever(void);

/* ───────── Private (only used in be_object3b.c; may move into the .c) ────── */

/* Private typedefs */

/**
 * @brief Per-frame state node for the card-claim transition controller.
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

/* Private data */
extern u8      D_801A2C44;    /**< Rule/mode selector for the post-game card-claim flow. */
extern ObjList D_801D42F8[];  /**< Card-claim handler object pool. */

#endif /* TRIPLETRIAD_BE_OBJECT3B_H */
