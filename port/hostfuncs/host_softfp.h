/* Shared helpers for the host versions of the ARM soft-float runtime (func_0205aab0 ... func_0205dc20).
 * The DS library is IEEE 754 binary32/binary64 done in integer code: round mode from bits 22-23 of the FP status
 * word (0 nearest-even, 1 +inf, 2 -inf, 3 zero), sticky flags IOC 0x1, DZC 0x2, OFC 0x4, UFC 0x8, IXC 0x10,
 * gradual underflow with tininess detected before rounding (UFC only when inexact), and every NaN result is the
 * default NaN 0x7fffffff / 0x7fffffff_ffffffff (IOC if an operand was a signalling NaN).
 * The trap-enable bits (0x100-0x1000) are not emulated: the flag is set and the default result returned. */
#ifndef HOST_SOFTFP_H
#define HOST_SOFTFP_H
#include "ctx.h"

extern u32 func_02063cb8(void); /* address of the FP status word */

#define SF_IOC 0x01u
#define SF_DZC 0x02u
#define SF_OFC 0x04u
#define SF_UFC 0x08u
#define SF_IXC 0x10u

#define F32_DEFAULT_NAN 0x7fffffffu
#define F64_DEFAULT_NAN 0x7fffffffffffffffull

static u32 SfStatus(void) { return *(u32 *)func_02063cb8(); }
static void SfRaise(u32 f) { *(u32 *)func_02063cb8() |= f; }
static u32 SfMode(void) { return (SfStatus() >> 22) & 3u; }

static int SfClz32(u32 x)
{
    int n = 0;
    if (x == 0) {
        return 32;
    }
    while ((x & 0x80000000u) == 0) {
        x <<= 1;
        n++;
    }
    return n;
}

static int SfClz64(u64 x)
{
    u32 hi = (u32)(x >> 32);
    return hi != 0 ? SfClz32(hi) : 32 + SfClz32((u32)x);
}

static u32 SfShr32Jam(u32 a, int n)
{
    if (n <= 0) {
        return a;
    }
    if (n >= 32) {
        return a != 0;
    }
    return (a >> n) | ((a << (32 - n)) != 0);
}

static u64 SfShr64Jam(u64 a, int n)
{
    if (n <= 0) {
        return a;
    }
    if (n >= 64) {
        return a != 0;
    }
    return (a >> n) | ((a << (64 - n)) != 0);
}

/* binary32 ------------------------------------------------------------------------------------------------- */
static int F32IsNan(u32 a) { return (a & 0x7fffffffu) > 0x7f800000u; }
static int F32IsSnan(u32 a) { return F32IsNan(a) && (a & 0x00400000u) == 0; }

static u32 F32NanResult(u32 a, u32 b)
{
    if (F32IsSnan(a) || F32IsSnan(b)) {
        SfRaise(SF_IOC);
    }
    return F32_DEFAULT_NAN;
}

static u32 F32Invalid(void)
{
    SfRaise(SF_IOC);
    return F32_DEFAULT_NAN;
}

/* sig: leading bit at 30 (or 0), exp = biased exponent - 1; 7 round bits. */
static u32 F32RoundPack(u32 sign, s32 exp, u32 sig)
{
    u32 mode = SfMode();
    u32 inc;
    u32 rbits;
    u32 flags = 0;

    if (mode == 0) {
        inc = 0x40;
    } else if (mode == 3) {
        inc = 0;
    } else if (mode == 1) {
        inc = sign ? 0 : 0x7f;
    } else {
        inc = sign ? 0x7f : 0;
    }
    rbits = sig & 0x7f;
    if ((u32)exp >= 0xfd) {
        if (exp < 0) {
            sig = SfShr32Jam(sig, -exp);
            exp = 0;
            rbits = sig & 0x7f;
            if (rbits != 0) {
                flags |= SF_UFC;
            }
        } else if (exp > 0xfd || sig + inc >= 0x80000000u) {
            SfRaise(SF_OFC | SF_IXC);
            return (sign << 31) + 0x7f800000u - (inc == 0);
        }
    }
    if (rbits != 0) {
        flags |= SF_IXC;
    }
    if (flags != 0) {
        SfRaise(flags);
    }
    sig = (sig + inc) >> 7;
    if (mode == 0 && rbits == 0x40) {
        sig &= ~1u;
    }
    if (sig == 0) {
        exp = 0;
    }
    return (sign << 31) + ((u32)exp << 23) + sig;
}

/* Normalises a non-zero denormal fraction: returns the significand with its leading bit at 23. */
static u32 F32NormSub(u32 frac, s32 *exp)
{
    int s = SfClz32(frac) - 8;
    *exp = 1 - s;
    return frac << s;
}

static u32 F32AddSub(u32 a, u32 b, int sub)
{
    u32 signA, signB;
    s32 expA, expB, exp;
    u32 sigA, sigB, sig;

    if (F32IsNan(a) || F32IsNan(b)) {
        return F32NanResult(a, b);
    }
    if (sub) {
        b ^= 0x80000000u;
    }
    signA = a >> 31;
    signB = b >> 31;
    expA = (s32)((a >> 23) & 0xff);
    expB = (s32)((b >> 23) & 0xff);
    if (expA == 0xff) {
        if (expB == 0xff && signA != signB) {
            return F32Invalid();
        }
        return a;
    }
    if (expB == 0xff) {
        return b;
    }
    /* implicit bit at 29; denormals use exponent 1 */
    sigA = (a & 0x007fffffu) << 6;
    sigB = (b & 0x007fffffu) << 6;
    if (expA != 0) {
        sigA |= 0x20000000u;
    } else {
        expA = 1;
    }
    if (expB != 0) {
        sigB |= 0x20000000u;
    } else {
        expB = 1;
    }
    if (signA == signB) {
        if (expA >= expB) {
            sigB = SfShr32Jam(sigB, expA - expB);
            exp = expA;
        } else {
            sigA = SfShr32Jam(sigA, expB - expA);
            exp = expB;
        }
        sig = sigA + sigB;
        if (sig & 0x40000000u) {
            return F32RoundPack(signA, exp, sig);
        }
        return F32RoundPack(signA, exp - 1, sig << 1);
    } else {
        u32 sign = signA;
        int s;
        if (expA > expB || (expA == expB && sigA >= sigB)) {
            sigB = SfShr32Jam(sigB, expA - expB);
            exp = expA;
            sig = sigA - sigB;
        } else {
            sigA = SfShr32Jam(sigA, expB - expA);
            exp = expB;
            sig = sigB - sigA;
            sign = signB;
        }
        if (sig == 0) {
            return SfMode() == 2 ? 0x80000000u : 0;
        }
        s = SfClz32(sig) - 1;
        return F32RoundPack(sign, exp - s, sig << s);
    }
}

/* binary64 ------------------------------------------------------------------------------------------------- */
static int F64IsNan(u64 a) { return (a & 0x7fffffffffffffffull) > 0x7ff0000000000000ull; }
static int F64IsSnan(u64 a) { return F64IsNan(a) && (a & 0x0008000000000000ull) == 0; }

static u64 F64NanResult(u64 a, u64 b)
{
    if (F64IsSnan(a) || F64IsSnan(b)) {
        SfRaise(SF_IOC);
    }
    return F64_DEFAULT_NAN;
}

static u64 F64Invalid(void)
{
    SfRaise(SF_IOC);
    return F64_DEFAULT_NAN;
}

/* sig: leading bit at 62 (or 0), exp = biased exponent - 1; 10 round bits. */
static u64 F64RoundPack(u32 sign, s32 exp, u64 sig)
{
    u32 mode = SfMode();
    u64 inc;
    u32 rbits;
    u32 flags = 0;

    if (mode == 0) {
        inc = 0x200;
    } else if (mode == 3) {
        inc = 0;
    } else if (mode == 1) {
        inc = sign ? 0 : 0x3ff;
    } else {
        inc = sign ? 0x3ff : 0;
    }
    rbits = (u32)sig & 0x3ff;
    if ((u32)exp >= 0x7fd) {
        if (exp < 0) {
            sig = SfShr64Jam(sig, -exp);
            exp = 0;
            rbits = (u32)sig & 0x3ff;
            if (rbits != 0) {
                flags |= SF_UFC;
            }
        } else if (exp > 0x7fd || sig + inc >= 0x8000000000000000ull) {
            SfRaise(SF_OFC | SF_IXC);
            return ((u64)sign << 63) + 0x7ff0000000000000ull - (inc == 0);
        }
    }
    if (rbits != 0) {
        flags |= SF_IXC;
    }
    if (flags != 0) {
        SfRaise(flags);
    }
    sig = (sig + inc) >> 10;
    if (mode == 0 && rbits == 0x200) {
        sig &= ~1ull;
    }
    if (sig == 0) {
        exp = 0;
    }
    return ((u64)sign << 63) + ((u64)(u32)exp << 52) + sig;
}

static u64 F64NormSub(u64 frac, s32 *exp)
{
    int s = SfClz64(frac) - 11;
    *exp = 1 - s;
    return frac << s;
}

static u64 F64AddSub(u64 a, u64 b, int sub)
{
    u32 signA, signB;
    s32 expA, expB, exp;
    u64 sigA, sigB, sig;

    if (F64IsNan(a) || F64IsNan(b)) {
        return F64NanResult(a, b);
    }
    if (sub) {
        b ^= 0x8000000000000000ull;
    }
    signA = (u32)(a >> 63);
    signB = (u32)(b >> 63);
    expA = (s32)((a >> 52) & 0x7ff);
    expB = (s32)((b >> 52) & 0x7ff);
    if (expA == 0x7ff) {
        if (expB == 0x7ff && signA != signB) {
            return F64Invalid();
        }
        return a;
    }
    if (expB == 0x7ff) {
        return b;
    }
    /* implicit bit at 61; denormals use exponent 1 */
    sigA = (a & 0x000fffffffffffffull) << 9;
    sigB = (b & 0x000fffffffffffffull) << 9;
    if (expA != 0) {
        sigA |= 0x2000000000000000ull;
    } else {
        expA = 1;
    }
    if (expB != 0) {
        sigB |= 0x2000000000000000ull;
    } else {
        expB = 1;
    }
    if (signA == signB) {
        if (expA >= expB) {
            sigB = SfShr64Jam(sigB, expA - expB);
            exp = expA;
        } else {
            sigA = SfShr64Jam(sigA, expB - expA);
            exp = expB;
        }
        sig = sigA + sigB;
        if (sig & 0x4000000000000000ull) {
            return F64RoundPack(signA, exp, sig);
        }
        return F64RoundPack(signA, exp - 1, sig << 1);
    } else {
        u32 sign = signA;
        int s;
        if (expA > expB || (expA == expB && sigA >= sigB)) {
            sigB = SfShr64Jam(sigB, expA - expB);
            exp = expA;
            sig = sigA - sigB;
        } else {
            sigA = SfShr64Jam(sigA, expB - expA);
            exp = expB;
            sig = sigB - sigA;
            sign = signB;
        }
        if (sig == 0) {
            return SfMode() == 2 ? 0x8000000000000000ull : 0;
        }
        s = SfClz64(sig) - 1;
        return F64RoundPack(sign, exp - s, sig << s);
    }
}

typedef union {
    double d;
    u64 u;
} SfF64Bits;

static u64 SfDoubleBits(double d)
{
    SfF64Bits v;
    v.d = d;
    return v.u;
}

static double SfBitsDouble(u64 u)
{
    SfF64Bits v;
    v.u = u;
    return v.d;
}

#endif
