#ifndef TRIPLETRIAD_BE_OBJECT1_H
#define TRIPLETRIAD_BE_OBJECT1_H

#include "common.h"
#include "tripletriad.h"

/* Declarations exported by be_object1.c.
   Populated as functions/data are decompiled — move the file-scope externs,
   typedefs, and prototypes out of be_object1.c into here as you go. */

/** @brief Battle-state handler node: sub-state selector, frame counter, and
 *  phase bit (which side the card is showing). Shared between the card-flip
 *  handler (be_object1.c) and the match-flow driver (be_object1b.c). */
typedef struct {
    u8    pad00[0x0C];
    void *subHandler;  /* 0x0C — spawned per-turn sub-handler node           */
    u8    state;       /* 0x10 — sub-state                                   */
    u8    counter;     /* 0x11 — per-state frame counter                     */
    u8    phase;       /* 0x12 — flip phase / card side                      */
    u8    rulesFlags;  /* 0x13 — applyCardRules result carried across frames */
    u8    pad14;       /* 0x14 */
    u8    retryFlag;   /* 0x15 — set when a rule pass triggered captures     */
    u8    pad16[2];    /* 0x16 */
} HandlerNode;

#endif /* TRIPLETRIAD_BE_OBJECT1_H */
