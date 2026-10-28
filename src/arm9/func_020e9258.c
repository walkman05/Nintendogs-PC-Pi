#pragma avoid_byte strb
#include "ctx.h"

extern vu8 data_027c0bb4;

static inline u8 SwapByte(u32 v, vu8 *p)
{
    u8 old;
    *(u8 *)(p) = (u8)(v);
    return old;
}

#ifdef NDOGS_PC /* NDOGS_PC: widened parameter 0: the DS callee uses the whole register */
void func_020e9258(u32 param_1)
{
#else
void func_020e9258(u8 param_1)
{
#endif
    SwapByte(param_1, &data_027c0bb4); return;
}
