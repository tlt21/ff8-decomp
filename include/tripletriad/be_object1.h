#ifndef TRIPLETRIAD_BE_OBJECT1_H
#define TRIPLETRIAD_BE_OBJECT1_H

#include "common.h"
#include "tripletriad.h"

/* Declarations for be_object1.c (Triple Triad board/card setup, draw-buffer
   init, deferred VRAM transfers, and the tetrahedral 3D icon). */

#define BE_OT_LEN 28  /**< OT entries per buffer (D_801A2CE8). */

/* TIM file structs (Tim / TimSection) come from tim.h via battle.h. */

typedef struct {
    u32 type;
    u32 size;
    s32 offset;
} ResHeader;

/** @brief Deferred VRAM transfer dispatched by @c func_800988E0. */
typedef enum {
    POOL_LOAD_IMAGE  = 0,  /**< LoadImage(rect, src). */
    POOL_LOAD_TIM    = 1,  /**< LoadImage twice from a TIM file (CLUT + image). */
    POOL_STORE_IMAGE = 2,  /**< StoreImage(rect, dst). */
    POOL_MOVE_IMAGE  = 3   /**< MoveImage(rect, dstX, dstY) with @c src packed as @c y<<16|x. */
} PoolAction;

typedef struct {
    u8   active;        /**< @c PoolAction enum value. */
    u8   pad01[3];
    RECT rect;          /**< Destination/source RECT (unused when @c active == POOL_LOAD_TIM). */
    void *src;          /**< TIM pointer / pixel buffer / packed dest coords (depends on @c active). */
} PoolEntry;

/** @brief 0x28 per-frame transform scratch for the card-flip handler: a
 *  scratch position vector followed by the composed rotation+translation
 *  matrix handed to the GTE. Allocated/freed each frame (func_80098B80/BA0). */
typedef struct {
    SVECTOR vec;   /* 0x00 — scratch position (morph target)        */
    MATRIX  mat;   /* 0x08 — composed YXZ rotation + translation     */
} TransformBuf;    /* 0x28 */

/**
 * @brief One face of the tetrahedral 3D icon model (@c D_80182BB8[4]).
 *
 * The 4 entries pair with the 4-vertex SVECTOR table @c D_80182B98 to describe
 * a tetrahedral 3D icon (3 yellow faces and one white face). The 3 vertex
 * indices select corners from the transformed vertex output; the three color
 * words pre-pack the @c POLY_G3 @c r/g/b/code byte quartets for direct
 * word-store into the primitive.
 */
typedef struct {
    u8  v0;             /**< Index 0..3 into the transformed vertex table. */
    u8  v1;
    u8  v2;
    u8  pad03;
    u32 color0Word;     /**< Packed @c r0|g0|b0|code (with @c 0x30 = G3 code). */
    u32 color1Word;     /**< Packed @c r1|g1|b1|pad1. */
    u32 color2Word;     /**< Packed @c r2|g2|b2|pad2. */
} TriadFaceDesc;        /* 0x10 bytes */

typedef struct { u8 r, g, b; } RGB;

/** @brief Battle-state handler signature (entries of @c g_tripleTriadStateHandlers). */
typedef u8 *(*BattleStateFn)(void);

/** @brief Battle-state handler node: sub-state selector, frame counter, and
 *  phase bit (which side the card is showing). Shared between the card-flip
 *  handler (be_object1.c) and the match-flow driver (be_object1b.c). */
typedef struct {
    u8    pad00[0x0C];
    void *subHandler;  /* 0x0C — spawned per-turn sub-handler node           */
    u8    state;       /* 0x10 — sub-state                                   */
    u8    counter;     /* 0x11 — per-state frame counter                     */
    u8    phase;       /* 0x12 — flip phase / card side                      */
    u8    rulesFlags;  /* 0x13 — applyCardRules result carried across frames */
    u8    pad14;       /* 0x14 */
    u8    retryFlag;   /* 0x15 — set when a rule pass triggered captures     */
    u8    pad16[2];    /* 0x16 */
} HandlerNode;

/* ── Card-flip transform scratch ──────────────────────────────────────────── */
extern SVECTOR       D_80182BF8;        /* +Z unit scratch vector (morph source)   */
extern SVECTOR       D_80182C00;        /* scratch target vector                   */
extern SVECTOR       D_80182C08;        /* per-frame YXZ rotation angles           */
extern s32           D_801D300C;        /* spin direction delta (+/-0x400)         */
extern TransformBuf *D_801D3010;        /* current frame's transform scratch       */

/* ── Tetrahedral 3D icon model ────────────────────────────────────────────── */
extern SVECTOR       D_80182B98[4];     /**< 4-vertex tetrahedron model. */
extern TriadFaceDesc D_80182BB8[4];     /**< 4 G3 face descriptors. */
extern u32          *D_801D3008;        /**< Scratch buffer pointer for RotTransPers4 outputs. */

/* ── Deferred VRAM transfer pool (func_800988E0) ──────────────────────────── */
extern PoolEntry     D_801C2ED0[];
extern ResHeader     D_800B71D8;               /**< Resource header registered by the draw-target setup. */
extern ResHeader     g_tripleTriadCardFrames;  /**< Card frame/border graphics (4bpp TIM, uploaded to VRAM at init). */
extern ResHeader     g_tripleTriadCardArt;     /**< Card face artwork (8bpp TIM, ~110 cards at 64x64, uploaded to VRAM at init). */

/* ── Draw buffers / VRAM scratch ──────────────────────────────────────────── */
extern DISPENV       D_801C2E88[2];
extern RECT          D_800A45A8;
extern RECT          D_800A45B0;
extern u32           D_801A2CE8[2][BE_OT_LEN];
extern u8            D_801A2DC8[2][0x10000];  /* primitive pool, 64KB per buffer */
extern u8            D_801D2FF0[2][8];
extern u8            D_801C2FE0[2][0x8000];
extern u8           *D_801D2FE0;
extern u8           *D_801D3000;
extern u8           *g_tripleTriadActiveList;
extern u8            D_80182B84[];
extern u8            g_tripleTriadCardCounts[];

/* ── Debug text / misc state ──────────────────────────────────────────────── */
extern s32           D_801C2FD0;
extern s32           D_801C2FD8;
extern s16           D_80182B54;
extern s16           D_80182B5A;
extern s16           D_80182B58;
extern s16           g_textCursorX;
extern u8            D_801C2DC8;
extern s8            D_801C2DC9;
extern volatile u16  D_8005F158;
extern RGB           D_80182B5C;        /**< Debug-text rgb color. */
extern u32           D_80182AA0[];      /**< Color palette table, indexed by ASCII byte '0'..'8'. */
extern BattleStateFn g_tripleTriadStateHandlers[];      /**< Battle-state handler table. */

/* ── Entry points defined in be_object1.c (forward-declared for earlier callers) ── */
extern void func_800988D4(void);
extern void func_800988E0(void);

#endif /* TRIPLETRIAD_BE_OBJECT1_H */
