#include "ctx.h"

extern u32 func_020d055c(u32 a, u32 b);

#ifdef NDOGS_PC /* NDOGS_PC: widened parameter 1: the DS callee uses the whole register */
u32 func_020d8188(u32 param_1, u32 param_2)
{
#else
u32 func_020d8188(u32 param_1, u8 param_2)
{
#endif
    return ((u32 (*)(u32, u32))func_020d055c)(param_1, (u8)(param_2 + 1));
}
