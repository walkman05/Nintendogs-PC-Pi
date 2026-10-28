#include "ctx.h"

extern s32 func_01ffdb08(s32 p);
#ifdef NDOGS_PC /* NDOGS_PC: return type of func_02028118 as defined (x86 extends by the caller's prototype, the DS by the callee) */
extern u32 func_02028118(s32 p);
#else
extern u16 func_02028118(s32 p);
#endif

s32 func_02109340(s32 param_1)
{
    u16 r = 0;
    s32 v = func_01ffdb08(param_1);
    if (v != 0) {
        r = func_02028118(v);
    }
    return r;
}
