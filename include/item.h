#ifndef ITEM_H
#define ITEM_H

#include "common.h"

/* Declarations exported by item.c. */

/** @brief PRNG used throughout the game; returns a fresh random word. */
extern s32 func_80023D04(void);

/** @brief Add @p delta to the owned quantity of card/item @p itemId; returns the new count. */
extern s32 modifyItemQuantity(s32 itemId, s32 delta);

#endif /* ITEM_H */
