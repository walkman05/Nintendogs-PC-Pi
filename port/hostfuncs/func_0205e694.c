#include "ctx.h"
/* Soft-float status word update: new = (old & ~clear) ^ toggle, returns the old value (fesetenv-style). */
extern u32 func_02063cb8(void);

u32 func_0205e694(u32 clear, u32 toggle)
{
    u32 *p = (u32 *)func_02063cb8();
    u32 old = *p;

    *p = (old & ~clear) ^ toggle;
    return old;
}
