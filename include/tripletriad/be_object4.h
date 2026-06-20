#ifndef TRIPLETRIAD_BE_OBJECT4_H
#define TRIPLETRIAD_BE_OBJECT4_H

#include "common.h"
#include "tripletriad.h"

/* Declarations exported by be_object4.c.
   Populated as functions/data are decompiled — move the file-scope externs,
   typedefs, and prototypes out of be_object4.c into here as you go. */

/* Public prototypes */
extern void func_800A233C(s32 a);     /**< Play / queue a Triple Triad sound effect. */
extern void closeMenu(void);
extern void func_800A1C6C(void);
extern void func_800A2214(void);
extern void clearAllSfx(void);
/** @brief Show a card's name, or build its detail popup buffer. */
extern void func_800A2114(s32 cardId);
/** @brief Card-claim helper. */
extern void func_800A26C8(void);

#endif /* TRIPLETRIAD_BE_OBJECT4_H */
