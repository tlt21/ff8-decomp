#include "common.h"
#include "battle.h"
#include "item.h"
#include "tripletriad.h"
#include "psxsdk/libgpu.h"
#include "psxsdk/libc.h"
#include "tripletriad/be_object1.h"
#include "tripletriad/be_object1b.h"
#include "tripletriad/be_object2.h"
#include "tripletriad/be_object3.h"
#include "tripletriad/be_object4.h"

/**
 * @brief Script-action entry in the D_801D3EC0 2x5 table.
 *
 * Initialized in initTripleTriadScripts. Used by func_8009FC90's state-2 sweep
 * to mark queued actions complete and by func_8009EF68 to scan for
 * pending actions.
 */
typedef struct {
    /* 0x00 */ u8 marker;       /**< Sentinel; set to 0xFF on init. */
    /* 0x01 */ u8 status;       /**< Action status (set to 3 to mark "complete"). */
    /* 0x02 */ u8 field02;      /**< Cleared when action marked complete; animation frame counter in func_8009EBF4. */
    /* 0x03 */ u8 row;          /**< Row index (cached on init). */
    /* 0x04 */ u8 col;          /**< Column index (cached on init). */
    /* 0x05 */ u8 pad05;
    /* 0x06 */ u16 field06;     /**< Rotation-vector base fed to RotMatrixYXZ (rotX); rotY at 0x08, rotZ at 0x0A. */
    /* 0x08 */ s16 actionId;    /**< Pending action ID; also the card-flip Y-rotation in func_8009EBF4. */
    /* 0x0A */ u16 field0A;
    /* 0x0C */ u8 pad0C[2];
    /* 0x0E */ s16 posX;        /**< Screen position fed to the GTE translation (func_8009EBF4). */
    /* 0x10 */ s16 posY;
    /* 0x12 */ s16 posZ;
    /* 0x14 */ s16 sort;        /**< OT sort key. */
} ScriptEntry; /* 0x16 = 22 bytes */

/**
 * @brief Scratch buffer for func_8009EBF4's per-card matrix build (0x2C bytes).
 *
 * Allocated each frame via scratchAlloc; @c f0/@c f2 carry the card's row/col,
 * layoutCardSlot fills @c pos (position + OT sort key), and @c mtx holds the
 * rotation/translation matrix passed to SetRotMatrix / SetTransMatrix.
 */
typedef struct {
    /* 0x00 */ u8      f0;    /**< layoutCardSlot descriptor type. */
    /* 0x01 */ u8      pad1;
    /* 0x02 */ u8      f2;    /**< layoutCardSlot descriptor row. */
    /* 0x03 */ u8      pad3;
    /* 0x04 */ SVECTOR pos;   /**< Position filled by layoutCardSlot (vx/vy/vz/pad). */
    /* 0x0C */ MATRIX  mtx;
} TmpBuf; /* 0x2C */

/**
 * @brief Callback context for state-machine handlers (e.g. func_8009FC90).
 *
 * Allocated via allocObjNode / allocObjNodeFront with state byte at +0x10.
 */
typedef struct {
    /* 0x00 */ u8 pad00[0x0C];
    /* 0x0C */ s32 cachedResult;
    /* 0x10 */ u8 state;
    /* 0x11 */ u8 subState;
    /* 0x12 */ u8 counter;
    /* 0x13 */ u8 field13;   /**< Cleared to 0 when func_8009F17C (re)inits the sequence. */
} ScriptCtx;

extern ObjList D_801D3C68[];
extern u8 D_801D3C78[];
extern ScriptEntry D_801D3EC0[2][5];
extern ObjList D_801D3FA0[];   /**< Script-handler object pool. */
extern u8 D_801D3FB0[];   /**< Backing element storage for the D_801D3FA0 pool. */
extern ObjList D_801D3EA0[];   /**< Setup-handler object pool (func_8009FAF8). */
extern u8 D_801D3E80[];   /**< Backing element storage for the D_801D3EA0 pool. */
extern u8 D_80082C95;     /**< Card-config byte; the owned-quantity delta for built hands. */
extern u8 D_80078658[];   /**< Card rarity/type table (cards 0x4D+); used to draw rarity-filtered hands. */
extern u8 D_801D4298[2][5]; /**< Per-player working copy of the hands (card ids), seeded from D_801A2C48 (func_800A1080). */
extern s32 D_801D4288;   /**< Count of objects processed this sweep (func_800A0F0C). */
extern s32 D_801D4448;   /**< Target object count; sweep completes when D_801D4288 reaches it. */
extern u8 D_801D444C;
extern s32 D_801D3D08;
extern ObjNodeFn D_80182E4C[];
extern u8 D_80158680[];
extern u8 D_80182E68[]; /**< Staged fade color (RGB); the start color for the next fade. */
extern u8  D_801D444D;   /**< Set to 1 when func_800A1374's capture/cleanup sweep finishes. */
extern s32 D_801D4450;   /**< Acting seat index (0 or 1) for the capture/cleanup sweeps. */
extern s32 D_801D4454;
extern s32 D_801D4178;   /**< Running captured-card counter (func_800A0B24). */
extern u8  D_801D4188[]; /**< 256-byte global scratch buffer for the built banner string. */
extern u16 D_80182686;   /**< Relative-pointer string-table offsets (pool base 0x80182680). */
extern u16 D_8018268A;
extern u16 D_8018268E;
extern u8  D_801D42A8[];  /**< Backing element storage for the D_801D42F8 pool. */
extern u8  D_801D4078[];  /**< Scratch buffer for the captured-card name banner. */
extern u16 D_80182692;    /**< Card-claim banner string-table offsets (pool base 0x80182680). */
extern u16 D_80182696;
extern s32 func_800A03DC(void); /**< Per-frame board render/update loop. */
extern u8  *func_80023A54(s32 cardId);       /**< Look up a card's name string. */
extern void strcpy(u8 *dst, u8 *src); /**< Copy a string into the work buffer. */
extern void func_80047C74(u8 *dst, u8 *src); /**< Append a string to the work buffer. */
extern s32 isItemPresent(s32 cardId);        /**< Non-zero if the card is in the collection. */

/* Per-frame input-edge flags: two separate u16s at 0x801D3E74 / 0x801D3E76,
   refreshed from D_801C2EBC / g_padPressed[2] at the top of func_8009F17C. */
extern u16 D_801D3E74;   /**< Edge flag A: 0x10 == remove-card request. */
extern u16 D_801D3E76;   /**< Edge flag B: 0x40/0x80 == add-card request. */
extern s32 D_801D3E78;   /**< Number of cards built into the active hand (0..5). */
extern u16 D_801C2EBC;   /**< Source value copied into D_801D3E74 each frame. */

/**
 * @brief Full-screen color-fade effect object.
 *
 * Spawned by @c func_800A030C (fade to black) and @c func_800A0370 (fade to
 * white), then driven each frame by @c func_800A01DC: it interpolates a
 * screen-sized sprite's color from @c startColor toward @c endColor across
 * @c duration frames, advancing @c frame each call. When the fade completes it
 * stages @c endColor back into @c D_80182E68 so the next fade starts from this
 * color.
 */
typedef struct {
    /* 0x00 */ u8  pad00[0x0C];
    /* 0x0C */ s16 frame;          /**< Current frame counter (0..duration). */
    /* 0x0E */ s16 duration;       /**< Total fade length, in frames. */
    /* 0x10 */ u8  startColor[4];  /**< Starting RGB, copied from the staged D_80182E68. */
    /* 0x14 */ u8  endColor[4];    /**< Target RGB (written as a word: 0 = black, 0xFFFFFF = white). */
} FadeObject;

/**
 * @brief Card scale/fade animation sprite primitive (0x18 bytes).
 *
 * Built each frame by @c func_8009E270 / @c func_8009E464 at the @c g_primCursor
 * cursor and linked into @c g_otBase[3]; the @c width field is animated to
 * scale the card sprite in/out.
 */
typedef struct {
    /* 0x00 */ u32 tag;       /**< OT-link tag (0x5000000). */
    /* 0x04 */ u32 code;      /**< GPU primitive command word (0xE100060E). */
    /* 0x08 */ u32 color;     /**< Packed command + RGB (0x64808080). */
    /* 0x0C */ s16 width;     /**< Animated sprite width. */
    /* 0x0E */ s16 field0E;
    /* 0x10 */ u8  field10;
    /* 0x11 */ u8  field11;
    /* 0x12 */ s16 field12;
    /* 0x14 */ s16 field14;
    /* 0x16 */ s16 field16;
} CardScaleSprite; /* 0x18 */

/* GPU/colour helpers used by the fade handler (main-binary primitives). */
extern void func_800408A4(s32 r, s32 g, s32 b);            /**< Set the interpolation target colour. */
extern void func_80040918(u8 *startColor, s32 frac, u8 *dst); /**< Lerp startColor -> target by @p frac into @p dst. */
/* AddPrim — link a primitive into the OT bucket (psxsdk/libgpu.h). */
extern void func_8004D724(void *prim, s32 a1, s32 a2, s32 a3); /**< Init the trailing blend-control primitive. */

extern s32 func_8009EBF4(void); /**< Per-frame card slide/scale animation sweep (defined below). */

/* Triple Triad capture/flow helpers (defined in a sibling overlay TU). */
extern void markItemPresent(s32 cardId);                  /**< Return a card to the owner's collection. */
extern void func_800A1D68(s32 a0, u8 *a1, s32 a2);        /**< Show a banner/message string. */
extern void func_800A2054(s32 a0);                        /**< Acknowledge/advance a message gate. */
extern s32  func_800A20F4(s32 a0);                        /**< Poll a message gate (>0 = result, <0 = pending). */
/* showCardDetail / activateMenuSubstate prototypes now live in tripletriad.h */
extern void func_800A44CC(void);   /**< Reset the hand-build UI state for a new claim sequence. */
extern void func_800A44B0(s32 a0); /**< Enable (1) / disable (0) the hand-build input prompt. */
extern void func_800A44BC(void);   /**< Tear down the claim UI at the end of the sequence. */

/** @brief Allocate a tripletriad object from the @c D_801D3C58 pool with the
 *         given per-frame callback; returns the new object (0 if pool is full). */
s32 func_8009E248(ObjNodeFn a0) {
    return allocObjNode(D_801D3C58, a0);
}

/**
 * @brief Per-frame card scale animation: scales the sprite in, holds, then scales out.
 *
 * Three states driven by @c node->state: state 0 scales the width up from 0x40 toward
 * 0x180 over 5 frames (via the @c /5 ramp @c ((field0D<<12)/5)); state 1 holds @c width
 * at 0x40 for 0x28 frames; state 2 scales back down over 5 frames. Each frame it rebuilds
 * the @c CardScaleSprite at the @c g_primCursor cursor and links it into @c g_otBase[3].
 *
 * @param node State-machine driver node (@c field0E selects the 0x3802/0x3842 sprite variant).
 * @return 2 once the scale-out completes (state 2), else 0.
 */
s32 func_8009E270(ScriptStateNode *node) {
    CardScaleSprite *prim;
    s32 q;

    prim = g_primCursor;
    switch (node->state) {
    case 0:
        q = ((++node->field0D & 0xFF) << 12) / 5;
        prim->width = (-((q * 5) << 6) >> 12) + 0x180;
        if (node->field0D >= 5) {
            node->state = 1;
            node->field0D = 0;
        }
        break;
    case 1:
        prim->width = 0x40;
        if ((++node->field0D & 0xFF) >= 0x28) {
            node->state = 2;
            node->field0D = 0;
        }
        break;
    case 2:
        q = ((++node->field0D & 0xFF) << 12) / 5;
        prim->width = ((q - ((q * 5) << 6)) >> 12) + 0x40;
        if (node->field0D >= 5) {
            return 2;
        }
        break;
    }
    prim->tag = 0x5000000;
    prim->code = 0xE100060E;
    prim->color = 0x64808080;
    prim->field0E = 0x50;
    prim->field10 = 0;
    if (node->field0E == 0) {
        prim->field11 = 0;
        prim->field12 = 0x3802;
    } else {
        prim->field11 = 0x40;
        prim->field12 = 0x3842;
    }
    prim->field14 = 0xFF;
    prim->field16 = 0x40;
    AddPrim(g_otBase + 3, prim);
    g_primCursor = (u8 *)prim + 0x18;
    return 0;
}

/**
 * @brief Per-frame card scale animation variant: scale in, hold, scale out.
 *
 * Twin of @c func_8009E270 with a shorter hold (state 1 lasts 0x19 frames) and a
 * fixed sprite variant (no @c field0E branch — always @c field11=0x80 /
 * @c field12=0x3882). Each frame it rebuilds the @c CardScaleSprite at the
 * @c g_primCursor cursor and links it into @c g_otBase[3].
 *
 * @param node State-machine driver node (state at 0x0C, frame counter at 0x0D).
 * @return 2 once the scale-out completes (state 2), else 0.
 *
 * @note The @c color store uses a @c volatile cast as a scheduling barrier: unlike
 *       @c func_8009E270 (whose @c field0E if/else creates a basic-block boundary
 *       after @c field10), this function stores the sprite fields unconditionally,
 *       so without the barrier cc1 floats the @c color store down past @c field10
 *       to fill the @c g_otBase load-delay shadow. The barrier keeps it in source
 *       order, matching the original.
 */
s32 func_8009E464(ScriptStateNode *node) {
    CardScaleSprite *prim;
    s32 q;

    prim = g_primCursor;
    switch (node->state) {
    case 0:
        q = ((++node->field0D & 0xFF) << 12) / 5;
        prim->width = (-((q * 5) << 6) >> 12) + 0x180;
        if (node->field0D >= 5) {
            node->state = 1;
            node->field0D = 0;
        }
        break;
    case 1:
        prim->width = 0x40;
        if ((++node->field0D & 0xFF) >= 0x19) {
            node->state = 2;
            node->field0D = 0;
        }
        break;
    case 2:
        q = ((++node->field0D & 0xFF) << 12) / 5;
        prim->width = ((q - ((q * 5) << 6)) >> 12) + 0x40;
        if (node->field0D >= 5) {
            return 2;
        }
        break;
    }
    prim->tag = 0x5000000;
    prim->code = 0xE100060E;
    *(volatile u32 *)&prim->color = 0x64808080;
    prim->field0E = 0x50;
    prim->field10 = 0;
    prim->field11 = 0x80;
    prim->field12 = 0x3882;
    prim->field14 = 0xFF;
    prim->field16 = 0x40;
    AddPrim(g_otBase + 3, prim);
    g_primCursor = (u8 *)prim + 0x18;
    return 0;
}

/** @brief D_801D3C68-pool node (0x24 bytes) for the screen gradient-fade effect. */
typedef struct {
    /* 0x00 */ u8  pad00[0x0C];
    /* 0x0C */ u8  state;
    /* 0x0D */ u8  frame;     /**< Brightness ramp counter (0..0xFF); seeds the gradient colours. */
    /* 0x0E */ u8  variant;   /**< Gradient style (2/3/4), set from the callback-table index. */
    /* 0x0F */ u8  pad0F[0x11];
    /* 0x20 */ s16 field20;
    /* 0x22 */ u8  done;      /**< When 1 the effect is finished (func_8009E640 returns 2). */
    /* 0x23 */ u8  pad23;
} GradientFadeNode; /* 0x24 */

/**
 * @brief Two-layer screen gradient-fade primitive (0x34 bytes), built by func_8009E640.
 *
 * A 4-vertex gradient quad spanning the play area (x 0x40..0x13F, y 0x58..0x88); the four
 * vertex colours are ramped from @c node->frame and @c pal* carry the palette level. Two of
 * these are linked per frame (different @c field0E / @c field1A blend/tpage words) for a
 * layered fade.
 * @note Exact GPU semantics of @c code* / @c pal* / @c field0E / @c field1A are estimated.
 */
typedef struct {
    /* 0x00 */ u32 tag;
    /* 0x04 */ u32 color0;
    /* 0x08 */ s16 x0;
    /* 0x0A */ s16 y0;
    /* 0x0C */ u8  code0;
    /* 0x0D */ u8  pal0;
    /* 0x0E */ s16 field0E;
    /* 0x10 */ u32 color1;
    /* 0x14 */ s16 x1;
    /* 0x16 */ s16 y1;
    /* 0x18 */ u8  code1;
    /* 0x19 */ u8  pal1;
    /* 0x1A */ s16 field1A;
    /* 0x1C */ u32 color2;
    /* 0x20 */ s16 x2;
    /* 0x22 */ s16 y2;
    /* 0x24 */ u8  code2;
    /* 0x25 */ u8  pal2;
    /* 0x28 */ u32 color3;
    /* 0x2C */ s16 x3;
    /* 0x2E */ s16 y3;
    /* 0x30 */ u8  code3;
    /* 0x31 */ u8  pal3;
} GradientFadeQuad; /* 0x34 */

/**
 * @brief Per-frame builder for the two-layer screen gradient fade.
 *
 * Builds two @c GradientFadeQuad primitives at the @c g_primCursor cursor and links each into
 * @c g_otBase[3]. The @c variant (2/3/4) selects the palette base and first-layer blend
 * word; the four vertex colours ramp with @c node->frame (each vertex 5 steps behind the
 * previous, clamped to 0..0x80 then packed as @c 0x3E000000|c|c<<8|c<<16). @c frame advances
 * each frame (capped at 0xFF).
 *
 * @param node Gradient-fade node (variant at 0x0E, ramp counter at 0x0D, done flag at 0x22).
 * @return 2 once @c done is set, else 0.
 *
 * @note Each layer's last colour store (@c color3) uses a @c volatile cast as a scheduling
 *       barrier: it stops cc1 pulling that store into the @c AddPrim jal delay slot, so
 *       the slot instead holds the @c g_otBase+3 pointer arithmetic, matching the original.
 */
s32 func_8009E640(GradientFadeNode *node) {
    GradientFadeQuad *prim;
    s32 colors[4];
    s32 i;
    s32 seed;
    s32 c;
    s32 lvl;

    if (node->done == 1) {
        return 2;
    }
    prim = g_primCursor;

    switch (node->variant) {
    case 2:
        lvl = 0;
        prim->field0E = 0x3801;
        break;
    case 3:
        lvl = 0x30;
        prim->field0E = 0x3841;
        break;
    case 4:
        lvl = 0x60;
        prim->field0E = 0x3881;
        break;
    }

    prim->field1A = 0x2D;
    prim->tag = 0xC000000;
    prim->x2 = 0x40;
    prim->x0 = 0x40;
    prim->x3 = 0x13F;
    prim->x1 = 0x13F;
    prim->y1 = 0x58;
    prim->y0 = 0x58;
    prim->y3 = 0x88;
    prim->y2 = 0x88;
    prim->code3 = 0xFF;
    prim->code1 = 0xFF;
    prim->code2 = 0;
    prim->code0 = 0;
    prim->pal1 = lvl;
    prim->pal0 = lvl;
    prim->pal3 = lvl + 0x30;
    prim->pal2 = lvl + 0x30;

    seed = node->frame;
    for (i = 0; i < 4; i++) {
        if (seed < 0) {
            colors[i] = 0;
        } else if (seed < 0xF) {
            colors[i] = seed * 128 / 15;
        } else {
            colors[i] = 0x80;
        }
        c = colors[i];
        colors[i] = c | (c << 8) | (c << 16) | 0x3E000000;
        seed -= 5;
    }
    prim->color0 = colors[0];
    prim->color1 = colors[2];
    prim->color2 = colors[1];
    *(volatile u32 *)&prim->color3 = colors[3];
    AddPrim(g_otBase + 3, prim);
    prim++;

    prim->tag = 0xC000000;
    prim->field1A = 0x4D;
    prim->field0E = 0x3FC0;
    prim->x2 = 0x40;
    prim->x0 = 0x40;
    prim->x3 = 0x13F;
    prim->x1 = 0x13F;
    prim->y1 = 0x58;
    prim->y0 = 0x58;
    prim->y3 = 0x88;
    prim->y2 = 0x88;
    prim->code3 = 0xFF;
    prim->code1 = 0xFF;
    prim->code2 = 0;
    prim->code0 = 0;
    prim->pal1 = lvl;
    prim->pal0 = lvl;
    prim->pal3 = lvl + 0x30;
    prim->pal2 = lvl + 0x30;
    prim->color0 = colors[0];
    prim->color1 = colors[2];
    prim->color2 = colors[1];
    *(volatile u32 *)&prim->color3 = colors[3];
    AddPrim(g_otBase + 3, prim);

    if (node->frame < 0xFF) {
        node->frame++;
    }
    g_primCursor = (u8 *)prim + 0x34;
    return 0;
}

/**
 * @brief Per-frame card colour fade-in / fade-out sprite builder.
 *
 * Sibling of @c func_8009E270 / @c func_8009E464: each frame it rebuilds a
 * @c CardScaleSprite at the @c g_primCursor cursor and links it into @c g_otBase[3],
 * animating the sprite's @c color. A two-state machine drives it:
 *  - state 0 fades the colour up: @c alpha = (v*255)>>12 with
 *    @c v = ((++frame & 0xFF)<<12)/10, packing 0x66-prefixed RGB; advances to
 *    state 1 once @c frame reaches 0xA.
 *  - state 1 fades back down: @c alpha = ((v - 127*v)>>12)+0xFF with @c v = .../20
 *    (the @c v-(v<<7) == -127*v idiom), packing 0x64-prefixed RGB until @c frame
 *    reaches 0x14, after which it holds a solid 0x64808080 colour while
 *    re-initialising the whole primitive.
 *
 * Each branch links its primitive and advances the cursor with @c prim++
 * (a @c CardScaleSprite is 0x18 bytes), writing the new tail back to @c g_primCursor.
 *
 * @param node State-machine driver (state at 0x0C, frame counter at 0x0D).
 * @return 2 when finished (@c node->field22 already 1, or while @c D_801D3D08 gates
 *         it off returns 0), else 0.
 */
s32 func_8009E904(ScriptStateNode *node) {
    CardScaleSprite *prim;
    s32 v;
    s32 a;
    s32 t;
    s32 a2;
    s32 t2;

    if (node->field22 == 1) {
        return 2;
    }
    if (D_801D3D08 >= 2) {
        return 0;
    }
    prim = g_primCursor;
    prim->tag = 0x5000000;
    prim->code = 0xE100062D;
    prim->width = 0x40;
    prim->field0E = 0x58;
    prim->field10 = 0;
    prim->field11 = 0;
    prim->field14 = 0xFF;
    prim->field16 = 0x30;
    switch (node->state) {
    case 0:
        v = ((++node->field0D & 0xFF) << 12) / 10;
        a = (v * 255) >> 12;
        t = (a << 8) | 0x66000000;
        a = a | t | (a << 16);
        *(volatile u32 *)&prim->color = a;
        prim->field12 = 0x3801;
        AddPrim(g_otBase + 3, prim++);
        if (node->field0D >= 0xA) {
            node->state = 1;
            node->field0D = 0;
        }
        break;
    case 1:
        if ((u32)node->field0D < 0x14) {
            v = ((++node->field0D & 0xFF) << 12) / 20;
            a2 = ((v - (v << 7)) >> 12) + 0xFF;
            prim->field12 = 0x3801;
            t2 = (a2 << 8) | 0x64000000;
            a2 = a2 | t2 | (a2 << 16);
            *(volatile u32 *)&prim->color = a2;
            AddPrim(g_otBase + 3, prim++);
        } else {
            prim->color = 0x64808080;
            prim->field12 = 0x3801;
            prim->code = 0xE100062D;
            prim->field10 = 0;
            prim->field11 = 0;
            prim->field16 = 0x30;
            prim->tag = 0x5000000;
            prim->width = 0x40;
            prim->field0E = 0x58;
            prim->field14 = 0xFF;
            AddPrim(g_otBase + 3, prim++);
        }
        break;
    }
    g_primCursor = prim;
    return 0;
}

/**
 * @brief Allocate a node in D_801D3C68 list with a callback from D_80182E4C.
 *
 * Looks up a callback pointer from the D_80182E4C table at index @p a0,
 * allocates a node via allocObjNodeFront, and initializes several fields.
 *
 * @param a0 Index into the D_80182E4C callback table.
 */
void func_8009EB30(s32 a0) {
    u8 *node = (u8 *)allocObjNodeFront(D_801D3C68, D_80182E4C[a0]);
    if (node != 0) {
        node[0xE] = a0;
        *(s16 *)(node + 0x20) = 0;
        node[0x22] = 0;
        node[0xC] = 0;
        node[0xD] = 0;
    }
}

/**
 * @brief Store a byte value at offset 0x22 of a structure.
 *
 * @param a0 Pointer to structure base.
 * @param a1 Byte value to store at offset 0x22.
 */
void func_8009EB90(u8 *a0, s32 a1) {
    a0[0x22] = a1;
}

/**
 * @brief Initialize the D_801D3C68 list with node pool D_801D3C78.
 *
 * Sets up a linked list with node size 0x24 and capacity 0x4.
 */
void func_8009EB98(void) {
    initObjList(D_801D3C68, D_801D3C78, 0x24, 0x4);
}

/**
 * @brief Tick the screen-fade effect objects, recording how many remain active.
 *
 * Updates the @c D_801D3C68 gradient-fade effect list and stores the live count
 * in @c D_801D3D08.
 */
void updateFadeEffects(void) {
    D_801D3D08 = updateObjectList(D_801D3C68);
}

/**
 * @brief Per-frame card slide/scale animation sweep over the D_801D3EC0 table.
 *
 * Walks all 2x5 @c ScriptEntry slots and, for each active one (@c status 1/2/3),
 * advances a 15-frame animation: status 1 slides the card in (posY eases from
 * @c f6 toward 0xE0), status 2 slides it out, status 3 flips it (Y-rotation
 * ramp + Z-scale). Each finished slot resets to idle (@c status = @c field02 = 0).
 * Active slots build a rotation/translation matrix from the entry's rotation
 * vector + computed position and link a sprite via drawTriadCard.
 *
 * @return 0.
 */
s32 func_8009EBF4(void)
{
    s32 col;
    s32 row;
    ScriptEntry *e;
    TmpBuf *tmp;
    s32 frame;
    s32 s0;
    s32 scratch[2];

    tmp = (TmpBuf *)scratchAlloc(0x2C);
    for (row = 0; row < 2; row++) {
        for (col = 0; col < 5; col++) {
            e = &D_801D3EC0[row][col];
            switch (e->status) {
            case 0:
                break;
            case 1:
                if (e->field02 == 0) {
                    playTriadSfx(0x59);
                }
                frame = ++e->field02;
                s0 = ((frame & 0xFF) << 12) / 15;
                s0 = rsin(s0 / 4);
                tmp->f0 = e->row;
                tmp->f2 = e->col;
                layoutCardSlot((u8 *)tmp, &tmp->pos);
                e->posX = tmp->pos.vx;
                e->posY = ((tmp->pos.vy - 0xE0) * s0 >> 12) + 0xE0;
                e->posZ = tmp->pos.vz;
                e->sort = tmp->pos.pad;
                if (e->field02 >= 0xF) {
                    e->status = 0;
                    e->field02 = 0;
                }
                break;
            case 2:
                frame = ++e->field02;
                s0 = ((frame & 0xFF) << 12) / 15;
                s0 = rsin(s0 / 4);
                tmp->f0 = e->row;
                tmp->f2 = e->col;
                layoutCardSlot((u8 *)tmp, &tmp->pos);
                e->posX = tmp->pos.vx;
                e->posY = ((0xE0 - tmp->pos.vy) * s0 >> 12) + tmp->pos.vy;
                e->posZ = tmp->pos.vz;
                e->sort = tmp->pos.pad;
                if (e->field02 >= 0xF) {
                    e->status = 0;
                    e->field02 = 0;
                }
                break;
            case 3:
                frame = ++e->field02;
                s0 = ((frame & 0xFF) << 12) / 15;
                e->actionId = (-(s0 << 11) >> 12) + 0x800;
                s0 = rsin(s0 / 2);
                e->posZ = 0x200 - ((s0 << 6) >> 12);
                if (e->field02 >= 0xF) {
                    e->status = 0;
                    e->field02 = 0;
                }
                break;
            }
            if (e->marker != 0xFF) {
                RotMatrixYXZ((u8 *)&e->field06, &tmp->mtx);
                tmp->mtx.t[0] = e->posX;
                tmp->mtx.t[1] = e->posY;
                tmp->mtx.t[2] = e->posZ;
                SetRotMatrix(&tmp->mtx);
                SetTransMatrix(&tmp->mtx);
                g_primCursor = drawTriadCard(e->marker, e->row | 0x12,
                                           g_otBase + e->sort, g_primCursor);
            }
        }
    }
    scratchFree(0x2C);
    return 0;
}

/**
 * Checks if any active slot has a pending action.
 *
 * Searches through a 2D table (2 rows of 5 entries, each 22 bytes apart,
 * rows 110 bytes apart) for an entry where byte 0 is not 0xFF and byte 1
 * is non-zero.
 *
 * @return 1 if a pending action was found, 0 otherwise.
 */
s32 func_8009EF68(void) {
    s32 row = 0;
    u8 *base = (u8 *)D_801D3EC0;
    s32 marker = 0xFF;
    s32 rowOff = row;
    s32 col;
    s32 colOff;
    do {
        col = 0;
        colOff = rowOff;
        do {
            u8 *entry = (u8 *)(colOff + (s32)base);
            if (entry[0] != marker) {
                if (entry[1] != 0) {
                    return 1;
                }
            }
            col++;
            colOff += 0x16;
        } while (col < 5);
        row++;
        rowOff += 0x6E;
    } while (row < 2);
    return 0;
}

extern u8 *func_80023A54(s32 idx); /**< Look up a card's display-name string by card index. */

/**
 * @brief Build a card-list display string for 11 consecutive card entries.
 *
 * For each index @p base + 0..10 whose owned count (@c func_80023B14) is
 * non-negative, appends the card's name (@c func_80023A54), the icon byte
 * 0xF4, the count rendered as decimal digit glyphs (+0x53, 1-3 digits), and
 * the icon byte 0xF5; every entry is terminated by a @c 2 separator. The final
 * separator is overwritten with a NUL.
 *
 * @param out  Destination string buffer.
 * @param base First card index.
 */
void func_8009EFD4(u8 *out, s32 base) {
    s32 i;
    s32 count;
    u8 *name;
    u8 c;

    for (i = 0; i < 11; i++) {
        count = func_80023B14(base + i);
        if (count >= 0) {
            name = func_80023A54(base + i);
            c = *name;
            while (c != 0) {
                name++;
                *out = c;
                c = *name;
                out++;
            }
            *out++ = 0xF4;
            if (count >= 100) {
                *out++ = (count / 100) % 10 + 0x53;
            }
            if (count >= 10) {
                *out++ = (count / 10) % 10 + 0x53;
            }
            *out++ = count % 10 + 0x53;
            *out++ = 0xF5;
        }
        *out++ = 2;
    }
    *(out - 1) = 0;
}

/**
 * @brief Per-frame interactive card-claim sequencer (state machine, @c node->state 0-3).
 *
 * Drives the post-battle "build your hand from the claimed cards" flow. State 0 clears
 * the hand slots and arms the UI; state 1 adds a card (on the @c D_801D3E76 0x40/0x80
 * input edge) or removes one (on the @c D_801D3E74 0x10 edge), one per frame, into
 * @c D_801D3EC0 / @c D_801A2C48 until the hand holds five; state 2 polls a confirmation
 * message gate; state 3 runs the fade-out. The case-1, @c func_8009EF68 and @c r<0
 * "still waiting" paths share a single return via @c ret0.
 *
 * @param node State-machine node (allocated by allocObjNode with state at +0x10).
 * @return 0 while the sequence is still running; the state-3 expression once it ends.
 */
s32 func_8009F17C(ScriptCtx *node) {
    s32 i;
    u8 card;
    s32 r;
    s32 idx;
    u8 *hand;
    u8 *handRow;

    D_801D3E74 = D_801C2EBC;
    D_801D3E76 = g_padPressed[2];
    while (1) {
        switch (node->state) {
        case 0:
            for (i = 0; i < 5; i++) {
                D_801A2C48[node->counter][i] = 0xFF;
            }
            func_800A44CC();
            func_800A44B0(1);
            node->field13 = 0;
            D_801D3E78 = 0;
            node->state = 1;
            node->subState = 0;
            break;
        case 1:
            if (node->subState == 0) {
                g_tripleTriadInputFlags |= TT_INPUT_HAND_BUILD;
                node->subState++;
                goto ret0;
            }
            if (D_801D3E76 == 0x40 || D_801D3E76 == 0x80) {
                if (D_801D44FC >= 0) {
                    card = D_801D44FC;
                    if (D_801D3E78 >= 5) {
                        node->state = 1;
                        node->subState = 0;
                        break;
                    }
                    if (func_80023B14(card) <= 0) {
                        node->state = 1;
                        node->subState = 0;
                        break;
                    }
                    (D_801D3EC0[node->counter] + D_801D3E78)->status = 1;
                    (D_801D3EC0[node->counter] + D_801D3E78)->field02 = 0;
                    D_801D3EC0[node->counter][D_801D3E78].marker = card;
                    hand = D_801A2C48[node->counter];
                    idx = D_801D3E78;
                    hand[idx] = card;
                    D_801D3E78 = ++idx;
                    modifyItemQuantity(card, D_80082C95);
                    playTriadSfx(1);
                    if (D_801D3E78 != 5) {
                        node->state = 1;
                        node->subState = 0;
                        break;
                    }
                    g_tripleTriadInputFlags &= ~TT_INPUT_HAND_BUILD;
                    node->state = 2;
                    node->subState = 0;
                    break;
                }
            }
            if (D_801D3E74 == 0x10 && D_801D3E78 > 0) {
                handRow = D_801A2C48[node->counter];
                D_801D3E78--;
                card = handRow[D_801D3E78];
                D_801A2C48[node->counter][D_801D3E78] = 0xFF;
                markItemPresent(card);
                node->state = 1;
                node->subState = 0;
                (D_801D3EC0[node->counter] + D_801D3E78)->status = 2;
                (D_801D3EC0[node->counter] + D_801D3E78)->field02 = 0;
                playTriadSfx(9);
            }
            return 0;
        case 2:
            if (node->subState == 0) {
                if (func_8009EF68() != 0) {
                ret0:
                    return 0;
                }
                func_800A44B0(0);
                func_800A1D68(6, (u8 *)&D_80182686 - 6 + D_80182686, 0);
                node->subState++;
            }
            r = func_800A20F4(6);
            if (r < 0) {
                goto ret0;
            }
            func_800A2054(6);
            if (r == 0) {
                node->state = 3;
                node->subState = 0;
                break;
            }
            func_800A44B0(1);
            node->state = 1;
            node->subState = 0;
            break;
        case 3:
            if (node->subState == 0) {
                func_800A44BC();
            }
            node->subState++;
            return ((node->subState & 0xFF) >= 0xF) << 1;
        }
    }
}

/**
 * @brief Build a player's Triple Triad hand by drawing cards.
 *
 * If @p arg1 is non-zero it first deals up to 5 cards of that rarity/type from the
 * @c D_80078658 table (cards 0x4D+), each gated by an RNG roll against the deal
 * threshold @c D_80082C90.field_09 (halved after the first hit). It then fills the
 * remaining slots by drawing from random tiers: @c D_80082C90.field_08 is a 7-bit
 * tier mask whose set bits build @c tierList (tier bases 0, 0xB, 0x16, ...), and each
 * card is @c tierList[rng%numTiers] + rng%11, rejecting the blocked card 0x2F and any
 * duplicate already in the hand. Results are written to @c D_801A2C48[player].
 *
 * @param player Hand index (0 or 1) — selects the 5-slot row in @c D_801A2C48.
 * @param arg1   Rarity/type filter for the seeded deal; 0 skips the seeded phase.
 *
 * @note The reject flag reuses @c rng1, and the @c do{}while(0) wrapping its reset is a
 *       scheduling barrier that keeps the reset after the draw (matches the original).
 *       The rarity probe uses @c *((rarity + i) + 0x4D) — the @c rarity[i + 0x4D] form
 *       reassociates the offset and does not match.
 */
void func_8009F5F0(s32 player, s32 arg1) {
    s32 count;
    s32 i;
    s32 threshold;
    s32 tierList[7];
    s32 numTiers;
    s32 bits;
    s32 tierVal;
    s32 card;
    s32 rng1;
    s32 rng2;
    s32 j;
    u8 *rarity;

    count = 0;
    rarity = D_80078658;
    threshold = D_80082C90.field_09;
    if (arg1 != 0) {
        for (i = 0; i < 0x21; i++) {
            if (*((rarity + i) + 0x4D) == arg1) {
                if ((func_80023D04() % 100) < threshold) {
                    D_801A2C48[player][count] = i + 0x4D;
                    count++;
                    threshold = D_80082C90.field_09 >> 1;
                    if (count >= 5) {
                        break;
                    }
                }
            }
        }
    }
    numTiers = 0;
    if (D_80082C90.field_08 == 0) {
        bits = 1;
    } else {
        bits = D_80082C90.field_08;
    }
    i = 0;
    tierVal = i;
    for (; i < 7; i++) {
        if ((bits >> i) & 1) {
            tierList[numTiers] = tierVal;
            numTiers++;
        }
        tierVal += 0xB;
    }
    for (i = count; i < 5; i++) {
        do {
            rng1 = func_80023D04();
            rng2 = func_80023D04();
            card = tierList[rng1 % numTiers] + (rng2 % 11);
            do { rng1 = 0; } while (0);
            if (card == 0x2F) {
                rng1 = 1;
            } else {
                for (j = 0; j < i; j++) {
                    if (card == D_801A2C48[player][j]) {
                        rng1 = 1;
                        break;
                    }
                }
            }
        } while (rng1 != 0);
        D_801A2C48[player][i] = card;
    }
}

/**
 * @brief Build a random 5-card hand for the given player by drawing owned cards.
 *
 * Repeatedly draws a random card id in 0..109 (@c func_80023D04 % 110); whenever
 * the player owns at least one of that card (@c func_80023B14 > 0) it places the id
 * in the player's hand row @c D_801A2C48[hand] and adjusts its owned quantity via
 * @c modifyItemQuantity. Continues until 5 cards have been placed.
 *
 * @param hand Player index (0 or 1); selects the 5-slot row in @c D_801A2C48.
 * @param arg1 Quantity delta passed to @c modifyItemQuantity for each drawn card.
 */
void func_8009F844(s32 hand, s32 arg1) {
    u8 *row;
    u8 *b;
    s32 i;
    s32 card;

    b = D_801A2C48[0];
    i = 0;
    row = b + hand * 5;
loop:
    card = func_80023D04() % 110;
    if (func_80023B14(card) > 0) {
        row[i] = card;
        i++;
        modifyItemQuantity(card, arg1);
        do {
        } while (0);
    }
    if (i < 5) {
        goto loop;
    }
}

/**
 * @brief Per-frame handler: copies a player's hand into the script-action table.
 *
 * Drives one card slot per 5 frames. @c node->state is the current slot index
 * (0..4) and doubles as the state; @c node->counter is the player index. On the
 * first frame of each slot (@c subState 0) it copies the hand card
 * @c D_801A2C48[player][slot] into @c D_801D3EC0[player][slot]: status 1, the
 * card id as the @c marker, and an @c actionId selected by the layout type
 * (@c 0x800 for the offset-hand layout @c D_801A2C70 >= 3, else 0). After 5
 * frames it advances to the next slot. Once all slots are done (@c state >= 5)
 * it returns 2 if no queued actions remain (@c func_8009EF68 == 0), else 0.
 *
 * @param node Handler context (state/slot at 0x10, subState at 0x11, player at 0x12).
 * @return 2 when all hands are queued and drained, else 0.
 *
 * @note The @c status / @c field02 stores use the pointer form
 *       @c (D_801D3EC0[player]+slot)->field to force the row index into the lower
 *       register (row-major codegen); the @c marker / @c actionId stores keep the
 *       array form, matching the original register allocation.
 */
s32 func_8009F908(ScriptCtx *node) {
    u8 card;

    if (node->state >= 5) {
        return (func_8009EF68() == 0) << 1;
    }
    if (node->subState == 0) {
        card = D_801A2C48[node->counter][node->state];
        (D_801D3EC0[node->counter] + node->state)->status = 1;
        (D_801D3EC0[node->counter] + node->state)->field02 = 0;
        D_801D3EC0[node->counter][node->state].marker = card;
        if (D_801A2C70[node->counter] < 3) {
            D_801D3EC0[node->counter][node->state].actionId = 0;
        } else {
            D_801D3EC0[node->counter][node->state].actionId = 0x800;
        }
    }
    node->subState++;
    if (node->subState >= 5) {
        node->subState = 0;
        node->state++;
    }
    return 0;
}

/**
 * @brief Set up the per-player hand and spawn its setup handler, per the active rules.
 *
 * Initialises the @c D_801D3EA0 pool, then (by @c g_tripleTriadRules) builds the
 * player's hand and chooses the per-frame handler:
 *  - rule bit @c 0x20000000: random hand (@c func_8009F5F0, delta 0) + @c func_8009F908.
 *  - rule bit @c 0x8: open hand — @c func_8009F5F0 if this player uses the offset-hand
 *    layout (@c D_801A2C70 >= 3) else @c func_8009F844 — + @c func_8009F908.
 *  - otherwise: @c func_8009F5F0 + @c func_8009F908 for the offset-hand layout, else the
 *    interactive @c func_8009F17C.
 *
 * The spawned node's @c counter is set to @p arg0 and its state cleared.
 *
 * @param arg0 Player/slot index.
 * @return The initialised pool (@c D_801D3EA0).
 * @note The third branch's @c D_801A2C70[node->counter] test reads @c node before it is
 *       assigned (the original relies on the stale register value); preserved to match.
 */
s32 func_8009FAF8(s32 arg0) {
    ScriptCtx *node;

    initObjList(D_801D3EA0, D_801D3E80, 0x14, 1);

    if (g_tripleTriadRules & 0x20000000) {
        func_8009F5F0(arg0, 0);
        node = (ScriptCtx *)allocObjNode(D_801D3EA0, (ObjNodeFn)func_8009F908);
    } else if (g_tripleTriadRules & 0x8) {
        if (D_801A2C70[arg0] >= 3) {
            func_8009F5F0(arg0, D_80082C95);
        } else {
            func_8009F844(arg0, D_80082C95);
        }
        node = (ScriptCtx *)allocObjNode(D_801D3EA0, (ObjNodeFn)func_8009F908);
    } else if (D_801A2C70[node->counter] >= 3) {
        func_8009F5F0(arg0, D_80082C95);
        node = (ScriptCtx *)allocObjNode(D_801D3EA0, (ObjNodeFn)func_8009F908);
    } else {
        node = (ScriptCtx *)allocObjNode(D_801D3EA0, (ObjNodeFn)func_8009F17C);
    }

    node->counter = arg0;
    node->state = 0;
    node->subState = 0;

    return (s32)D_801D3EA0;
}

/**
 * @brief Add a rendering command entry based on the alternate screen index.
 *
 * Reads g_drawBufferIndex, XORs with 1 to get the alternate index, computes
 * an offset of index * 92 into g_drawEnvs, and calls queueLoadImage
 * with the resulting pointer and D_8012E66C.
 *
 * @return Always 0.
 */
s32 func_8009FC40(void) {
    s32 idx = g_drawBufferIndex ^ 1;
    queueLoadImage(&g_drawEnvs[idx].clip, D_8012E66C);
    return 0;
}

extern void func_800A030C(s32 a0);

/**
 * @brief Battle-script callback: 5-state machine driving an intro/setup sequence.
 *
 * Registered via initTripleTriadScripts. Each invocation advances the state machine
 * one tick; returns 0 while running, returns from any case.
 *
 * State 0: warmup. Calls func_800A030C(0xF) once, ticks 15 frames.
 * State 1: setup. Calls func_8009FAF8(counter) per counter, polls
 *          updateObjectList until ready; tries counter 0..1, then branches
 *          to state 2 (if g_tripleTriadRules & 1) or state 4.
 * State 2: clear sweep. Every 5 ticks, marks queued actions in
 *          D_801D3EC0[row][col] complete for column = 4 down to 0;
 *          transitions to state 3 once column 0 is processed.
 * State 3: wait. Calls func_8009EF68 once, idles 0x1E frames.
 * State 4: done. Sets g_tripleTriadState = TT_STATE_PLAY and exits.
 *
 * @param ctx Callback context (state at +0x10, subState at +0x11).
 * @return 0 while progressing, 0 on completion.
 */
s32 func_8009FC90(ScriptCtx *ctx) {
    while (1) {
        switch (ctx->state) {
        case 0:
            if (ctx->subState == 0) {
                func_800A030C(0xF);
            }
            ctx->subState++;
            if (ctx->subState < 0xF) {
                return 0;
            }
            ctx->counter = 0;
            ctx->state = 1;
            ctx->subState = 0;
            break;

        case 1:
            if (ctx->subState == 0) {
                ctx->cachedResult = func_8009FAF8(ctx->counter);
                ctx->subState++;
            }
            if (updateObjectList((u8 *)ctx->cachedResult) != 0) {
                return 0;
            }
            ctx->counter++;
            if (ctx->counter < 2) {
                ctx->state = 1;
                ctx->subState = 0;
                break;
            }
            if (g_tripleTriadRules & 1) {
                ctx->state = 2;
                ctx->subState = 0;
                break;
            }
            ctx->state = 4;
            ctx->subState = 0;
            break;

        case 2: {
            s32 row;
            s32 col;
            if ((u8)(ctx->subState % 5) == 0) {
                col = 4 - (u8)(ctx->subState / 5);
                for (row = 0; row < 2; row++) {
                    if (D_801D3EC0[row][col].actionId != 0) {
                        D_801D3EC0[row][col].status = 3;
                        D_801D3EC0[row][col].field02 = 0;
                    }
                }
                if (col == 0) {
                    ctx->state = 3;
                    ctx->subState = 0;
                }
            }
            ctx->subState++;
            return 0;
        }

        case 3:
            if (ctx->subState == 0) {
                if (func_8009EF68() != 0) {
                    return 0;
                }
            }
            ctx->subState++;
            if (ctx->subState < 0x1E) {
                return 0;
            }
            ctx->state = 4;
            ctx->subState = 0;
            break;

        case 4:
            g_tripleTriadState = TT_STATE_PLAY;
            return 0;
        }
    }
}

/**
 * @brief Per-frame display-node spawn for some D_801D44FC-driven animation.
 *
 * Validates the current slot index in D_801D44FC, allocates a 40-byte
 * @c DispNode via @c scratchAlloc, and initializes it based on the
 * current phase counter (@c D_801D3EB0 / @c D_801D3EB8):
 * - phase < 10:  rotating-arc setup, angle scaled via @c rsin
 * - phase < 300: simple static node (scale = 0x18)
 * - else:        squashed angle via @c rsin + phase mirror
 *
 * On any failure (slot out of range or @c func_80023B14 returns negative),
 * sets @c D_80182E64 = @c -1 and returns 0.
 *
 * @return Always 0.
 */
s32 func_8009FED0(void) {
    DispNode *node;
    s32 cur;

    if ((u32)D_801D44FC < 0x6E) {
        if (func_80023B14(D_801D44FC) >= 0) {
            node = (DispNode *)scratchAlloc(0x28);
            if (D_80182E64 != D_801D44FC) {
                D_801D3EB8 = 0;
                D_801D3EB0 = 0;
            }
            cur = D_801D3EB0;
            if (cur < 0xA) {
                s32 newCur = cur + 1;
                s32 v = (newCur << 12) / 10;
                D_801D3EB0 = newCur;
                D_801D3EB4 = v;
                D_801D3EB4 = rsin(v / 4);
                node->unk04 = 0;
                node->phase = 0;
                node->angle = 0;
                RotMatrixYXZ(node, node->subNode);
                {
                    s32 r = D_801D3EB4;
                    node->charType = 0x44;
                    node->unk24 = 0x200;
                    node->scale = (-(r * 88) >> 12) + 0x70;
                }
            } else if (cur < 0x12C) {
                D_801D3EB0 = cur + 1;
                node->unk04 = 0;
                node->phase = 0;
                node->angle = 0;
                RotMatrixYXZ(node, node->subNode);
                node->charType = 0x44;
                node->scale = 0x18;
                node->unk24 = 0x200;
            } else {
                s32 newScale = D_801D3EB8 + 0x20;
                s32 rounded;
                s32 v;
                D_801D3EB8 = newScale;
                rounded = newScale + (s32)((u32)newScale >> 31);
                v = rsin(rounded >> 1);
                v = -v;
                if (v < 0) v += 7;
                node->angle = v >> 3;
                node->unk04 = 0;
                node->phase = (u16)D_801D3EB8;
                RotMatrixYXZ(node, node->subNode);
                node->charType = 0x44;
                node->scale = 0x18;
                node->unk24 = 0x200;
            }
            SetRotMatrix(node->subNode);
            SetTransMatrix(node->subNode);
            g_primCursor = drawTriadCard((u8)D_801D44FC, 0x13, &g_otBase[3], g_primCursor);
            scratchFree(0x28);
            D_80182E64 = D_801D44FC;
        } else {
            D_80182E64 = -1;
        }
    } else {
        D_80182E64 = -1;
    }
    return 0;
}

/**
 * @brief Initialise the script-handler subsystem and its object pool.
 *
 * Sets up the @c D_801D3FA0 pool (10 entries of 0x14 bytes), spawns the
 * persistent handler nodes (state machine @c func_8009FC90, @c func_8009EBF4,
 * @c func_8009FED0, @c func_8009FC40), and clears the @c D_801D3EC0 2x5
 * @c ScriptEntry table (each entry tagged with @c marker 0xFF and its
 * row/column cached).
 *
 * @return The initialised pool (@c D_801D3FA0).
 */
u8 *initTripleTriadScripts(void) {
    ScriptCtx *node;
    ScriptEntry *e;
    u8 *base;
    s32 row;
    s32 rowOff;
    s32 marker;
    s32 col;
    s32 colOff;

    initObjList(D_801D3FA0, D_801D3FB0, 0x14, 0xA);
    node = (ScriptCtx *)allocObjNode(D_801D3FA0, (ObjNodeFn)func_8009FC90);
    node->state = 0;
    node->subState = 0;
    allocObjNode(D_801D3FA0, (ObjNodeFn)func_8009EBF4);
    row = 0;
    base = (u8 *)D_801D3EC0;
    marker = 0xFF;
    rowOff = row;
    do {
        col = 0;
        colOff = rowOff;
        do {
            e = (ScriptEntry *)(colOff + (s32)base);
            e->col = col;
            e->status = 0;
            e->marker = marker;
            e->row = row;
            e->field06 = 0;
            e->actionId = 0;
            e->field0A = 0;
            col++;
            colOff += 0x16;
        } while (col < 5);
        row++;
        rowOff += 0x6E;
    } while (row < 2);
    allocObjNode(D_801D3FA0, (ObjNodeFn)func_8009FED0);
    allocObjNode(D_801D3FA0, (ObjNodeFn)func_8009FC40);
    return D_801D3FA0;
}

/**
 * @brief Per-frame handler for a screen colour-fade (@c FadeObject).
 *
 * Builds a full-screen TILE primitive whose colour is interpolated from
 * @c startColor toward @c endColor by the elapsed fraction, links it (plus a
 * trailing blend-control primitive) into OT bucket 2, and advances the
 * primitive pool. Returns 0 while fading; once @c frame reaches @c duration it
 * stages @c endColor into @c D_80182E68 and returns 2 (fade complete).
 *
 * @param obj The fade object.
 * @return 0 while in progress, 2 when complete.
 */
s32 func_800A01DC(FadeObject *obj) {
    TILE *prim;
    s32 progress;
    s32 frame;

    prim = g_primCursor;
    prim->tag = 0x5000000;
    prim->w = 0x180;
    prim->y0 = 0;
    prim->x0 = 0;
    prim->h = 0xE0;
    progress = (obj->frame << 12) / obj->duration;
    func_800408A4(obj->endColor[0], obj->endColor[1], obj->endColor[2]);
    func_80040918(obj->startColor, progress, &prim->r0);
    prim->code = 0x62;
    AddPrim(g_otBase + 2, prim);
    func_8004D724((u8 *)prim + 0x10, 1, 1, 0x40);
    AddPrim(g_otBase + 2, (u8 *)prim + 0x10);
    g_primCursor = (u8 *)prim + 0x18;
    frame = (u16)obj->frame;
    obj->frame = frame + 1;
    if ((s16)frame >= obj->duration) {
        memcpy(D_80182E68, obj->endColor, 4);
        return 2;
    }
    return 0;
}

/**
 * @brief Start a full-screen fade to black over @p duration frames.
 *
 * Spawns a @c FadeObject driven by @c func_800A01DC. The fade begins from the
 * currently-staged color (@c D_80182E68) and ends at black; black is then
 * staged so any following fade starts from black.
 *
 * @param duration Fade length in frames.
 */
void func_800A030C(s32 duration) {
    FadeObject *obj;

    obj = (FadeObject *)func_8009E248((ObjNodeFn)func_800A01DC);
    if (obj != NULL) {
        obj->frame = 0;
        obj->duration = duration;
        memcpy(obj->startColor, D_80182E68, 4);
        *(s32 *)D_80182E68 = 0;
        *(s32 *)obj->endColor = 0;
    }
}

/**
 * @brief Start a full-screen fade to white over @p duration frames.
 *
 * Identical to @c func_800A030C but ends at white (0xFFFFFF), which is also
 * staged into @c D_80182E68 for the next fade.
 *
 * @param duration Fade length in frames.
 */
void func_800A0370(s32 duration) {
    FadeObject *obj;

    obj = (FadeObject *)func_8009E248((ObjNodeFn)func_800A01DC);
    if (obj != NULL) {
        obj->frame = 0;
        obj->duration = duration;
        memcpy(obj->startColor, D_80182E68, 4);
        *(s32 *)D_80182E68 = 0xFFFFFF;
        *(s32 *)obj->endColor = 0xFFFFFF;
    }
}

/**
 * @brief Per-frame board render/update loop for the post-game card-claim board.
 *
 * Walks the ten board cells (@c D_801D4308). For each, it builds a rotation/translation
 * @c MATRIX from the cell's @c rot* and @c base* fields, then runs the cell's animation
 * state (@c field8): 1 = slide in from off-screen; 2 = flip (rotates @c rotY, toggles
 * @c fieldA owner at the half-way point, bobs @c baseZ); 3/4 = capture lift-and-fade
 * (rises, shows the captured card's name banner, then settles) for the two capture
 * directions. Cells with @c flags bit 0 set are transformed (@c SetRotMatrix /
 * @c SetTransMatrix) and drawn via @c drawTriadCard into the OT at @c sort + 9.
 *
 * @return 0 (driven again next frame by the spawning handler).
 */
s32 func_800A03DC(void) {
    MATRIX *m;
    s32 i;
    s32 s0;
    ActiveObj *cell;
    s32 s7;
    s32 t;
    s32 n;
    s32 inc;

    m = (MATRIX *)scratchAlloc(0x20);
    for (i = 0; i < 10; i++) {
        cell = &D_801D4308[i];
        RotMatrixYXZ(&cell->rotX, m);
        m->t[0] = cell->baseX;
        m->t[1] = cell->baseY;
        m->t[2] = cell->baseZ;
        s7 = cell->fieldA | 0x12;
        if (cell->field8) switch (cell->field8) {
        case 1:
            s0 = (cell->field9 << 12) / 30;
            s0 = rsin(s0 / 4);
            if (cell->fieldA == 0) {
                m->t[0] = ((cell->baseX + 0x1A0) * s0 >> 12) - 0x1A0;
            } else {
                m->t[0] = ((cell->baseX - 0x1A0) * s0 >> 12) + 0x1A0;
            }
            cell->field9++;
            if ((cell->field9 & 0xFF) >= 0x1E) {
                cell->field8 = 0;
                cell->field9 = 0;
            }
            break;
        case 2:
            if (cell->field9 == 0) {
                cell->sort -= 5;
                playTriadSfx(0x5A);
            }
            inc = cell->field9 + 1;
            cell->field9 = inc;
            n = inc & 0xFF;
            s0 = (n << 12) / 20;
            if (n == 0xA) {
                cell->fieldA ^= 1;
            }
            cell->rotY = s0;
            t = rsin(s0 / 2);
            cell->baseZ = 0x200 - ((t << 5) >> 12);
            if ((cell->field9 & 0xFF) >= 0x14) {
                cell->field8 = 0;
                cell->field9 = 0;
                cell->sort += 5;
            }
            break;
        case 3:
            s0 = cell->field9;
            if (s0 < 0x1E) {
                if (s0 == 0) {
                    cell->sort = 0;
                    playTriadSfx(0x59);
                }
                s0 = (s0 << 12) / 30;
                t = rsin(s0 / 2);
                m->t[0] = cell->baseX + (-cell->baseX * s0 >> 12);
                m->t[1] = cell->baseY + (-cell->baseY * s0 >> 12) + (-((t * 7) << 4) >> 12);
                m->t[2] = cell->baseZ + ((0x100 - cell->baseZ) * s0 >> 12);
                cell->field9++;
            } else {
                s0 -= 0x1E;
                if (s0 < 2) {
                    m->t[0] = 0;
                    m->t[1] = 0;
                    m->t[2] = 0x100;
                    if (s0 == 0) {
                        strcpy(D_801D4078, func_80023A54(cell->cardId));
                        func_80047C74(D_801D4078, (u8 *)&D_80182692 - 0x12 + D_80182692);
                        func_800A1D68(1, D_801D4078, 0);
                        cell->field9++;
                    } else if ((g_padPressed[0] | g_padPressed[1]) != 0) {
                        D_801D4454 = 1;
                        func_800A2054(1);
                        playTriadSfx(isItemPresent(cell->cardId) ? 0x59 : 0x12);
                        cell->field9++;
                    }
                } else {
                    s0 -= 2;
                    if (s0 < 0xF) {
                        m->t[0] = 0;
                        m->t[2] = 0x100;
                        s0 = (s0 << 12) / 15;
                        m->t[1] = ((s0 * 7) << 4) >> 12;
                        cell->field9++;
                    } else {
                        cell->field8 = 0;
                        cell->field9 = 0;
                        cell->flags &= ~1;
                    }
                }
            }
            break;
        case 4:
            s0 = cell->field9;
            if (s0 < 0x1E) {
                if (s0 == 0) {
                    cell->sort = 0;
                    playTriadSfx(0x59);
                }
                s0 = (s0 << 12) / 30;
                t = rsin(s0 / 2);
                m->t[0] = cell->baseX + (-cell->baseX * s0 >> 12);
                m->t[1] = cell->baseY + (-cell->baseY * s0 >> 12) + (((t * 7) << 4) >> 12);
                m->t[2] = cell->baseZ + ((0x100 - cell->baseZ) * s0 >> 12);
                cell->field9++;
            } else {
                s0 -= 0x1E;
                if (s0 < 2) {
                    m->t[0] = 0;
                    m->t[1] = 0;
                    m->t[2] = 0x100;
                    if (s0 == 0) {
                        strcpy(D_801D4078, func_80023A54(cell->cardId));
                        func_80047C74(D_801D4078, (u8 *)&D_80182696 - 0x16 + D_80182696);
                        func_800A1D68(1, D_801D4078, 0);
                        cell->field9++;
                    } else if ((g_padPressed[0] | g_padPressed[1]) != 0) {
                        D_801D4454 = 1;
                        func_800A2054(1);
                        playTriadSfx(0x59);
                        cell->field9++;
                    }
                } else {
                    s0 -= 2;
                    if (s0 < 0xF) {
                        m->t[0] = 0;
                        m->t[2] = 0x100;
                        s0 = (s0 << 12) / 15;
                        m->t[1] = -((s0 * 7) << 4) >> 12;
                        cell->field9++;
                    } else {
                        cell->field8 = 0;
                        cell->field9 = 0;
                        cell->flags &= ~1;
                    }
                }
            }
            break;
        }
        if (cell->flags & 1) {
            SetRotMatrix(m);
            SetTransMatrix(m);
            g_primCursor = drawTriadCard(cell->cardId, s7, g_otBase + (cell->sort + 9), g_primCursor);
        }
    }
    scratchFree(0x20);
    return 0;
}

/**
 * @brief Check if any active card object has a pending action.
 *
 * Iterates through 10 entries in D_801D4308 (stride 0x20). For each entry
 * where bit 0 of the word at offset +4 is set and the byte at offset +8
 * is non-zero, returns 1. Returns 0 if no such entry is found.
 *
 * @return 1 if any active object has a pending action, 0 otherwise.
 */
s32 func_800A0A88(void) {
    s32 i = 0;
    u8 *entry = D_801D4308;

    top:
    if ((*(s32 *)(entry + 4) & 1) && *(u8 *)(entry + 8) != 0) {
        return 1;
    }
    i++;
    entry += 0x20;
    if (i < 10) goto top;

    return 0;
}

/**
 * @brief Add a rendering command for the alternate screen buffer.
 *
 * Reads g_drawBufferIndex, XORs with 1 to get the alternate index, computes
 * an offset of index * 92 into g_drawEnvs, and calls queueLoadImage
 * with the resulting pointer and D_80158680.
 *
 * @return Always 0.
 */
s32 func_800A0AD4(void) {
    s32 idx = g_drawBufferIndex ^ 1;
    queueLoadImage(&g_drawEnvs[idx].clip, D_80158680);
    return 0;
}

/**
 * @brief Interactive "keep a captured card" selection sequence (Random/Trade rule).
 *
 * State 0 builds a banner string into @c D_801D4188 — a prefix (string-pool entry
 * @c D_8018268A), the acting seat as a letter (@c D_801D4448 + '!'), then a suffix
 * (@c D_8018268E) — and shows it. State 1 runs the picker: it opens a selection
 * substate, then on the player's choice (@c D_801D3359) either shows a card's detail
 * (1), toggles a card for capture while tracking the remaining budget @c D_801D4178
 * (2), or deselects one (3). State 2 confirms via @c func_800A20F4: on accept it sets
 * @c D_801D444C and returns 2; on cancel it returns to the picker.
 *
 * @param node State node (state 0x0C, per-pass guard 0x0D, acting seat 0x0E).
 * @return 2 once the selection is confirmed, else 0.
 */
s32 func_800A0B24(ScriptStateNode *node) {
    /* 8 bytes of stack the original reserves (frame vars=8); unreferenced on every
       path. Purpose uncertain — likely leftover scratch from the string build above. */
    u8 scratch[4];
    u8 *dst;
    u8 *src;
    u8 c;
    u8 c2;
    ActiveObj *cell;
    s32 r;

    while (1) {
        switch (node->state) {
        case 0:
            src = (u8 *)&D_8018268A - 0xA + D_8018268A;
            dst = D_801D4188;
            c = *src;
            while (c != 0) {
                src++;
                *dst = c;
                c = *src;
                dst++;
            }
            *dst = D_801D4448 + 0x21;
            src = (u8 *)&D_8018268E - 0xE + D_8018268E;
            while (dst++, (c2 = *src) != 0) {
                src++;
                *dst = c2;
            }
            *dst = 0;
            func_800A1D68(1, D_801D4188, 0);
            D_801D4178 = 0;
            node->state = 1;
            node->field0D = 0;
            break;
        case 1:
            if (node->field0D == 0) {
                if (func_800A0A88() != 0) {
                    return 0;
                }
                activateMenuSubstate((node->field0E ^ 1) + 4, 0, 2, 0);
                node->field0D++;
                break;
            }
            switch (D_801D3359) {
            case 1:
                showCardDetail(D_801D4308[D_801D335C.field0].cardId);
                return 0;
            case 2:
                cell = &D_801D4308[D_801D335C.field0];
                if (cell->fieldA != node->field0E) {
                    if (D_801D4178 < D_801D4448) {
                        cell->field8 = 2;
                        D_801D4308[D_801D335C.field0].field9 = 0;
                        playTriadSfx(1);
                        D_801D4178++;
                    } else {
                        node->state = 1;
                        node->field0D = 0;
                        playTriadSfx(0x10);
                        continue;
                    }
                } else {
                    if (D_801D4178 <= 0) {
                        break;
                    }
                    cell->field8 = 2;
                    D_801D4308[D_801D335C.field0].field9 = 0;
                    playTriadSfx(9);
                    D_801D4178--;
                }
                if (D_801D4178 == D_801D4448) {
                    node->state = 2;
                    node->field0D = 0;
                    continue;
                }
                node->state = 1;
                node->field0D = 0;
                return 0;
            case 3:
                cell = &D_801D4308[D_801D335C.field0];
                if (cell->fieldA == node->field0E) {
                    cell->field8 = 2;
                    D_801D4308[D_801D335C.field0].field9 = 0;
                    playTriadSfx(9);
                    D_801D4178--;
                }
                break;
            default:
                continue;
            }
            node->state = 1;
            node->field0D = 0;
            break;
        case 2:
            if (node->field0D == 0) {
                if (func_800A0A88() != 0) {
                    return 0;
                }
                node->field0D++;
                break;
            }
            if (node->field0D == 1) {
                func_800A1D68(6, (u8 *)&D_80182686 - 6 + D_80182686, 0);
                node->field0D++;
                break;
            }
            r = func_800A20F4(6);
            switch (r) {
            case 0:
                func_800A2054(6);
                func_800A2054(1);
                D_801D444C = 1;
                return 2;
            case 1:
                func_800A2054(6);
                node->state = 1;
                node->field0D = 0;
                continue;
            }
            return 0;
        }
    }
}

/**
 * @brief Per-frame AI move-selection state machine.
 *
 * State 0 resets the placed-count (@c D_801D4288). State 1 alternates between a
 * gate tick (waits while @c func_800A0A88 reports a pending action; finishes with
 * @c D_801D444C=1, return 2 once @c D_801D4288 reaches @c D_801D4448) and a scan
 * tick that picks the capturable object (owner != @c node->field0E, @c flags bit 0
 * set) with the highest card level (@c g_tripleTriadCardStats[cardId] level byte),
 * marks it (0x08=2, 0x09=0), and bumps @c D_801D4288.
 *
 * @param node The driver node.
 * @return 0 while blocked, 2 when the move sweep completes.
 */
s32 func_800A0F0C(ScriptStateNode *node) {
    s32 bestVal;
    s32 bestIdx;
    s32 i;

    while (1) {
        switch (node->state) {
        case 0:
            D_801D4288 = 0;
            node->state = 1;
            node->field0D = 0;
            break;
        case 1:
            if (node->field0D == 0) {
                if (func_800A0A88() != 0) {
                    return 0;
                }
                if (D_801D4288 == D_801D4448) {
                    D_801D444C = 1;
                    return 2;
                }
                node->field0D++;
                break;
            }
            bestVal = 0;
            bestIdx = 0;
            for (i = 0; i < 10; i++) {
                if (D_801D4308[i].fieldA != node->field0E &&
                    (D_801D4308[i].flags & 1) &&
                    bestVal < g_tripleTriadCardStats[D_801D4308[i].cardId].pad05[0]) {
                    bestVal = g_tripleTriadCardStats[D_801D4308[i].cardId].pad05[0];
                    bestIdx = i;
                }
            }
            D_801D4308[bestIdx].field8 = 2;
            D_801D4308[bestIdx].field9 = 0;
            D_801D4288++;
            node->state = 1;
            node->field0D = 0;
            break;
        }
    }
}

/**
 * @brief Per-frame state machine that consumes each played hand card from the
 *        working board/hand state, one move per pass.
 *
 * State 0 seeds the per-player working hand copy @c D_801D4298 from the match
 * hands @c D_801A2C48 (2 players x 5 cards), then advances to state 1.
 *
 * State 1 walks @c g_tripleTriadCardHands one move at a time (index @c node->index,
 * advanced each pass while @c func_800A0A88 reports no pending action). For the
 * current move it takes the card id and owning seat (bit 0 of @c initFlags) and
 * marks that card consumed: if the card is still in the owner's working hand
 * @c D_801D4298[owner] it is stamped @c 0xFF; otherwise the matching board cell in
 * @c D_801D4308 owned by the *other* seat is flagged (@c field8 = 2, @c field9 = 0)
 * to trigger its flip.
 *
 * @param node State-machine driver node.
 * @return 0 while @c func_800A0A88 reports a pending action; 2 once all 10 moves
 *         have been processed (also sets @c D_801D444C = 1).
 */
s32 func_800A1080(ScriptStateNode *node) {
    s32 i;
    s32 col;
    s32 owner;
    u8 card;

    while (1) {
        switch (node->state) {
        case 0:
            for (i = 0; i < 2; i++) {
                for (col = 0; col < 5; col++) {
                    D_801D4298[i][col] = D_801A2C48[i][col];
                }
            }
            node->index = 0;
            node->state = 1;
            node->field0D = 0;
            break;
        case 1:
            if (node->field0D == 0) {
                if (func_800A0A88() != 0) {
                    return 0;
                }
                if (node->index >= 10) {
                    D_801D444C = 1;
                    return 2;
                }
                node->field0D++;
            }
            owner = g_tripleTriadCardHands[node->index].initFlags & 1;
            card = g_tripleTriadCardHands[node->index].cardId;
            for (i = 0; i < 5; i++) {
                if (D_801D4298[owner][i] == card) {
                    D_801D4298[owner][i] = 0xFF;
                    break;
                }
            }
            if (i >= 5) {
                for (i = 0; i < 10; i++) {
                    if (D_801D4308[i].cardId == card &&
                        D_801D4308[i].fieldA != owner) {
                        D_801D4308[i].field8 = 2;
                        D_801D4308[i].field9 = 0;
                        break;
                    }
                }
            }
            node->state = 1;
            node->field0D = 0;
            node->index++;
            break;
        }
    }
}

/**
 * @brief Per-frame state machine that sweeps the 10 D_801D4308 cells.
 *
 * State 0 resets the scan (index = 0). State 1 walks one cell per tick:
 * if the cell's 0x0A field equals @c node->field0E XOR 1, the cell is marked
 * (0x08 = 2, 0x09 = 0). When all 10 cells are processed it sets @c D_801D444C
 * and returns 2; if @c func_800A0A88 reports a pending action it returns 0.
 *
 * @param node The driver node.
 * @return 0 if blocked by a pending action, 2 when the sweep completes.
 */
s32 func_800A1260(ScriptStateNode *node) {
    while (1) {
        switch (node->state) {
        case 0:
            node->index = 0;
            node->state = 1;
            node->field0D = 0;
            break;
        case 1:
            if (node->field0D == 0) {
                if (func_800A0A88() != 0) {
                    return 0;
                }
                if (node->index >= 10) {
                    D_801D444C = 1;
                    return 2;
                }
                node->field0D++;
            }
            if (D_801D4308[node->index].fieldA == (node->field0E ^ 1)) {
                D_801D4308[node->index].field8 = 2;
                D_801D4308[node->index].field9 = 0;
            }
            node->state = 1;
            node->field0D = 0;
            node->index++;
            break;
        }
    }
}

/**
 * @brief Post-round capture/cleanup sweep over the board.
 *
 * State machine (one cell advanced per pass while @c func_800A0A88 reports no pending
 * action). State 0 waits for the action gate. State 1 sweeps the opponent row
 * (@c D_801D4450 ^ 1): each card owned by the acting seat is flagged @c field8 = 3.
 * State 2 sweeps the acting seat's row (@c D_801D4450): each card NOT owned by it is
 * flagged @c field8 = 4. State 3 returns every acting-seat card to its collection
 * (@c markItemPresent) and finishes, setting @c D_801D444D and returning 2 — unless held
 * by @c D_801D4454 / @c g_padPressed[2], in which case it returns 0.
 *
 * @param node State node (state 0x0C, per-pass guard 0x0D, cell index 0x0F).
 * @return 0 while a pass is pending or gated; 2 once the cleanup completes.
 */
s32 func_800A1374(ScriptStateNode *node) {
    s32 i;
    ActiveObj *c;

    while (1) {
        switch (node->state) {
        case 0:
            if (func_800A0A88() != 0) {
                return 0;
            }
            node->state = 1;
            node->field0D = 0;
            node->index = 0;
            break;
        case 1:
            if (node->field0D == 0) {
                if (node->index < 5) {
                    c = &D_801D4308[(D_801D4450 ^ 1) * 5 + node->index];
                    if (c->fieldA == D_801D4450) {
                        c->field8 = 3;
                        c->field9 = 0;
                    }
                    node->index++;
                    node->field0D++;
                } else {
                    node->state = 2;
                    node->field0D = 0;
                    node->index = 0;
                    break;
                }
            }
            if (func_800A0A88() != 0) {
                return 0;
            }
            node->field0D = 0;
            break;
        case 2:
            if (node->field0D == 0) {
                if (node->index < 5) {
                    c = &D_801D4308[D_801D4450 * 5 + node->index];
                    if (c->fieldA != D_801D4450) {
                        c->field8 = 4;
                        c->field9 = 0;
                    }
                    node->index++;
                    node->field0D++;
                } else {
                    node->state = 3;
                    node->field0D = 0;
                    node->index = 0;
                    break;
                }
            }
            if (func_800A0A88() != 0) {
                return 0;
            }
            node->field0D = 0;
            break;
        case 3:
            if (node->field0D == 0) {
                for (i = 0; i < 10; i++) {
                    if (D_801D4308[i].fieldA == D_801D4450) {
                        markItemPresent(D_801D4308[i].cardId);
                    }
                }
                node->field0D++;
            } else {
                if (D_801D4454 == 0) {
                    if (g_padPressed[2] == 0) {
                        return 0;
                    }
                }
                D_801D444D = 1;
                return 2;
            }
            break;
        }
    }
}
