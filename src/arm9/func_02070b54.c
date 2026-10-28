#include "ctx.h"

void func_020703ec(u32 a);

/* the second argument (r1) passes straight through to the callee */
void func_02070b54(u32 *param_1, u32 v)
{
    void (*fp)(u32, u32) = (void (*)(u32, u32))func_020703ec;
    fp(*param_1, v);
}
