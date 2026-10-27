#include "ctx.h"
#include "host_softfp.h"
/* _fgr: float (raw bits) a > b, returns 1 or 0; signalling compare: any NaN operand returns 0 and raises IOC.
 * Entry point inside func_0205c288 (the comparison block). */

s32 func_0205c454(s32 ia, s32 ib)
{
    u32 a = (u32)ia;
    u32 b = (u32)ib;
    s32 ka, kb;

    if (F32IsNan(a) || F32IsNan(b)) {
        SfRaise(SF_IOC);
        return 0;
    }
    /* sign-magnitude to two's complement (both zeros map to 0) */
    ka = (a & 0x80000000u) ? (s32)(0u - (a & 0x7fffffffu)) : (s32)a;
    kb = (b & 0x80000000u) ? (s32)(0u - (b & 0x7fffffffu)) : (s32)b;
    return ka > kb;
}
