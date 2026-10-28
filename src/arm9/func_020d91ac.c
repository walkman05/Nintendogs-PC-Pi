#include "ctx.h"

extern u32 func_020d055c(u8 *a, u8 b);

#ifdef NDOGS_PC /* NDOGS_PC: widened parameter 1: the DS callee uses the whole register */
u32 func_020d91ac(u8 *p, u32 b)
{
#else
u32 func_020d91ac(u8 *p, u8 b)
{
#endif
    return func_020d055c(p + 4, (u8)(b + 3));
}
