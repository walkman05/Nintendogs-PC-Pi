#pragma avoid_byte strb
#include "ctx.h"

extern u8 *data_02159028;

#ifdef NDOGS_PC /* NDOGS_PC: widened parameter 1: the DS callee uses the whole register */
#ifdef NDOGS_PC /* NDOGS_PC: widened parameter 2: the DS callee uses the whole register */
void func_0208e2b4(u32 param_1, u32 param_2, u32 param_3)
{
#else
void func_0208e2b4(u32 param_1, u32 param_2, u8 param_3)
{
#endif
#else
void func_0208e2b4(u32 param_1, u8 param_2, u8 param_3)
{
#endif
    u8 **pg;
    u8 *G;
    u8 *q;
    pg = &data_02159028;
    G = *pg;
    q = *(u8 **)(G + 8) + param_1 * 4;
    q[0] = param_2;
    q[1] = 0;
    q[2] = param_3;
}
