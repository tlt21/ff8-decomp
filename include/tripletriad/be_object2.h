#ifndef TRIPLETRIAD_BE_OBJECT2_H
#define TRIPLETRIAD_BE_OBJECT2_H

#include "common.h"
#include "tripletriad.h"

/* Declarations exported by be_object2.c.
   Populated as functions/data are decompiled — move the file-scope externs,
   typedefs, and prototypes out of be_object2.c into here as you go. */

/**
 * @brief Capture-direction descriptor — maps a captured-from direction to a
 *        card slide-in animation.
 *
 * @c D_80182D54 holds four entries (captured-from UP, LEFT, RIGHT, DOWN). For a
 * cell captured this turn, @c resolveCaptures matches the cell's
 * @c TT_CELL_CAP_FROM_* bit against @c capBit, then plays the matching
 * @c animType slide animation via @c setCardEntityType.
 */
typedef struct {
    s16 capBit;    /* 0x00 — matching TT_CELL_CAP_FROM_* bit */
    s16 animType;  /* 0x02 — CardEffectState (a CARD_FX_SLIDE_* direction) for setCardEntityType */
} CaptureDir;      /* 0x04 */

extern CaptureDir D_80182D54[4];

/** @brief Set a card object's effect-animation @c state (a @ref CardEffectState);
 *         the @c CARD_FX_SLIDE_* captures also trigger a flip SFX.
 *         @c entityIdx indexes @c g_tripleTriadCardHands. */
extern void setCardEntityType(s32 entityIdx, s32 type);

/** @brief Resolve captured cells: trigger each captured cell's flip animation
 *         and clear its captured-from bits. */
extern void resolveCaptures(TripleTriadBoard *board);

/* Card-object processing + per-match board/hand setup. */
extern void processCardObjects(s32 arg);
extern void resetTriadBoard(void);
extern void setupTripleTriadHands(void);

#endif /* TRIPLETRIAD_BE_OBJECT2_H */
