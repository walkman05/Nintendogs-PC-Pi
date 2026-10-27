#include "ctx.h"

extern s32 func_0207eff4(void);
extern void func_0207ef18(void);

s32 func_0207ef30(u32 a)
{
    s32 (*fp)(u32, u32, u32) = (s32 (*)(u32, u32, u32))func_0207eff4;
    return fp(a, 0x027fffe0, (u32)func_0207ef18);
}
