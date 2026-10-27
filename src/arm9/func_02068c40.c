#include "ctx.h"

extern s32 func_02068c64(u8 *p);
extern u8 data_02150e3e[];

s32 func_02068c40(void)
{
    s32 (*fp)(u8 *) = func_02068c64;
    *(vu32 *)0x04000000 = *(vu32 *)0x04000000 & 0xbfffffff;
    return fp(data_02150e3e);
}
