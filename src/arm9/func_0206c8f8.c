#include "ctx.h"

void func_0206c8f8(u32 *param_1, u32 *param_2)
{
#ifdef NDOGS_PC
    {
        int i;
        if ((u32)param_2 >> 24 == 0x04) { /* to a register (matrix FIFO): no trap per word */
            for (i = 0; i < 16; i++)
                HostIoWrite32((u32)(param_2 + i), param_1[i]);
        } else if ((u32)param_1 >> 24 == 0x04) { /* from a register (matrix results) */
            for (i = 0; i < 16; i++)
                param_2[i] = HostIoRead32((u32)(param_1 + i));
        } else {
            for (i = 0; i < 16; i++)
                param_2[i] = param_1[i];
        }
    }
#else
    asm {
        ldmia r0!, {r2, r3, r12}
        stmia r1!, {r2, r3, r12}
        ldmia r0!, {r2, r3, r12}
        stmia r1!, {r2, r3, r12}
        ldmia r0!, {r2, r3, r12}
        stmia r1!, {r2, r3, r12}
        ldmia r0!, {r2, r3, r12}
        stmia r1!, {r2, r3, r12}
        ldmia r0, {r0, r2, r3, r12}
        stmia r1!, {r0, r2, r3, r12}
    }
#endif
}
