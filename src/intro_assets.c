/**
 * @file intro_assets.c
 * @brief CD asset table for the intro overlay.
 *
 * Each entry is a @c (sector, size) pair describing one streamed asset on
 * the disc — bitmap, music, or text page. @c func_80098338 indexes this
 * table by stage number, while @c func_800985B4 reads entry 34 directly.
 *
 * In a CD-mastering pipeline this file would be regenerated from the disc
 * layout; for now the offsets are taken from the original game image.
 */
#include "intro_assets.h"

u32 g_introAssetTable[] = {
    /* idx  sector       size           description                         */
    0x000246D9, 0x0002E5F2,  /*  0: Disc 1 swap prompt                       */
    0x00024736, 0x0002CE46,  /*  1: Disc 2 swap prompt                       */
    0x00024790, 0x0002D772,  /*  2: Disc 3 swap prompt                       */
    0x000247EB, 0x0002C834,  /*  3: Disc 4 swap prompt                       */
    0x00024845, 0x0000F199,  /*  4: FF8 logo                                 */
    0x00024864, 0x0000FCBC,  /*  5: SeeD story text page                     */
    0x00024884, 0x0000F96C,  /*  6: SeeD story text page                     */
    0x000248A4, 0x00024404,  /*  7: SeeD story text page                     */
    0x000248ED, 0x00029BC7,  /*  8: SeeD story text page                     */
    0x00024941, 0x0000FC47,  /*  9: SeeD story text page                     */
    0x00024961, 0x00010186,  /* 10: SeeD story text page                     */
    0x00024982, 0x00023D6F,  /* 11: SeeD story text page                     */
    0x000249CA, 0x000221CF,  /* 12: SeeD story text page                     */
    0x00024A0F, 0x0000FCA1,  /* 13: SeeD story text page                     */
    0x00024A2F, 0x0000FF14,  /* 14: SeeD story text page                     */
    0x00024A4F, 0x00020597,  /* 15: SeeD story text page                     */
    0x00024A90, 0x00021378,  /* 16: SeeD story text page                     */
    0x00024AD3, 0x00010261,  /* 17: SeeD story text page                     */
    0x00024AF4, 0x0000FF48,  /* 18: SeeD story text page                     */
    0x00024B14, 0x0002144C,  /* 19: SeeD story text page                     */
    0x00024B57, 0x00026CAD,  /* 20: SeeD story text page                     */
    0x00024BA5, 0x00010236,  /* 21: SeeD story text page                     */
    0x00024BC6, 0x000107D9,  /* 22: SeeD story text page                     */
    0x00024BE7, 0x00029B94,  /* 23: SeeD story text page                     */
    0x00024C3B, 0x000279E1,  /* 24: SeeD story text page                     */
    0x00024C8B, 0x00010054,  /* 25: SeeD story text page                     */
    0x00024CAC, 0x0000FFB0,  /* 26: SeeD story text page                     */
    0x00024CCC, 0x0001FE02,  /* 27: SeeD story text page                     */
    0x00024D0C, 0x00026F37,  /* 28: SeeD story text page                     */
    0x00024D5A, 0x0001007E,  /* 29: SeeD story text page                     */
    0x00024D7B, 0x0000FBD7,  /* 30: SeeD story text page                     */
    0x00024D9B, 0x00026F1F,  /* 31: SeeD story text page                     */
    0x00024DE9, 0x0002D5DA,  /* 32: "The End"                                */
    0x00024E44, 0x00015485,  /* 33: Final fade                               */
    0x00024E6F, 0x0000FF09,  /* 34: Background music (read by func_800985B4) */
    0x00024E8F, 0x0000F131,  /* 35: Squaresoft logo                          */
};
