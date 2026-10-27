#include "ctx.h"

extern s32 func_0200c498(u32 a, s16 b);

s32 func_02036484(u8 *p)
{
    u8 *q = p + 0x100;
    s16 x = *(s16 *)(q + 0x3e);
    return func_0200c498(11, (s16)(x - 13));
}
