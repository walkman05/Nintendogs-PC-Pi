#pragma avoid_byte strb
#include "ctx.h"
#define FX_Mul(a, b) ((s32)(((s64)(a) * (b) + 0x800) >> 12))

#ifdef NDOGS_PC
u32 func_02025758(u8 *o)
#else
void func_02025758(u8 *o)
#endif
{
    s32 v = *(s32 *)(*(u8 **)(o + 0x9e8) + 8);
    *(s32 *)(o + 0x920) = FX_Mul(v, 0xe6a);
#ifdef NDOGS_PC
    return HostSwpb((vu8 *)(o + 175), 3);
#else
    *(o + 175) = 3;
#endif
}
