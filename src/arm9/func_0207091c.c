#include "ctx.h"

#ifdef NDOGS_PC /* NDOGS_PC: widened parameter 1: the DS callee uses the whole register */
void func_0207091c(u8 **pp, u32 v)
{
#else
void func_0207091c(u8 **pp, u16 v)
{
#endif
    if (*pp != NULL) {
        *(u16 *)(*pp + 0x34) = 1;
        *(u16 *)(*pp + 0x38) = v;
    }
}
