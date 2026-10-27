#include "ctx.h"

extern u32 func_020d055c(u8 *a, u8 b);

u32 func_020d91ac(u8 *p, u8 b)
{
    return func_020d055c(p + 4, (u8)(b + 3));
}
