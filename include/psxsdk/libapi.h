#ifndef LIBAPI_H
#define LIBAPI_H

#include "common.h"

/* --- libapi function declarations (signatures match PsyQ 4.6 LIBAPI.H) --- */

s32 OpenEvent(u32 desc, s32 spec, s32 mode, s32 (*func)());
s32 EnableEvent(s32 event);
s32 CloseEvent(s32 event);
s32 DisableEvent(s32 event);
void SystemError(u8 type, s32 errorCode);

extern void SetMem(u8 a);
#endif /* LIBAPI_H */
