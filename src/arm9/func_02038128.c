#include "ctx.h"

#ifdef NDOGS_PC /* NDOGS_PC: widened parameter 1: the DS callee uses the whole register */
void func_02038128(void *p, u32 v)
{
#else
void func_02038128(void *p, u16 v)
{
#endif
    u8 *q = (u8 *)p + 0x100;
    *(u16 *)(q + 0x48) = v + 0x11;
}
