#include "ctx.h"

/* Floating-point runtime init: clear the FP status word and the five words of the FP environment. */
extern u32 *func_02063cb8(void);
extern u32 *func_02063cc4(void);

void func_0205e660(void)
{
    u32 *e;
    *func_02063cb8() = 0;
    e = func_02063cc4();
    e[1] = 0;
    e[2] = 0;
    e[3] = 0;
    e[4] = 0;
    e[5] = 0;
}
