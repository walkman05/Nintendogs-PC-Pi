#include "ctx.h"
#include "host_softfp.h"
/* _dmul: IEEE double multiplication (ARM soft-float runtime). */

static u64 Mul64Hi(u64 a, u64 b, u64 *lo)
{
    u64 a0 = (u32)a, a1 = a >> 32, b0 = (u32)b, b1 = b >> 32;
    u64 p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
    u64 mid = (p00 >> 32) + (u32)p01 + (u32)p10;
    *lo = (mid << 32) | (u32)p00;
    return p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
}

double func_0205b1b8(double da, double db)
{
    u64 a = SfDoubleBits(da);
    u64 b = SfDoubleBits(db);
    u32 sign = (u32)((a ^ b) >> 63);
    s32 expA = (s32)((a >> 52) & 0x7ff);
    s32 expB = (s32)((b >> 52) & 0x7ff);
    u64 sigA = a & 0x000fffffffffffffull;
    u64 sigB = b & 0x000fffffffffffffull;
    u64 hi, lo;
    s32 exp;

    if (F64IsNan(a) || F64IsNan(b)) {
        return SfBitsDouble(F64NanResult(a, b));
    }
    if (expA == 0x7ff || expB == 0x7ff) {
        if ((expA == 0 && sigA == 0) || (expB == 0 && sigB == 0)) {
            return SfBitsDouble(F64Invalid());
        }
        return SfBitsDouble(((u64)sign << 63) | 0x7ff0000000000000ull);
    }
    if ((expA == 0 && sigA == 0) || (expB == 0 && sigB == 0)) {
        return SfBitsDouble((u64)sign << 63);
    }
    if (expA == 0) {
        sigA = F64NormSub(sigA, &expA);
    } else {
        sigA |= 0x0010000000000000ull;
    }
    if (expB == 0) {
        sigB = F64NormSub(sigB, &expB);
    } else {
        sigB |= 0x0010000000000000ull;
    }
    exp = expA + expB - 0x3ff;
    hi = Mul64Hi(sigA << 10, sigB << 11, &lo);
    hi |= (lo != 0);
    if (hi < 0x4000000000000000ull) {
        exp--;
        hi <<= 1;
    }
    return SfBitsDouble(F64RoundPack(sign, exp, hi));
}
