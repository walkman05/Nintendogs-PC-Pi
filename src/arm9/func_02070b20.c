#include "ctx.h"

#ifdef NDOGS_PC /* NDOGS_PC: widened parameter 1: the DS callee uses the whole register */
void func_02070b20(u32 *p, u32 v)
{
#else
void func_02070b20(u32 *p, u8 v)
{
#endif
    if (*p != 0) {
        *(u8 *)(*p + 0x41) = v;
    }
}
