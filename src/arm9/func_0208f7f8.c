#include "ctx.h"

extern void func_02077e94(void);

/* the second argument (r1) passes straight through to the callee */
void func_0208f7f8(u8 *p, u32 v)
{
    u8 *q = *(u8 **)(p + 0x1c);
    void (*fp)(u32, u32) = (void (*)(u32, u32))func_02077e94;
    fp(*(u32 *)(q + 8), v);
}
