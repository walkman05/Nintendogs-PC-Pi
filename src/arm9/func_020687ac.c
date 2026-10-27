#include "ctx.h"

extern u8 data_02150e30;
extern u32 func_02068824(u32 a);

u32 func_020687ac(void)
{
    return func_02068824(*(u16 *)(&data_02150e30 + 8));
}
