#pragma avoid_byte strb
#include "ctx.h"

#define FX_Mul(a, b) ((s32)(((s64)(a) * (b) + 0x800) >> 12))

#ifdef NDOGS_PC
u32 func_02025490(u8 *param_1)
#else
void func_02025490(u8 *param_1)
#endif
{
    s32 v = *(s32 *)(*(u8 **)(param_1 + 0x9e8) + 8);
    *(s32 *)(param_1 + 0x920) = FX_Mul(v, 0x1004);
#ifdef NDOGS_PC
    return HostSwpb((vu8 *)(param_1 + 175), 3);
#else
    param_1[175] = 3;
#endif
}
