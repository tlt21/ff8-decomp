#ifndef TRIPLETRIAD_BE_OBJECT4_H
#define TRIPLETRIAD_BE_OBJECT4_H

#include "common.h"
#include "tripletriad.h"

/* Declarations exported by be_object4.c.
   Populated as functions/data are decompiled — move the file-scope externs,
   typedefs, and prototypes out of be_object4.c into here as you go. */

/* Public prototypes */
/** @brief Queue a Triple Triad sound effect (center pan, full volume). */
extern void playTriadSfx(s32 sfxId);
/** @brief Queue a Triple Triad sound effect with a per-effect @p param. */
extern void playTriadSfxParam(s32 sfxId, s32 param);
extern void closeMenu(void);
extern void func_800A1C6C(void);
extern void func_800A2214(void);
extern void clearAllSfx(void);
/** @brief Show a card's name, or build its detail popup buffer. */
extern void showCardDetail(s32 cardId);
/** @brief Open the Triple Triad in-game menu and freeze card input. */
extern void openTriadMenu(void);

#endif /* TRIPLETRIAD_BE_OBJECT4_H */
