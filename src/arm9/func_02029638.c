#include "ctx.h"

#ifdef NDOGS_PC /* NDOGS_PC: widened parameter 1: the DS callee uses the whole register */
u32 func_02029638(u8 *p, s32 v)
{
#else
u32 func_02029638(u8 *p, s16 v)
{
#endif
    *(u16 *)(p + 0xea) = v + 0x110;
    return 1;
}
