#pragma avoid_byte strb
#include "ctx.h"

static inline u32 SwapByteInPlace(u32 v, vu8 *p)
{
    *(u8 *)(p) = (u8)(v);
    return v;
}

#ifdef NDOGS_PC
u32 func_020bd868(u8 *param_1)
{
    u8 old;
    *(u16 *)(param_1 + 0x20) = *(u16 *)(param_1 + 0x20) & ~1;
    old = *(u8 *)(param_1 + 1);
    *(u8 *)(param_1 + 1) = (u8)(old & ~1);
    return old; /* DS: swpb leaves the old byte in r0 */
}
#else
void func_020bd868(u8 *param_1)
{
    *(u16 *)(param_1 + 0x20) = *(u16 *)(param_1 + 0x20) & ~1;
    SwapByteInPlace(*(u8 *)(param_1 + 1) & ~1, (vu8 *)(param_1 + 1)); return;
}
#endif
