#include "ctx.h"

extern void func_020cfd4c(void);
extern u32 data_021594a8;

/* the second argument (r1) passes straight through to the callee */
void func_020cfd34(u32 unused, u32 v)
{
    void (*fp)(u32, u32) = (void (*)(u32, u32))func_020cfd4c;
    (void)unused;
    fp(data_021594a8, v);
}
