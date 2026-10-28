#include "ctx.h"

#pragma opt_loop_invariants off

extern u32 data_0215be48;
#ifdef NDOGS_PC /* NDOGS_PC: return type of func_02085cf4 as defined (x86 extends by the caller's prototype, the DS by the callee) */
u32 func_02085cf4(u32 a, u32 b);
#else
s16 func_02085cf4(u32 a, u32 b);
#endif
u32 *func_02085ccc(u32 a, u32 b);

void func_021127b8(s32 param_1, s32 param_2, u32 param_3)
{
    s32 cnt;
    s32 bound;
    u32 iVar4;
    s32 iVar3;
#ifdef NDOGS_PC /* NDOGS_PC: widened: holds func_02085cf4's value unchanged, as the DS register does */
    u32 uVar1;
#else
    s16 uVar1;
#endif
    u32 *puVar2;

    iVar3 = ((u32)data_0215be48 + 0x1dec) + param_1 * 0x294;
    iVar4 = *(u32 *)(*(u32 *)(iVar3 + 8) + param_2 * 8 + 0xc);
    uVar1 = func_02085cf4(*(u32 *)(iVar3 + 0xc), iVar4);
    puVar2 = func_02085ccc(*(u32 *)(iVar3 + 0xc), iVar4);
    bound = uVar1;
    cnt = 0;
    if (bound > 0) {
        do {
            *puVar2 = (*puVar2 & 0xfffff3ff) | ((param_3 & 3) << 10);
            cnt = cnt + 1;
            puVar2 = puVar2 + 3;
        } while (cnt < bound);
    }
}
