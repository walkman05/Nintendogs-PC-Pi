#include "ctx.h"
#include "host_softfp.h"
/* _d2ull: double to unsigned long long, truncating. Negative values (including -inf) give 0 with no flag;
 * NaN (either sign) and values >= 2^64 give 0xffffffff_ffffffff with IOC; dropped fraction bits raise IXC. */

u64 func_0205b04c(u32 lo, u32 hi)
{
    u64 a = ((u64)hi << 32) | lo;
    s32 sh;
    u64 m;
    u64 r;

    if (hi & 0x80000000u) {
        if (a > 0xfff0000000000000ull) {
            SfRaise(SF_IOC);
            return 0xffffffffffffffffull;
        }
        return 0;
    }
    sh = 0x43e - (s32)(hi >> 20);
    if (sh < 0) {
        SfRaise(SF_IOC);
        return 0xffffffffffffffffull;
    }
    if (sh >= 64) {
        if (a != 0) {
            SfRaise(SF_IXC);
        }
        return 0;
    }
    m = (a << 11) | 0x8000000000000000ull;
    r = sh == 0 ? m : m >> sh;
    if (sh != 0 && (m << (64 - sh)) != 0) {
        SfRaise(SF_IXC);
    }
    return r;
}
