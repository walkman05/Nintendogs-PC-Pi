#include "ctx.h"

extern s32 func_01ffdb08(s32 idx);
#ifdef NDOGS_PC /* NDOGS_PC: return type of func_02028100 as defined (x86 extends by the caller's prototype, the DS by the callee) */
extern u32 func_02028100(s32 v);
#else
extern u8 func_02028100(s32 v);
#endif

u32 func_02109368(s32 param_1)
{
    u8 r = 0;
    s32 v = func_01ffdb08(param_1);
    if (v != 0) {
        r = func_02028100(v);
    }
    return r;
}
