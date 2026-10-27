#include "ctx.h"
/* _s32_div: signed 32-bit divide, quotient in r0 (low word), remainder (sign of the dividend) in r1 (high word).
 * Divide by zero returns (a, 0); 0x80000000 / -1 gives quotient 0x80000000, remainder 0. */

u64 func_0205e270(s32 a, s32 b)
{
    u32 ua = a < 0 ? 0u - (u32)a : (u32)a;
    u32 ub = b < 0 ? 0u - (u32)b : (u32)b;
    u32 q, r;

    if (ub == 0) {
        return (u32)a;
    }
    q = ua / ub;
    r = ua % ub;
    if ((a < 0) != (b < 0)) {
        q = 0u - q;
    }
    if (a < 0) {
        r = 0u - r;
    }
    return ((u64)r << 32) | q;
}
