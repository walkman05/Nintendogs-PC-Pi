#pragma avoid_byte strb
#include "ctx.h"

static inline u8 SwapByte(u32 v, vu8 *p)
{
    u8 old;
    *(u8 *)(p) = (u8)(v);
    return old;
}

#ifdef NDOGS_PC
u32 func_02025bb0(u8 *param_1)
#else
void func_02025bb0(u8 *param_1)
#endif
{
    s32 x;
    u32 q;
    x = *(s32 *)(*(u8 **)(param_1 + 0x9e8) + 8);
    *(u32 *)(param_1 + 0x920) = (u32)(((s64)x * 9020 + 0x800) >> 12);
    q = (__typeof__(q))((u32)(param_1) + (175));
#ifdef NDOGS_PC
    return HostSwpb((vu8 *)(param_1 + 175), 1);
#else
    SwapByte(1, (vu8 *)q); return;
#endif
}
