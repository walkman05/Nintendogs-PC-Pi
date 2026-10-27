#include "ctx.h"

extern u32 data_02159464;
extern u32 func_020cfad0(u8 *p);

s32 func_020bc130(void)
{
    u32 (*fp)(u8 *) = func_020cfad0;
    return fp(*(u8 **)&data_02159464 + 0x88);
}
