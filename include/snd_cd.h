#ifndef SND_CD_H
#define SND_CD_H

#include "common.h"

// Public prototypes

/** @brief LZSS-decompress @p src into @p dest (owned by snd_cd.c). */
s32 func_80039444(u8 *src, u8 *dest);

#endif /* SND_CD_H */
