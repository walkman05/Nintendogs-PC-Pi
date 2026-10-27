#include "ctx.h"
#include "host_softfp.h"
/* _deq: double a == b (args as lo/hi words, same layout as two doubles on cdecl), returns 1 or 0; quiet compare:
 * NaN gives 0, IOC only for a signalling NaN. Entry point inside func_0205c288 (the comparison block). */

s32 func_0205c7f0(u32 alo, u32 ahi, u32 blo, u32 bhi)
{
    u64 a = ((u64)ahi << 32) | alo;
    u64 b = ((u64)bhi << 32) | blo;

    if (F64IsNan(a) || F64IsNan(b)) {
        if (F64IsSnan(a) || F64IsSnan(b)) {
            SfRaise(SF_IOC);
        }
        return 0;
    }
    if (((a | b) & 0x7fffffffffffffffull) == 0) {
        return 1;
    }
    return a == b;
}
