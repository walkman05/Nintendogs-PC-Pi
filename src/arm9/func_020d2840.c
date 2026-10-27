#include "ctx.h"

extern u32 func_020d055c(u32 a, u32 b);

u32 func_020d2840(u32 a, u8 b)
{
    return func_020d055c(a, (u8)(b + 2));
}
