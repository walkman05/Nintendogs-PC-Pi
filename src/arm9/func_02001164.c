#include "ctx.h"

extern u8 data_0214e684[];

#ifdef NDOGS_PC /* NDOGS_PC: widened parameter 0: the DS callee uses the whole register */
void func_02001164(u32 mask)
{
#else
void func_02001164(u16 mask)
{
#endif
    do {
    } while ((*(vu16 *)(data_0214e684 + 0x36) & mask) != 0);
}
