#include "ctx.h"

u32 func_0207ce4c(u32 a, u32 b);

u32 func_0208f698(int param_1)
{
    u32 (*fp)(u32, u32) = func_0207ce4c;
    return fp(param_1 + 0x24, 0);
}
