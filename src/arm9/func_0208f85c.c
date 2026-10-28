#include "ctx.h"

extern void func_0208e690(void);

/* the second argument (r1) passes straight through to the callee */
void func_0208f85c(u8 *p, u32 v)
{
    void (*fp)(u32, u32) = (void (*)(u32, u32))func_0208e690;
    fp(*(u32 *)(p + 0x74), v);
}
