#ifndef TRIPLETRIAD_BE_OBJECT4_H
#define TRIPLETRIAD_BE_OBJECT4_H

#include "common.h"
#include "tripletriad.h"

/* Declarations exported by be_object4.c.
   Populated as functions/data are decompiled — move the file-scope externs,
   typedefs, and prototypes out of be_object4.c into here as you go. */

/* Triple Triad SFX request queue: queueTriadSfx pushes effects here and
   flushTriadSfxQueue drains them to sndPlaySfx once per frame. */
typedef struct {
    u8  active;   /**< 0x0 — 1 when this slot holds a queued effect. */
    u8  volume;   /**< 0x1 — sndPlaySfx volume. */
    u8  pan;      /**< 0x2 — sndPlaySfx pan. */
    u8  sfxId;    /**< 0x3 — sndPlaySfx sound id. */
    u32 param;    /**< 0x4 — sndPlaySfx addr; also the per-effect voice-channel
                       mask used to drop overlapping requests. */
    u8  pad8[4];  /**< 0x8 */
} SfxQueueEntry;  /* 0xC */

extern SfxQueueEntry D_801D4500[];  /**< SFX request queue (up to 7 pending entries). */
extern s32 D_801D4560;              /**< Number of queued SFX requests. */

/* Public data */
extern u8 g_cardDetailMsg[];    /**< Work buffer for the card-detail popup message (built by showCardDetail). */
extern u8 g_cardDetailSuffix[]; /**< String appended after the card name in the detail message. */

/* Public prototypes */
/** @brief Queue a Triple Triad sound effect (center pan, full volume). */
extern void playTriadSfx(s32 sfxId);
/** @brief Queue a Triple Triad sound effect with a per-effect @p param. */
extern void playTriadSfxParam(s32 sfxId, s32 param);
extern void closeMenu(void);
extern void func_800A1C6C(void);
/** @brief Flush the queued Triple Triad SFX to the SPU and empty the queue. */
extern void flushTriadSfxQueue(void);
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
