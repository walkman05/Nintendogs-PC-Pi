#include "ctx.h"

extern u8 data_02150e3a[];
extern s32 func_02068c64(void *p);

s32 func_02068bf4(void)
{
    s32 (*fp)(void *) = func_02068c64;
    return fp(data_02150e3a);
}
