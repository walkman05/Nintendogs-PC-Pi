#include "ctx.h"

extern u8 *data_02159464;
extern s32 func_020ced70(u8 *a);

s32 func_020bbdc0(void)
{
    return ((s32 (*)(u8 *))func_020ced70)(data_02159464 + 0x144);
}
