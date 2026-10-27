#include "ctx.h"

extern u32 data_02159038;
extern void func_02096264(u32 a, u32 b, u32 c);

s32 func_02096244(void)
{
    return ((s32 (*)(u32, u32, u32))func_02096264)(data_02159038, 0, 0);
}
