#ifndef THREAD_H
#define THREAD_H

#include "common.h"

/* Raw controller-input helpers (thread.c). */

/* Public prototypes */
extern void func_800275D4(void);                /**< Refresh the raw controller buffers. */
extern s32  func_80027DB4(s32 a, s32 b, s32 c); /**< Read an analog axis (b: 2 = X, 3 = Y). */
extern s32  func_80027CF8(s32 a, s32 b, s32 c); /**< Fold a recentred analog stick into d-pad bits. */

/* getAnimFrameParam (thread.c) returns u16, but consumers like be_object4.c's readPads use the
   result as s32 with no widening mask — an inconsistent caller view that can't share a decl here,
   so those callers keep their own `extern s32 getAnimFrameParam(...)`. */

#endif /* THREAD_H */
