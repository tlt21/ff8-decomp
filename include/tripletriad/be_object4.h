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

/* Message-gate / banner + hand-build UI helpers (used by be_object3 / be_object3b). */
extern void func_800A1D68(s32 a0, u8 *a1, s32 a2);  /**< Show a banner/message string. */
extern void func_800A2054(s32 a0);                  /**< Acknowledge/advance a message gate. */
extern void func_800A44CC(void);   /**< Reset the hand-build UI state for a new claim sequence. */
extern void func_800A44B0(s32 a0); /**< Enable (1) / disable (0) the hand-build input prompt. */
extern void func_800A44BC(void);   /**< Tear down the claim UI at the end of the sequence. */

#endif /* TRIPLETRIAD_BE_OBJECT4_H */
