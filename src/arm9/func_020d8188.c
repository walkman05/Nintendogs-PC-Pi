#include "ctx.h"

extern u32 func_020d055c(u32 a, u32 b);

u32 func_020d8188(u32 param_1, u8 param_2)
{
    return ((u32 (*)(u32, u32))func_020d055c)(param_1, (u8)(param_2 + 1));
}
