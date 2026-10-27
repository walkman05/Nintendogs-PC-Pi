#pragma avoid_byte strb
#include "ctx.h"

#ifdef NDOGS_PC
u32 func_02025974(u8 *param_1)
#else
void func_02025974(u8 *param_1)
#endif
{
    s32 a = *(s32 *)(*(u8 **)(param_1 + 0x9e8) + 8);
    *(s32 *)(param_1 + 0x920) = (s32)(((s64)a * 0x2008 + 0x800) >> 12);
#ifdef NDOGS_PC
    return HostSwpb((vu8 *)(param_1 + 175), 1);
#else
    *(u8 *)(param_1 + 175) = 1;
#endif
}
