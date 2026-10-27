#include "ctx.h"
/* _ll_div: signed 64-bit divide a / b, truncating; divide by zero returns a unchanged. When both operands fit
 * in 32 bits the original uses _s32_div and sign-extends, so INT32_MIN / -1 gives 0xffffffff_80000000. */

s64 func_0205e0c0(s64 a, s64 b)
{
    u64 ua, ub, q;

    if (b == 0) {
        return a;
    }
    if (a >= -0x80000000ll && a <= 0x7fffffffll && b >= -0x80000000ll && b <= 0x7fffffffll) {
        s32 a32 = (s32)a;
        s32 b32 = (s32)b;
        u32 ua32 = a32 < 0 ? 0u - (u32)a32 : (u32)a32;
        u32 ub32 = b32 < 0 ? 0u - (u32)b32 : (u32)b32;
        u32 q32 = ua32 / ub32;
        if ((a32 < 0) != (b32 < 0)) {
            q32 = 0u - q32;
        }
        return (s64)(s32)q32;
    }
    ua = a < 0 ? 0ull - (u64)a : (u64)a;
    ub = b < 0 ? 0ull - (u64)b : (u64)b;
    q = ua / ub;
    if ((a < 0) != (b < 0)) {
        q = 0ull - q;
    }
    return (s64)q;
}
