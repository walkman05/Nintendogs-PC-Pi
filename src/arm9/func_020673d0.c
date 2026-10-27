#include "ctx.h"

void func_020673d0(u32 *param_1, u32 *param_2)
{
#ifdef NDOGS_PC
    {
        int i;
        for (i = 0; i < 4; i++) {
            param_2[i * 3 + 0] = param_1[i * 4 + 0];
            param_2[i * 3 + 1] = param_1[i * 4 + 1];
            param_2[i * 3 + 2] = param_1[i * 4 + 2];
        }
    }
#else
    asm {
        ldmia r0!, {r2, r3, r12}
        add r0, r0, #4
        stmia r1!, {r2, r3, r12}
        ldmia r0!, {r2, r3, r12}
        add r0, r0, #4
        stmia r1!, {r2, r3, r12}
        ldmia r0!, {r2, r3, r12}
        add r0, r0, #4
        stmia r1!, {r2, r3, r12}
        ldmia r0!, {r2, r3, r12}
        add r0, r0, #4
        stmia r1!, {r2, r3, r12}
    }
#endif
}
