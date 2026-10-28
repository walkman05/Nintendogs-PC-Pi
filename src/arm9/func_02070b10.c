#include "ctx.h"

#ifdef NDOGS_PC /* NDOGS_PC: widened parameter 1: the DS callee uses the whole register */
void func_02070b10(u8 **p, u32 v)
{
#else
void func_02070b10(u8 **p, u8 v)
{
#endif
    if (*p != NULL) {
        (*p)[0x40] = v;
    }
}
