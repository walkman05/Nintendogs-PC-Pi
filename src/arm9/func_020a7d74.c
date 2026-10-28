#include "ctx.h"

extern u8 *data_0215942c;

void func_020a85c4(u8 *o, u32 v);

#ifdef NDOGS_PC /* NDOGS_PC: widened parameter 0: the DS callee uses the whole register */
#ifdef NDOGS_PC /* NDOGS_PC: widened parameter 1: the DS callee uses the whole register */
void func_020a7d74(u32 param_1, u32 param_2)
{
#else
void func_020a7d74(u32 param_1, u16 param_2)
{
#endif
#else
void func_020a7d74(u16 param_1, u16 param_2)
{
#endif
    u8 *o = data_0215942c;
    s16 s;
    s32 k;
    u8 *b;

    func_020a85c4(o, 17);
    *(u16 *)(o + 4) = 0;
    *(u16 *)(o + 26) = param_1;
    *(u16 *)(o + 14) = param_2;
    s = *(s16 *)(o + 14);
    b = o + 32;
    k = s * 20;
    *(u16 *)(b + k) = *(u16 *)(b + k) | 1;
    *(u16 *)(b + k + 2) = (u16)s;
}
