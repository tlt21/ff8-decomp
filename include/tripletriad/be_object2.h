#ifndef TRIPLETRIAD_BE_OBJECT2_H
#define TRIPLETRIAD_BE_OBJECT2_H

#include "common.h"
#include "battle.h"        /* SubstateSlot */
#include "tripletriad.h"   /* board/card types, SVECTOR / CVECTOR / VECTOR */

/* Declarations for be_object2.c (Triple Triad card objects, the rules engine,
   the menu/cursor sub-machine, and the AI move search). */

/* ───────────────────────────── Public ──────────────────────────────────── */

/* Public typedefs */

/**
 * @brief Capture-direction descriptor — maps a captured-from direction to a
 *        card slide-in animation.
 *
 * @c D_80182D54 holds four entries (captured-from UP, LEFT, RIGHT, DOWN). For a
 * cell captured this turn, @c resolveCaptures matches the cell's
 * @c TT_CELL_CAP_FROM_* bit against @c capBit, then plays the matching
 * @c animType slide animation via @c setCardEntityType.
 */
typedef struct {
    s16 capBit;    /* 0x00 — matching TT_CELL_CAP_FROM_* bit */
    s16 animType;  /* 0x02 — CardEffectState (a CARD_FX_SLIDE_* direction) for setCardEntityType */
} CaptureDir;      /* 0x04 */

/* Public data */
extern CaptureDir D_80182D54[4];

/* Public prototypes */

/** @brief Set a card object's effect-animation @c state (a @ref CardEffectState);
 *         the @c CARD_FX_SLIDE_* captures also trigger a flip SFX.
 *         @c entityIdx indexes @c g_tripleTriadCardHands. */
extern void setCardEntityType(s32 entityIdx, s32 type);

/** @brief Resolve captured cells: trigger each captured cell's flip animation
 *         and clear its captured-from bits. */
extern void resolveCaptures(TripleTriadBoard *board);

/** @brief True while any hand card is mid effect-animation (slide/flip). */
extern s32 anyCardEffectActive(void);

/* Card-object processing + per-match board/hand setup. */
extern void processCardObjects(s32 arg);
extern void resetTriadBoard(void);
extern void setupTripleTriadHands(void);

/* Menu / cursor sub-machine. */
extern void initTriadTaskPool(void);
extern void resetTriadMenuState(void);
extern void updateTriadMenu(void);
extern void processTriadTasks(void);
/** @brief Enter an interactive card-selection substate. */
extern void activateMenuSubstate(s32 idx, s32 mask, u8 stateByte, s32 suppressFlags);
/** @brief Spawn the interactive card-selection cursor handler; returns the cursor list head. */
extern u8 *spawnCardSelectCursor(s32 rowSeed, s32 stateByte);

/* Card render / per-frame effect. */
extern TSPRT *drawCardOverlaySprite(CardAnimNode *node, s32 variant, void *ot, TSPRT *out);
extern void   animateCardEffect(TripleTriadCardObject *entity);
extern void   transformCardEffect(TripleTriadCardObject *entity, CardAnimNode *node, void *otBucket);

/* ───────── Private (only used in be_object2.c; may move into the .c) ─────── */

/* Enums / defines / consts */

/** @brief Outer state machine driven by @ref updateAiTurn (@c AiTurnNode.state). */
enum AiTurnState {
    AI_TURN_INIT    = 0,  /**< Reset the move-search workspace @c D_801D3460, then SEARCH. */
    AI_TURN_SEARCH  = 1,  /**< Run the move search (@ref AiSearchPhase sub-machine). */
    AI_TURN_ANIMATE = 2,  /**< Play the card-selection animation, then PLACE. */
    AI_TURN_PLACE   = 3   /**< Commit the chosen card to the board, then return 2. */
};

/** @brief Phase within @ref AI_TURN_SEARCH (@c AiTurnNode.sub). */
enum AiSearchPhase {
    AI_SEARCH_PREP = 0,   /**< Arm the search timer; latch the candidate card slot. */
    AI_SEARCH_RUN  = 1,   /**< Run @ref searchBestMove and handle its result. */
    AI_SEARCH_WAIT = 2    /**< Tick the timer; re-search (result 3) or advance to ANIMATE. */
};

/** @brief Frames the picked card is shown during @ref AI_TURN_ANIMATE before placement. */
#define AI_ANIM_FRAMES 15

/* Typedefs */

/**
 * @brief Per-ply state of the AI move search (one node of @ref searchBestMoveStack).
 *
 * @c D_801D3460 holds 9 of these — one per search ply. Each node carries the
 * (card, row, col) iterators the resumable minimax is currently trying at that
 * ply, the best move found so far, and its scores. Entry @c [0] is the root:
 * once the search completes its @c bestCol / @c bestRow give the chosen board
 * cell and @c bestCard the chosen hand slot.
 * @note @c D_801D3462 / @c D_801D3466 are separate splat symbols that alias
 *       @c D_801D3460[0].card / @c D_801D3460[0].bestCard.
 */
typedef struct {
    /* 0x00 */ u8 col;          /* board column currently being tried (0..2) */
    /* 0x01 */ u8 row;          /* board row currently being tried (0..2) */
    /* 0x02 */ u8 card;         /* hand slot currently being tried (0..4); reachable as D_801D3462 */
    /* 0x03 */ u8 noBest;       /* 1 = no best move recorded yet in this pass */
    /* 0x04 */ u8 bestCol;      /* best/chosen board column */
    /* 0x05 */ u8 bestRow;      /* best/chosen board row */
    /* 0x06 */ u8 bestCard;     /* best/chosen hand slot; reachable as D_801D3466 */
    /* 0x07 */ u8 checkBound;   /* 1 = prune this pass against `bound` (a prior pass completed) */
    /* 0x08 */ u8 pad08[4];
    /* 0x0C */ s32 bestWeighted; /* best difficulty-weighted score (raw * weight >> 12) */
    /* 0x10 */ s32 bestScore;    /* best raw board score */
    /* 0x14 */ s32 bound;        /* pruning threshold carried over from the completed pass */
} AiMove;                      /* 0x18 */

/**
 * @brief AI turn/placement controller — a 0x14-byte callback list node.
 *
 * Allocated by @ref spawnAiTurn via @c allocObjNode with @ref updateAiTurn
 * registered as its per-frame callback; the leading 0xC bytes are the list-node
 * header (links/callback managed by the @c func_80098Bxx pool routines).
 */
typedef struct {
    /* 0x00 */ u8 pad00[0xC];  /* list-node header (links / callback) */
    /* 0x0C */ u8 seat;        /* player / seat index */
    /* 0x0D */ u8 cardSlot;    /* hand-slot index of the card being worked */
    /* 0x0E */ u8 state;       /**< Outer state — see @ref AiTurnState. */
    /* 0x0F */ u8 sub;         /**< Phase within SEARCH (@ref AiSearchPhase); reused as a
                                    frame counter within ANIMATE. */
    /* 0x10 */ u8 unk10;       /* search-depth parameter passed to searchBestMove */
    /* 0x11 */ u8 result;      /* searchBestMove result status */
    /* 0x12 */ u8 pad12[2];
} AiTurnNode;          /* 0x14 */

/** @brief One row of the AI evaluation-weight table (@ref D_80182DAC). */
typedef struct { s32 w0, w1, w2, w3, w4; } WeightSet;  /* 0x14 */

/**
 * @brief 20-byte linked-list node owned by the @c D_801D3380 list and
 *        driven by the @ref updateCardSelectCursor callback.
 *
 * @c state is a small dispatch index (0..3) for the card-selection
 * cursor; @c fieldD and @c fieldE seed the row-cursor substate
 * subscriptions; @c snapshot is a 4-byte mirror of the current
 * @c D_801D3340 entry copied in when state 1 commits the choice.
 */
typedef struct {
    u8           pad00[0xC];   /* linked-list header (flags / next / callback) */
    u8           state;        /* 0x0C: dispatch state (0..3) */
    u8           fieldD;       /* 0x0D: cursor row index seed */
    u8           fieldE;       /* 0x0E: state byte forwarded to activateMenuSubstate */
    u8           pad0F;        /* 0x0F */
    SubstateSlot snapshot;     /* 0x10: copy of D_801D335C after the user's pick */
} SubstateMachineNode;

/* Data — list pools / heads */
extern ObjList D_801D3110[];
extern u8 D_801D3120[];
extern u8 D_801D3360[];
extern ObjList D_801D3380[];
extern u8 D_801D3798[];
extern s32 D_801D3328;

/* Data — AI move search */
extern AiMove    D_801D3460[9];   /* AI move-search workspace (root = [0], one entry per ply) */
extern u8        D_801D3462;      /* = D_801D3460[0].card (root card iterator / latched slot) */
extern u8        D_801D3466;      /* = D_801D3460[0].bestCard (chosen placement card) */
extern s32       D_801D3538;      /* minimax placement budget per search slice (see searchBestMoveStack) */
extern s32       D_801D353C;      /* search-phase frame timer */
extern s32       D_801D35C0;      /* current player / seat index */
extern u8        D_80182D64[8][9]; /* [difficulty][cards-left] → AI search-depth param */
extern WeightSet D_80182DAC[8];    /* per-difficulty AI evaluation-weight rows */
extern u8        D_80082C97;       /* = D_80082C90.field_07 (distinct splat symbol) */
extern u8        D_801D3540[];     /* AI turn-node pool */
extern ObjList        D_801D3560[];     /* AI turn-node list head */

/* Data — card render path (drawTriadCard) */
extern SVECTOR D_80182C30[4];   /* main card-face quad corners */
extern SVECTOR D_80182C50[4];   /* outer border quad corners */
extern SVECTOR D_80182C70[4];   /* per-corner offsets used per rank digit */
extern SVECTOR D_80182C90[4];   /* per-rank digit center positions */
extern SVECTOR D_80182CD0[4];   /* element marker quad corners */
extern SVECTOR D_80182CF0[4];   /* shadow quad corners (card drop-shadow) */

/* Data — card-effect gouraud quad (drawCardEffectQuad) */
extern s32     D_80182D30[];    /**< Per-corner phase offsets for the gouraud flicker. */
extern CVECTOR D_80182D40;       /**< Base colour passed to @c DpqColor. */
extern CVECTOR *D_801D3390;      /**< Scratch walker: current vertex-colour slot in the @c POLY_G4. */

/** @brief Card scale constant {0x1000, 0x1000, 0, 0} (1.0 in 12-bit fixed point),
 *         applied to a card's matrix during the slide-in animation. */
extern const VECTOR g_cardScaleVec;

/* Private prototypes — be_object2.c forward declarations */
extern s32  updateCardSelectCursor(SubstateMachineNode *p);
extern void drawMenuPrim(s32 mode, SubstateSlot *slot);
extern void handleCursorSubstate1(SubstateSlot *slot, s32 idx);
extern void handleCursorSubstate2(SubstateSlot *slot, s32 idx);
extern void handleCursorSubstate3(SubstateSlot *slot, s32 idx);

/* Private prototypes — imported from sibling TUs.
   (showCardDetail / openTriadMenu prototypes live in tripletriad.h; the GTE /
   GPU primitives — RotTransPers4, NormalClip, AddPrim — in the psxsdk headers.) */
extern void *func_8002FF34(s32 *otBase, void *pkt, s32 ch, s32 yPos, s32 w, s32 col);

#endif /* TRIPLETRIAD_BE_OBJECT2_H */
