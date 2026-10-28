#include "ctx.h"

extern u16 data_02150e30;

extern void func_020699dc(u16 v);

#ifdef NDOGS_PC /* NDOGS_PC: widened parameter 0: the DS callee uses the whole register */
void func_02068edc(u32 v)
{
#else
void func_02068edc(u16 v)
{
#endif
    void (*fp)(u16);
    data_02150e30 |= v;
    fp = (void (*)(u16))func_020699dc;
    fp(v);
}
