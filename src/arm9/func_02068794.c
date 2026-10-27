#include "ctx.h"

extern u8 data_02150e30;
extern u32 func_02068824(u32 a);

u32 func_02068794(void)
{
    u32 (*fp)(u32) = func_02068824;
    return fp(*(u16 *)((u8 *)&data_02150e30 + 10));
}
