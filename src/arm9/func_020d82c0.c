#include "ctx.h"

extern u32 func_020d055c(u32 a, u8 b);

#ifdef NDOGS_PC /* NDOGS_PC: widened parameter 1: the DS callee uses the whole register */
u32 func_020d82c0(u32 a, u32 b)
{
#else
u32 func_020d82c0(u32 a, u8 b)
{
#endif
    return func_020d055c(a, b + 1);
}
