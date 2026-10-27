#include "ctx.h"

extern u32 func_0207e1ac(u32 a, u32 b);

u32 func_02077dc4(u32 a, u8 *p)
{
    u32 (*fp)(u32, u32) = func_0207e1ac;
    return fp(a, (u32)p[0x18] * 0x38);
}
