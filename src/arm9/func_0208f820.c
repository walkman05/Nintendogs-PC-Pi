#include "ctx.h"

extern void func_02077f24(u32 a);

/* the second argument (r1) passes straight through to the callee */
void func_0208f820(u8 *p, u32 v)
{
    void (*fp)(u32, u32) = (void (*)(u32, u32))func_02077f24;
    fp(*(u32 *)(*(u8 **)(p + 0x1c) + 8), v);
}
