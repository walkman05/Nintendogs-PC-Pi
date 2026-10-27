#include "ctx.h"

extern s32 func_0206d7a4(void);

s32 func_0206d7d8(void)
{
    s32 (*fp)(u32) = (s32 (*)(u32))func_0206d7a4;
    return fp(39);
}
