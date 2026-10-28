#include "ctx.h"

extern void func_0208e468(void);

/* the second argument (r1) passes straight through to the callee */
void func_0208f84c(void *p, u32 v)
{
    ((void (*)(u32, u32))func_0208e468)(*(u32 *)((u8 *)p + 0x7c), v);
}
