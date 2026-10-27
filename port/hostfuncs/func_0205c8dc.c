#include "ctx.h"
#include "host_softfp.h"
/* _dls: double a < b (args as lo/hi words), returns 1 or 0; signalling compare: any NaN operand returns 0 and
 * raises IOC. Entry point inside func_0205c288 (the comparison block). */

s32 func_0205c8dc(u32 alo, u32 ahi, u32 blo, u32 bhi)
{
    u64 a = ((u64)ahi << 32) | alo;
    u64 b = ((u64)bhi << 32) | blo;

    if (F64IsNan(a) || F64IsNan(b)) {
        SfRaise(SF_IOC);
        return 0;
    }
    if (((a | b) & 0x7fffffffffffffffull) == 0) {
        return 0;
    }
    if ((a | b) & 0x8000000000000000ull) {
        /* at least one negative: the raw bits compared the other way round order them correctly */
        return b < a;
    }
    return a < b;
}
