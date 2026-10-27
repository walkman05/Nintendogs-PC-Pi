#include "ctx.h"

#ifdef NDOGS_PC
u64 func_0206c7bc(u16 param_1, u8 *param_2, s32 param_3)
#else
void func_0206c7bc(u16 param_1, u8 *param_2, s32 param_3)
#endif
{
    s32 i;

    i = 0;
    for (;;) {
        if (i >= param_3) {
            break;
        }
        *(u16 *)(param_2 + i) = param_1;
        i = i + 2;
    }
#ifdef NDOGS_PC
    /* r0 still holds param_1, r1 param_2 (a caller reads r1). */
    return ((u64)(u32)param_2 << 32) | param_1;
#endif
}
