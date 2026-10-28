#pragma avoid_byte strb
#include "ctx.h"
extern u8 *data_0214f464;
#ifdef NDOGS_PC /* NDOGS_PC: widened parameter 0: the DS callee uses the whole register */
void func_02044c24(u32 v)
{
#else
void func_02044c24(u8 v)
{
#endif
    u8 *g = data_0214f464;
    if (*(s8 *)(g + 7) == -1)
        g[8] = v;
}
