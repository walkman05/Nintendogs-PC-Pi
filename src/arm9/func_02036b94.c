#include "ctx.h"

extern void func_02009bd0(void);

s32 func_02036b94(void *p, u32 a)
{
    return ((s32 (*)(u32, u32))func_02009bd0)(a, (u32)((u8 *)p + 0x2e8));
}
