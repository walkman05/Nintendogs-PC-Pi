#include "ctx.h"

extern void func_02067728(u32 a, u32 b);
extern s32 func_020677c8(void);

s32 func_02067898(u32 param_1, u32 param_2)
{
    func_02067728(param_1, param_2);
    return func_020677c8();
}
