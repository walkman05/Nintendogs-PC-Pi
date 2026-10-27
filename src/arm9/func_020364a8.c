#include "ctx.h"

extern s32 func_0200c498(u32 a, s32 b);

s32 func_020364a8(u8 *p)
{
    s32 (*fp)(u32, s32) = func_0200c498;
    return fp(9, (s16)(*(s16 *)(p + 0x13e) - 45));
}
