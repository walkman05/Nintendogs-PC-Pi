#include "ctx.h"

s32 func_0208817c(u32 a, u32 b, u32 c, u32 d);

s32 func_02087e90(u32 a, u32 b, u32 c)
{
    s32 (*fp)(u32, u32, u32, u32) = func_0208817c;
    return fp(a, b, c, 0);
}
