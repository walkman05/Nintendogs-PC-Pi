#include "ctx.h"

u32 func_020d055c(void *p, u8 v);

#ifdef NDOGS_PC /* NDOGS_PC: widened parameter 1: the DS callee uses the whole register */
u32 func_020d35d0(s32 *param_1, u32 param_2)
{
#else
u32 func_020d35d0(s32 *param_1, u8 param_2)
{
#endif
    u8 v = param_2 + 2;
    if (*param_1 == 2) {
        v = v + 1;
    }
    return func_020d055c(param_1 + 2, v);
}
