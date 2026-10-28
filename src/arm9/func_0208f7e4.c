#include "ctx.h"

extern void func_02077e4c(u32 a);

/* the second argument (r1) passes straight through to the callee */
void func_0208f7e4(u8 *p, u32 v)
{
    u8 *q = *(u8 **)(p + 0x1c);
    ((void (*)(u32, u32))func_02077e4c)(*(u32 *)(q + 8), v);
}
