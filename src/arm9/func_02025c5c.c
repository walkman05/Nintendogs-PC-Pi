#pragma avoid_byte strb
#include "ctx.h"
#ifdef NDOGS_PC
u32 func_02025c5c(u8 *param_1)
#else
void func_02025c5c(u8 *param_1)
#endif
{
    s32 x = *(s32 *)(*(u8 **)(param_1 + 0x9e8) + 8);
    u32 v;
    u8 *t = param_1 + 175;
    *(u32 *)(param_1 + 0x920) = (u32)(((s64)x * 0x1cd4 + 0x800) >> 12);
#ifdef NDOGS_PC
    return HostSwpb((vu8 *)(param_1 + 175), 1);
#else
    v = 1;
    *(u8 *)(t) = (u8)(v);
#endif
}
