#ifndef TRIPLETRIAD_BE_OBJECT1B_H
#define TRIPLETRIAD_BE_OBJECT1B_H

#include "common.h"

/* Declarations for be_object1b.c (Triple Triad match-flow controller, the
   per-frame update-list callbacks, and battle-object search helpers). */

/** @brief Six-word combined DR_TPAGE + SPRT primitive (24 bytes) used by the
 *  cell-marker and score-tally renderers. */
typedef struct {
    /* 0x00 */ u32 tag;        /**< P_TAG: len=5, next=0 (six-dword primitive). */
    /* 0x04 */ u32 tpageCmd;   /**< GP0(0xE1) Draw Mode + tpage attribute. */
    /* 0x08 */ u32 sprtCmd;    /**< GP0(0x66) textured sprite + tint RGB. */
    /* 0x0C */ s16 x0, y0;     /**< Screen-space sprite origin (top-left). */
    /* 0x10 */ u8  u0, v0;     /**< Texture-page UV. */
    /* 0x12 */ u16 clut;       /**< CLUT id. */
    /* 0x14 */ s16 w, h;       /**< Sprite size in pixels. */
} TripleTriadCellPrim;

/** @brief @c HandlerNode.state values for the match-flow controller
 *  (@ref matchFlowHandler). States 2 and 3 are unused. */
typedef enum {
    MATCH_FLOW_INIT     = 0,  /**< Set up the match; pick the starting player.   */
    MATCH_FLOW_TURN     = 1,  /**< A player or the AI places a card.             */
    MATCH_FLOW_RULES    = 4,  /**< Evaluate Same/Plus and animate the captures.  */
    MATCH_FLOW_CHAIN    = 5,  /**< Cascade further capture chains until settled. */
    MATCH_FLOW_TURN_END = 6,  /**< Board full -> tally; else next player's turn. */
    MATCH_FLOW_TALLY    = 7,  /**< Count each side's cards and pick the winner.  */
    MATCH_FLOW_RESULT   = 8,  /**< Play the result jingle; wait for input.       */
    MATCH_FLOW_FADE     = 9,  /**< Fade out, record the result, exit the match.  */
} MatchFlowState;

/* ── Functions defined in be_object1b.c ──────────────────────────────────── */

/** @brief Build the per-frame update list and return its head. */
extern u8 *initTripleTriadUpdateList(void);

/** @brief Fill an animation RECT from an entity descriptor (type/col/row). */
extern u8 *layoutCardSlot(u8 *src, s16 *dst);

/** @brief Find the @c g_tripleTriadCardHands slot matching a search key, or -1. */
extern s32 findCardSlot(s32 groupId, s32 fieldD, s32 priority);

/** @brief Flag the matching card object (sets bit 1 of its @c flags). */
extern void highlightCardSlot(s32 groupId, s32 priority);

#endif /* TRIPLETRIAD_BE_OBJECT1B_H */
