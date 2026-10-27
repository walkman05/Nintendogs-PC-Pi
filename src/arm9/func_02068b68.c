#include "ctx.h"

extern u32 func_02068c64(void);
extern u8 data_02150e48;

u32 func_02068b68(void)
{
    u32 *reg = (u32 *)0x04001000;
    u32 (*fp)(void *) = (u32 (*)(void *))func_02068c64;
    *reg = *reg & 0x7fffffff;
    return fp(&data_02150e48);
}
