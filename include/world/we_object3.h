#ifndef WORLD_WE_OBJECT3_H
#define WORLD_WE_OBJECT3_H

#include "common.h"
#include "psxsdk/libgpu.h"
#include "world.h"

typedef struct {
    s32 val;    /* +0x00 */
    s16 hval;   /* +0x04 */
    s16 pad;    /* +0x06 */
} FeaEntry40C0; /* size 0x08 */

typedef struct {
    u16 x;
    u16 y;
} ImageCoord;

typedef struct {
    /* 0x0000 */ u32 id;                /**< TIM magic */
    /* 0x0004 */ u32 flags;             /**< TIM flags */
    /* 0x0008 */ u32 size1;             /**< First image block size */
    /* 0x000C */ RECT rect1;            /**< First image rect (unused here) */
    /* 0x0014 */ u32 image1[0x800];     /**< First image pixel data (256*16 16bpp) */
    /* 0x2014 */ u32 size2;             /**< Second image block size */
    /* 0x2018 */ RECT rect2;            /**< Second image rect (unused here) */
    /* 0x2020 */ u32 image2[1];         /**< Second image pixel data */
} PackedTIMPair;

extern FeaEntry40C0  D_800D24A8[12];
extern WorldObject   D_800D3320[16];
extern WorldObject  *D_800D3318;
extern WorldObject  *D_800D34E0;
extern WorldObject  *D_800D34E4;
extern u32           D_800D2284;
extern u32           D_800D34A0[16];
extern u32           D_800D34F0;
extern WorldObject   D_800D33E0[16];
extern WorldObject   D_800C9EF0[16];
extern WorldObject  *D_800CA030;
extern POLY_F4       D_800D3300;
extern ImageCoord    D_800C5388[];
extern ImageCoord    D_800C5378[];
extern RECT          D_800D32F0;

extern s32 func_800A629C(WorldObject *target);

#endif /* WORLD_WE_OBJECT3_H */
