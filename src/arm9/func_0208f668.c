#include "ctx.h"

extern u32 func_0207ce4c(u8 *p, u32 v);

u32 func_0208f668(u8 *p, u32 v)
{
    u32 (*fp)(u8 *, u32) = func_0207ce4c;
    return fp(p + 60, (u16)v);
}
