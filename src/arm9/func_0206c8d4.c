#include "ctx.h"

u64 func_0206c8d4(u32 *src, u32 *dst)
{
#ifdef NDOGS_PC
    {
        /* returns r0:r1 as left by the asm: r0 = src + 12 (callers use it as u32 *), r1 = dst + 12 */
        int i;
        for (i = 0; i < 12; i++)
            dst[i] = src[i];
        return ((u64)(u32)(dst + 12) << 32) | (u32)(src + 12);
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
    }
#endif
}
