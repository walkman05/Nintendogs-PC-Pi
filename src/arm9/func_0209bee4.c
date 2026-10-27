#include "ctx.h"
extern u32 func_0209bf30(u32 a, u32 b);
void func_0209bee4(u32 *param_1, u32 param_2, u32 param_3)
{
    u32 uVar1;
    u8 *q;
    u16 h;
    u16 one;
    uVar1 = func_0209bf30((u32)param_1, param_2);
    if (uVar1 != 0xffffffff) {
#ifdef NDOGS_PC
        q = (u8 *)param_1 + (uVar1 << 2);
#else
        asm { add q, param_1, uVar1, lsl #2 }
#endif
        *(u32 *)(q + 0x2c) = param_3;
        h = *(u16 *)((u8 *)param_1 + 0x2a);
        one = 1;
#ifdef NDOGS_PC
        one = (u16)(h | (one << uVar1));
#else
        asm { orr one, h, one, lsl uVar1 }
#endif
        *(u16 *)((u8 *)param_1 + 0x2a) = one;
    }
}
