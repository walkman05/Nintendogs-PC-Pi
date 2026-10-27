#include "ctx.h"

void func_02003be4(void);
#ifdef NDOGS_PC
u32 func_020a1a40(s32 v);
#else
void func_020a1a40(s32 v);
#endif
void func_02003c10(s32 v);

#ifdef NDOGS_PC
u32 func_020ad6fc(u8 *param_1, u16 param_2)
#else
void func_020ad6fc(u8 *param_1, u16 param_2)
#endif
{
    *(u16 *)(param_1 + 44) = param_2;
    if (param_2 & 1) {
        func_02003be4();
        if ((((s32)(*(vu16 *)0x04000304 & 0x8000)) >> 15) == 1) {
#ifdef NDOGS_PC
            return func_020a1a40(40);
#else
            func_020a1a40(40);
#endif
        } else {
#ifdef NDOGS_PC
            return func_020a1a40(68);
#else
            func_020a1a40(68);
#endif
        }
#ifndef NDOGS_PC
        return;
#endif
    }
    if (param_2 & 2) {
        func_02003c10(20);
    }
#ifdef NDOGS_PC
    return 0;
#endif
}
