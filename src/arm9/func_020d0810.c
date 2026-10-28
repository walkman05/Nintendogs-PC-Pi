#include "ctx.h"

#ifdef NDOGS_PC /* NDOGS_PC: widened parameter 1: the DS callee uses the whole register */
u8 func_020d0810(u32 a, u32 b)
{
#else
u8 func_020d0810(u32 a, u8 b)
{
#endif
    return (u8)(b + 1) + 3;
}
