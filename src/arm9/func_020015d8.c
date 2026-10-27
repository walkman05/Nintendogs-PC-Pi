#include "ctx.h"

extern void func_02001164(u32 p);
extern s32 func_020015f8(u16 *p);

s32 func_020015d8(u16 *param_1)
{
    func_02001164(1);
    return func_020015f8(param_1);
}
