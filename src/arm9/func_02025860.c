#pragma avoid_byte strb
#include "ctx.h"

#ifdef NDOGS_PC
u32 func_02025860(u8 *param_1)
#else
void func_02025860(u8 *param_1)
#endif
{
    s32 x = *(s32 *)(*(u8 **)(param_1 + 0x9e8) + 8);
    vu8 *q;
    u32 v;
    *(s32 *)(param_1 + 0x920) = (s32)(u32)((((s64)4100 * (s64)x) + 0x800) >> 12);
    q = (__typeof__(q))((u32)(param_1) + (175));
#ifdef NDOGS_PC
    return HostSwpb((vu8 *)(param_1 + 175), 2);
#else
    v = (__typeof__(v))(2);
    *(u8 *)(q) = (u8)(v);
    return;
#endif
}
