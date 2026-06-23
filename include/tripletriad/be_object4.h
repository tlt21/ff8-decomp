#ifndef TRIPLETRIAD_BE_OBJECT4_H
#define TRIPLETRIAD_BE_OBJECT4_H

#include "common.h"
#include "tripletriad.h"

/* Public interface of be_object4.c: the Triple Triad SFX request queue, the card-detail
   popup buffers, the rule-description string table, and the message-gate / menu helpers
   shared with be_object3 / be_object3b. */

/* ───────────────────────── Public typedefs ───────────────────────── */

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

/** @brief Typed view of the Triple Triad rule-description string table @ 0x80182680.
 *
 * One shared block (the be_object3 card-claim banners read its earlier slots): a u16
 * offset table whose entries are byte offsets from the block base to the FF8-encoded
 * rule labels packed later in the block.  func_800A24B4 recovers the base from the title
 * slot (@c &D_8018269E - 0x1E) and reaches each label as @c base + offset.  The intervening
 * @c padXX halfwords are the unused second field of each 4-byte offset record. */
typedef struct {
    /* 0x00 */ u16 pad00;
    /* 0x02 */ u16 sameOrPlus0;     /**< Lead-in control code for the Same/Plus clause. */
    /* 0x04 */ u16 pad04[13];
    /* 0x1E */ u16 title;           /**< "Rules" (carved as D_8018269E). */
    /* 0x20 */ u16 pad20;
    /* 0x22 */ u16 sameOrPlus1;     /**< ": " separator before the Same/Plus list. */
    /* 0x24 */ u16 pad24;
    /* 0x26 */ u16 plusConj;        /**< "," between listed Same/Plus terms (carved as D_801826A6). */
    /* 0x28 */ u16 pad28[3];
    /* 0x2E */ u16 openStr;         /**< ": Open"         (TT_RULE_OPEN). */
    /* 0x30 */ u16 pad30;
    /* 0x32 */ u16 randomStr;       /**< ": Random"       (TT_RULE_RANDOM). */
    /* 0x34 */ u16 pad34;
    /* 0x36 */ u16 suddenDeathStr;  /**< ": Sudden Death" (TT_RULE_SUDDEN_DEATH). */
    /* 0x38 */ u16 pad38;
    /* 0x3A */ u16 sameStr;         /**< "Same"           (TT_RULE_SAME). */
    /* 0x3C */ u16 pad3C;
    /* 0x3E */ u16 plusStr;         /**< "Plus"           (TT_RULE_PLUS). */
} RuleStrTable;

/* ───────────────────────── Public data ───────────────────────── */

extern SfxQueueEntry D_801D4500[];  /**< SFX request queue (up to 7 pending entries). */
extern s32 D_801D4560;              /**< Number of queued SFX requests. */

extern u8 g_cardDetailMsg[];    /**< Work buffer for the card-detail popup message (built by showCardDetail). */
extern u8 g_cardDetailSuffix[]; /**< String appended after the card name in the detail message. */

/* Rule-string table @ 0x80182680 (see RuleStrTable): individual u16 offset slots, each
   carved as its own symbol; the block base is recovered as @c &<slot> - <slot offset>. */
extern u16 D_8018269E;   /**< Rule-table title slot; the table base is @c &D_8018269E - 0x1E (0x80182680). */
extern u16 D_801826A6;   /**< Rule-table offset: "," conjunction (table + 0x26). */
extern u16 D_801826C2;   /**< Rule-table offset: "Same Wall"     (table + 0x42). */
extern u16 D_801826C6;   /**< Rule-table offset: ": Elemental"   (table + 0x46). */
extern u16 D_801826CA;   /**< Rule-table offset: ": Trade Rule", followed by the trade-rule names (table + 0x4A). */
extern u16 D_801826E2;   /**< Rule-table offset: post-game "Play / Quit" suffix (table + 0x62; used by func_800A1D68/updatePlayQuitPrompt). */

/* ───────────────────────── Public prototypes ───────────────────────── */

/** @brief Build the active Triple Triad rules description string into @p dst. */
extern void func_800A24B4(u8 *dst);
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
