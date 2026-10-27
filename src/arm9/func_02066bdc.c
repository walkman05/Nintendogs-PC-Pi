#include "ctx.h"

void func_02066bdc(u32 *param_1, u32 *param_2)
{
#ifdef NDOGS_PC
    {
        u32 s0 = param_1[0], s1 = param_1[1], s2 = param_1[2];
        u32 s3 = param_1[3], s4 = param_1[4], s5 = param_1[5];
        u32 s6 = param_1[6], s7 = param_1[7], s8 = param_1[8];
        param_2[0] = s0; param_2[1] = s3; param_2[2] = s6;
        param_2[3] = s1; param_2[4] = s4; param_2[5] = s7;
        param_2[6] = s2; param_2[7] = s5; param_2[8] = s8;
        param_2[9] = 0; param_2[10] = 0; param_2[11] = 0;
    }
#else
    asm {
        ldmia r0, {r2, r3, r4, r5, r6, r7, r8, r9, r12}
        stmia r1!, {r2, r5, r8}
        stmia r1!, {r3, r6, r9}
        stmia r1!, {r4, r7, r12}
        mov r0, #0
        str r0, [r1]
        str r0, [r1, #0x4]
        str r0, [r1, #0x8]
    }
#endif
}
