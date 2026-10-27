#include "ctx.h"

void func_0206be70(u32 *src, u32 *dst)
{
#ifdef NDOGS_PC
    {
        /* dst is not advanced: each row overwrites dst[0..2], the last one (src[9..11]) remains */
        int i;
        for (i = 0; i < 4; i++) {
            dst[0] = src[i * 3 + 0];
            dst[1] = src[i * 3 + 1];
            dst[2] = src[i * 3 + 2];
        }
    }
#else
    asm {
        ldmia r0!, {r2, r3, r12}
        stmia r1, {r2, r3, r12}
        ldmia r0!, {r2, r3, r12}
        stmia r1, {r2, r3, r12}
        ldmia r0!, {r2, r3, r12}
        stmia r1, {r2, r3, r12}
        ldmia r0!, {r2, r3, r12}
        stmia r1, {r2, r3, r12}
    }
#endif
}
