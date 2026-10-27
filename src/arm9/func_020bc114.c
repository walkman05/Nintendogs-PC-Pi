#include "ctx.h"

extern u8 *data_02159464;
extern u32 func_020cfabc(void *p);

s32 func_020bc114(void)
{
    u32 (*fp)(void *) = func_020cfabc;
    return fp(data_02159464 + 0x88);
}
