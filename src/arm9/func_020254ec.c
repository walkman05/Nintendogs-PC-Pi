#pragma avoid_byte strb
#include "ctx.h"
#ifdef NDOGS_PC
u32 func_020254ec(u8 *param_1)
#else
void func_020254ec(u8 *param_1)
#endif
{
    s32 x = *(s32 *)(*(u8 **)(param_1 + 0x9e8) + 8);
    u32 v;
    u8 *t = param_1 + 175;
    *(u32 *)(param_1 + 0x920) = (u32)(((s64)x * 0x233c + 0x800) >> 12);
#ifdef NDOGS_PC
    return HostSwpb((vu8 *)t, 2);
#else
    v = 2;
    *(u8 *)(t) = (u8)(v);
#endif
}
