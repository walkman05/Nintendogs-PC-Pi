#include "ctx.h"

extern void func_02096264(void);
extern u32 data_02159038;

s32 func_02096224(void)
{
    s32 (*fp)(u32, u32, u32) = (s32 (*)(u32, u32, u32))func_02096264;
    return fp(data_02159038, 6, 0);
}
