#ifndef ITEM_H
#define ITEM_H

#include "common.h"

/* Declarations exported by item.c. */

/** @brief PRNG used throughout the game; returns a fresh random word. */
extern s32 func_80023D04(void);

/** @brief Add @p delta to the owned quantity of card/item @p itemId; returns the new count. */
extern s32 modifyItemQuantity(s32 itemId, s32 delta);

/** @brief Owned quantity of card/item @p idx; negative when the index is invalid.
 *  @note Some callers (menuext/menucrd) invoke it with no argument via an implicit
 *        declaration, so this prototype is intentionally kept out of common.h. */
extern s32 func_80023B14(s32 idx);

/** @brief Non-zero if card/item @p cardId is present in the collection. */
extern s32 isItemPresent(s32 cardId);

/** @brief Mark card/item @p cardId as present (return it to the collection). */
extern void markItemPresent(s32 cardId);

#endif /* ITEM_H */
