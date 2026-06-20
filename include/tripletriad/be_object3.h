#ifndef TRIPLETRIAD_BE_OBJECT3_H
#define TRIPLETRIAD_BE_OBJECT3_H

#include "common.h"
#include "tripletriad.h"

/* Declarations exported by be_object3.c.
   Populated as functions/data are decompiled — move the file-scope externs,
   typedefs, and prototypes out of be_object3.c into here as you go. */

/**
 * @brief State-machine driver node (state byte at 0x0C).
 *
 * Used by the claim handlers func_800A0B24 / func_800A0F0C / func_800A1080 /
 * func_800A1260 / func_800A1374 (in be_object3.c) and spawned by the card-claim
 * controller func_800A15C8 (in be_object3b.c).
 */
typedef struct {
    /* 0x00 */ u8 pad00[0x0C];
    /* 0x0C */ u8 state;
    /* 0x0D */ u8 field0D;
    /* 0x0E */ u8 field0E;   /**< Side; matched against an object's 0x0A field XOR 1. */
    /* 0x0F */ u8 index;     /**< Per-tick iteration index. */
    /* 0x10 */ u8 pad10[0x12];
    /* 0x22 */ u8 field22;   /**< Pre-set "already finished" flag: func_8009E904 returns 2 immediately when this is 1. */
} ScriptStateNode;

/** @brief One 0x20-byte entry of the D_801D4308 active-object array. */
typedef struct {
    /* 0x00 */ u8  cardId;   /**< Index into g_tripleTriadCardStats. */
    /* 0x01 */ u8  pad01[3];
    /* 0x04 */ s32 flags;    /**< Bit 0 set = active/pending. */
    /* 0x08 */ u8  field8;
    /* 0x09 */ u8  field9;
    /* 0x0A */ u8  fieldA;   /**< Owning side. */
    /* 0x0B */ u8  pad0B[5];
    /* 0x10 */ s16 rotX;     /**< Rotation vector X (passed to func_80041274). */
    /* 0x12 */ s16 rotY;     /**< Rotation vector Y (card-flip angle). */
    /* 0x14 */ s16 rotZ;     /**< Rotation vector Z. */
    /* 0x16 */ u8  pad16[2];
    /* 0x18 */ s16 baseX;    /**< Board position fed to the GTE translation. */
    /* 0x1A */ s16 baseY;
    /* 0x1C */ s16 baseZ;
    /* 0x1E */ s16 sort;     /**< OT sort-offset (added to g_otBase base). */
} ActiveObj; /* 0x20 */

extern ActiveObj D_801D4308[]; /**< Active card-display object array. */

/* Public prototypes */
extern void updateFadeEffects(void);

#endif /* TRIPLETRIAD_BE_OBJECT3_H */
