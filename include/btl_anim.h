#ifndef BTL_ANIM_H
#define BTL_ANIM_H

#include "common.h"

/* Battle display-list render helpers (btl_anim.c). */

/* Public prototypes */
extern void renderAndUpdateDisplay(s32 frameCount); /**< Advance and render the battle display list. */
extern s32  renderBattleDisplayList(s32 *colorTag); /**< Walk the ordering table and emit its primitives. */

#endif /* BTL_ANIM_H */
