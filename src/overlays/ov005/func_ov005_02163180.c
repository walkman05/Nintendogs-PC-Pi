#include "ctx.h"
typedef volatile s32 vs32;

extern u32 data_ov005_02167f00;
extern s32 data_ov005_02167c48;

#define FX_Mul(a, b) ((s32)(((s64)(a) * (b) + 0x800) >> 12))

s32 func_01ffdb08(s32 a);
void func_ov005_02163d4c(s32 a, s32 b, s32 *c);
void func_0202756c(s32 a, s32 *b);
s32 func_02028130(s32 a);
void func_01ff832c(s32 a, s32 *b, s32 *c, s32 *d);
void func_02027618(s32 a, s32 *b);
void func_02081aec(u8 *p, s32 *a, s32 *b);
#ifdef NDOGS_PC /* NDOGS_PC: return type of func_02028118 as defined (x86 extends by the caller's prototype, the DS by the callee) */
u32 func_02028118(s32 a);
#else
u16 func_02028118(s32 a);
#endif

void func_ov005_02163180(u8 *param_1)
{
    s32 st[24];
    s32 iVar4;
    s32 iVar6;
    s32 uVar5;
#ifdef NDOGS_PC /* NDOGS_PC: widened: holds func_02028118's value unchanged, as the DS register does */
    u32 uVar3;
#else
    u16 uVar3;
#endif

    iVar4 = func_01ffdb08(*(s16 *)(*(u32 *)&data_ov005_02167f00));
    func_ov005_02163d4c(iVar4, 6, st);
    func_0202756c(iVar4, &st[9]);
    st[21] = st[15];
    st[22] = st[16];
    st[23] = st[17];
    uVar5 = func_02028130(iVar4);
    func_01ff832c(FX_Mul(uVar5, 2048), &st[21], st, &st[3]);
    func_02027618(iVar4, &st[6]);
    uVar5 = func_02028130(iVar4);
    st[5] += FX_Mul(uVar5, 2048);
    uVar5 = func_02028130(iVar4);
    st[8] += FX_Mul(uVar5, 2048);
    func_02081aec(param_1, &st[3], &st[6]);
    iVar6 = func_02028130(iVar4);
    uVar3 = func_02028118(iVar4);
    iVar4 = *(vs32 *)&(&data_ov005_02167c48)[uVar3];
    *(s32 *)(param_1 + 0x44) = FX_Mul(iVar4, iVar6);
    *(s32 *)(param_1 + 0x54) = *(s16 *)(*(u32 *)&data_ov005_02167f00 + 8);
}
