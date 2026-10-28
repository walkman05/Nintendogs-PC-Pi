#include "ctx.h"

#ifdef NDOGS_PC /* NDOGS_PC: widened parameter 1: the DS callee uses the whole register */
void func_ov019_0215cdbc(u8 *p, u32 v)
{
#else
void func_ov019_0215cdbc(u8 *p, u16 v)
{
#endif
    if (p != NULL) {
        *(u16 *)(p + 0x2c) = v;
    }
}
