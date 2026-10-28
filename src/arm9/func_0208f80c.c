#include "ctx.h"

extern void func_02077edc(u32 v);

/* the second argument (r1) passes straight through to the callee */
void func_0208f80c(u8 *p, u32 v)
{
    u32 *q = *(u32 **)(p + 0x1c);
    ((void (*)(u32, u32))func_02077edc)(q[2], v);
}
