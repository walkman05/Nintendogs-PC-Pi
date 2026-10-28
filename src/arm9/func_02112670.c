#include "ctx.h"
#pragma opt_loop_invariants off

extern s32 data_0215be48;
#ifdef NDOGS_PC /* NDOGS_PC: return type of func_02085cf4 as defined (x86 extends by the caller's prototype, the DS by the callee) */
extern u32 func_02085cf4(s32 a, s32 b);
#else
extern u16 func_02085cf4(s32 a, s32 b);
#endif
extern u8 *func_02085ccc(s32 a, s32 b);

void func_02112670(s32 param_1, u16 *param_2, u32 param_3)
{
    u32 lim = 0xffff;
    u16 *walk;
#ifdef NDOGS_PC /* NDOGS_PC: widened: holds func_02085cf4's value unchanged, as the DS register does */
    u32 uVar3;
#else
    u16 uVar3;
#endif
    s32 iVar5;
    s32 base;
    u32 sh;
    s32 *gp;
    s32 cnt;
    u8 *iVar4;

    walk = param_2;
    if (*walk != lim) {
        base = param_1 * 660;
        sh = (param_3 & 0xf) << 12;
        gp = &data_0215be48;
#ifdef NDOGS_PC
#else
        asm { mov r6, #0 }
#endif
        do {
            iVar4 = (u8 *)(*gp + 0x1dec + base);
            {
                u16 wv = *walk;
                iVar5 = *(s32 *)(*(u8 **)(iVar4 + 8) + (u32)wv * 8 + 0xc);
            }
            uVar3 = func_02085cf4(*(s32 *)(iVar4 + 0xc), iVar5);
            iVar4 = func_02085ccc(*(s32 *)((u8 *)(*gp + base) + 0x1df8), iVar5);
#ifdef NDOGS_PC
            cnt = 0;
#else
            asm { mov cnt, r6 }
#endif
            if ((s32)uVar3 > 0) {
                do {
                    cnt = cnt + 1;
                    *(u32 *)(iVar4 + 4) = (*(u32 *)(iVar4 + 4) & 0xffff0fff) | sh;
                    iVar4 = iVar4 + 12;
                } while (cnt < (s32)(u32)uVar3);
            }
            walk = walk + 1;
        } while (*walk != lim);
    }
}
