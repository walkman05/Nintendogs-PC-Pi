#pragma avoid_byte strb
#include "ctx.h"

extern u8 *data_0214f464;

#ifdef NDOGS_PC /* NDOGS_PC: widened parameter 0: the DS callee uses the whole register */
void func_0204494c(u32 v)
{
#else
void func_0204494c(u8 v)
{
#endif
    if (*(s8 *)(data_0214f464 + 19) == 3) {
        *(u8 *)(data_0214f464 + 19) = v;
    }
}
