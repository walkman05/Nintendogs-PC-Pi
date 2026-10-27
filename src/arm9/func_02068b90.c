#include "ctx.h"

extern u8 data_02150e46[];
extern u32 func_02068c64(void *p);

u32 func_02068b90(void)
{
    u32 (*fp)(void *) = func_02068c64;
    u32 *io = (u32 *)0x04001000;
    *io = *io & 0xbfffffff;
    return fp(data_02150e46);
}
