#include "ctx.h"

extern u8 data_02150e40;
extern s32 func_02068c64(u8 *p);

s32 func_02068c1c(void)
{
    u32 *r = (u32 *)0x04000000;
    *r = *r & 0x7fffffff;
    return func_02068c64(&data_02150e40);
}
