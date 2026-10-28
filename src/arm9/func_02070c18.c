#include "ctx.h"

extern void func_02070434(u32 v);

/* the second argument (r1) passes straight through to the callee */
void func_02070c18(u32 *p, u32 v)
{
    ((void (*)(u32, u32))func_02070434)(*p, v);
}
