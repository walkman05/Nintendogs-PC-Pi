#include "ctx.h"

extern s32 func_0207d1b4(void);
extern u8 data_02155c74;

s32 func_0207d0c4(u32 a)
{
    s32 (*fp)(void *, u32) = (s32 (*)(void *, u32))func_0207d1b4;
    return fp(&data_02155c74, a);
}
