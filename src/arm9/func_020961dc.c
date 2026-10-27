#include "ctx.h"

extern u32 data_02159038;
extern void func_02096264(void);

s32 func_020961dc(u32 v)
{
    return ((s32 (*)(u32, u32, u32))func_02096264)(data_02159038, 2, (u16)v);
}
