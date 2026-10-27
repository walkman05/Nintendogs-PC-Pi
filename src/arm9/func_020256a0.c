#pragma avoid_byte strb
#include "ctx.h"

static inline u32 SwapV(u32 v, vu8 *p)
{
    *(u8 *)(p) = (u8)(v);
    return v;
}

#ifdef NDOGS_PC
u32 func_020256a0(u8 *p)
#else
void func_020256a0(u8 *p)
#endif
{
    s32 v = *(s32 *)(*(u8 **)(p + 0x9e8) + 8);
    *(u32 *)(p + 0x920) = (u32)(((s64)v * 0x1338 + 0x800) >> 12);
#ifdef NDOGS_PC
    return HostSwpb((vu8 *)(p + 175), 1);
#else
    SwapV(1, (vu8 *)(p + 0xaf));
#endif
}
