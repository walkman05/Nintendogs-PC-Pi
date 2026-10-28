#include "ctx.h"

extern u8 data_02142e74[];
extern u32 data_021594a8;
extern u32 func_020d055c(u8 *a, u32 b);
#ifdef NDOGS_PC /* NDOGS_PC: return type of func_020d0810 as defined (x86 extends by the caller's prototype, the DS by the callee) */
extern u8 func_020d0810(u32 a, u32 b);
#else
extern s32 func_020d0810(u32 a, u32 b);
#endif

s32 func_020cfcf8(u32 param_1, s32 param_2)
{
    u32 r = func_020d055c(data_02142e74, (u32)(param_2 + 1) & 0xff);
    return func_020d0810(data_021594a8, r);
}
