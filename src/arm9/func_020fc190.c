#include "ctx.h"

#ifdef NDOGS_PC /* NDOGS_PC: widened parameter 1: the DS callee uses the whole register */
void func_020fc190(u16 *p, u32 n, u32 x)
{
#else
void func_020fc190(u16 *p, u16 n, u32 x)
{
#endif
    u32 inc = (x << 28) >> 16;
    if (n == 0) {
        goto done;
    }
    do {
        *p = (u16)(*p + inc);
        p = p + 1;
        n--;
    } while (n != 0);
done:
    return;
}
