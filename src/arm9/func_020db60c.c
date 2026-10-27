#include "ctx.h"

extern s32 func_020db14c(u32 a, u32 b, u8 *c);

s32 func_020db60c(u8 *p, u8 *q)
{
    return func_020db14c(*(u32 *)(p + 0x14), *(u32 *)(q + 0x14), q + 8);
}
