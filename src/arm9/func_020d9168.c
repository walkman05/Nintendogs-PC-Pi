#include "ctx.h"

u32 func_020d055c(u32 a, u32 b);

u32 func_020d9168(u32 param_1, u8 param_2)
{
    u32 (*fp)(u32, u32) = func_020d055c;
    return fp(param_1, (u8)(param_2 + 1));
}
