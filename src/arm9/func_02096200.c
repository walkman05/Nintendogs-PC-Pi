#include "ctx.h"

extern u32 data_02159038;
s32 func_02096264(u32 a, u32 b, u32 c);

s32 func_02096200(u32 param_1)
{
    s32 (*fp)(u32, u32, u32) = func_02096264;
    return fp(data_02159038, 1, (u16)param_1);
}
