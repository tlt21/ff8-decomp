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
