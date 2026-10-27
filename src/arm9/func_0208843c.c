#include "ctx.h"

extern u8 data_02156054;
extern u32 func_020873d8(u8 *p, u32 a, u32 b);

u32 func_0208843c(u32 a, u32 b)
{
    return func_020873d8(&data_02156054, a, b);
}
