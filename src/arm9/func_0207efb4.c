#include "ctx.h"

extern void func_0207ef9c(void);
extern s32 func_0207f004(u32 a, u32 b, u32 c, u32 d);

s32 func_0207efb4(u32 param_1)
{
    return ((s32 (*)(u32, u32, u32, u32))func_0207f004)(param_1, 0x027fffe8, (u32)func_0207ef9c, 1);
}
