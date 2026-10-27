#pragma avoid_byte strb
#include "ctx.h"

#ifdef NDOGS_PC
u32 func_02025a84(u8 *param_1)
#else
void func_02025a84(u8 *param_1)
#endif
{
    s32 x = *(s32 *)(*(u8 **)(param_1 + 0x9e8) + 8);
    *(s32 *)(param_1 + 0x920) = (s32)((((s64)x * 6560) + 0x800) >> 12);
#ifdef NDOGS_PC
    return HostSwpb((vu8 *)(param_1 + 175), 2);
#else
    *(u8 *)(param_1 + 175) = 2;
#endif
}
