#include "ctx.h"
#ifdef NDOGS_PC
extern void host_ctx_forget(u32 *ctx);
#endif

/* OS_InitContext(ctx, entry, stack) */
void func_0207f980(u32 *param_1, u32 param_2, u32 param_3)
{
#ifdef NDOGS_PC
    host_ctx_forget(param_1);  /* a new thread: the host gives it a fresh fiber on first switch */
#endif
#ifdef NDOGS_PC
    param_2 = param_2 + 4;
#else
    asm { add param_2, param_2, #4 }
#endif
    param_1[16] = param_2;
    param_1[17] = param_3;
    param_1[14] = param_3 - 64;
#ifdef NDOGS_PC
    param_2 = (param_2 & 1) ? 63 : 31;
#else
    asm { ands param_2, param_2, #1 }
    asm { movne param_2, #63 }
    asm { moveq param_2, #31 }
#endif
    param_1[0] = param_2;
    param_1[1] = 0;
    param_1[2] = 0;
    param_1[3] = 0;
    param_1[4] = 0;
    param_1[5] = 0;
    param_1[6] = 0;
    param_1[7] = 0;
    param_1[8] = 0;
    param_1[9] = 0;
    param_1[10] = 0;
    param_1[11] = 0;
    param_1[12] = 0;
    param_1[13] = 0;
    param_1[15] = 0;
}
