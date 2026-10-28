#include "ctx.h"

extern u32 func_020bd858(u8 *p);
#ifdef NDOGS_PC /* NDOGS_PC: return type of func_020bd868 as defined (x86 extends by the caller's prototype, the DS by the callee) */
extern u32 func_020bd868(u8 *p);
#else
extern u8 func_020bd868(u8 *p);
#endif

u32 func_020eefd0(u8 *param_1)
{
    s16 s = *(s16 *)(param_1 + 144);
    if (s == 0) {
        goto c0;
    }
    if (s == 2) {
        goto c2;
    }
    if (s == 3) {
        *(u16 *)(param_1 + 166) = 1;
        *(u32 *)(param_1 + 0xa8) = 0;
    }
    return (u32)param_1;
c0:
    return func_020bd858(param_1);
c2:
    return func_020bd868(param_1);
}
