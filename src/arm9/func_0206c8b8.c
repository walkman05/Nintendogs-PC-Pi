#include "ctx.h"

void func_0206c8b8(u32 *param_1, u32 *param_2)
{
#ifdef NDOGS_PC
    {
        int i;
        for (i = 0; i < 9; i++)
            param_2[i] = param_1[i];
    }
#else
    asm {
        ldmia r0!, {r2, r3, r12}
        stmia r1!, {r2, r3, r12}
        ldmia r0!, {r2, r3, r12}
        stmia r1!, {r2, r3, r12}
        ldmia r0!, {r2, r3, r12}
        stmia r1!, {r2, r3, r12}
    }
#endif
}
