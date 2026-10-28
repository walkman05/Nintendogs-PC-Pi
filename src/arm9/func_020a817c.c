#include "ctx.h"

extern u8 *data_0215942c;

void func_020a85c4(u16 *p, s32 v);
void func_020a8328(u8 *p);

#ifdef NDOGS_PC /* NDOGS_PC: widened parameter 1: the DS callee uses the whole register */
#ifdef NDOGS_PC /* NDOGS_PC: widened parameter 2: the DS callee uses the whole register */
void func_020a817c(u8 *param_1, s32 param_2, s32 param_3)
{
#else
void func_020a817c(u8 *param_1, s32 param_2, s16 param_3)
{
#endif
#else
void func_020a817c(u8 *param_1, s16 param_2, s16 param_3)
{
#endif
    u16 *puVar3;
    u8 *base;
    s16 sVar1;
    s32 off;

    puVar3 = (u16 *)data_0215942c;
    func_020a85c4(puVar3, 3);
    puVar3[2] = 2;
    puVar3[3] = 1;
    puVar3[4] = 0;
    puVar3[5] = param_3;
    puVar3[7] = param_2;
    sVar1 = puVar3[7];
    base = (u8 *)puVar3 + 32;
    off = sVar1 * 20;
    *(u16 *)(base + off) = *(u16 *)(base + off) | 1;
    *(u16 *)(base + off + 2) = sVar1;
    if (param_1 != NULL) {
        func_020a8328(param_1);
    }
}
