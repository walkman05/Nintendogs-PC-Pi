#include "ctx.h"
#include "host_softfp.h"
/* _ffix: float (raw bits) to signed int, truncating toward zero. |x| >= 2^31 and NaN raise IOC and saturate by
 * the sign bit (0x7fffffff / 0x80000000; -2^31 itself is exact); dropped fraction bits raise IXC. */

s32 func_0205d4f0(s32 ia)
{
    u32 a = (u32)ia;
    u32 mag = a & 0x7fffffffu;
    s32 sh = 0x9e - (s32)(mag >> 23);
    u32 m;
    u32 r;

    if (sh <= 0) {
        if (a != 0xcf000000u) {
            SfRaise(SF_IOC);
        }
        return ia < 0 ? (s32)0x80000000u : 0x7fffffff;
    }
    if (sh >= 32) {
        if (mag != 0) {
            SfRaise(SF_IXC);
        }
        return 0;
    }
    m = (mag << 8) | 0x80000000u;
    r = m >> sh;
    if (ia < 0) {
        r = 0u - r;
    }
    if ((m << (32 - sh)) != 0) {
        SfRaise(SF_IXC);
    }
    return (s32)r;
}
