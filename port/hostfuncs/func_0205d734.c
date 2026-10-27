#include "ctx.h"
#include "host_softfp.h"
/* _fmul: IEEE float multiplication on raw bits (ARM soft-float runtime). */

s32 func_0205d734(s32 ia, s32 ib)
{
    u32 a = (u32)ia;
    u32 b = (u32)ib;
    u32 sign = (a ^ b) >> 31;
    s32 expA = (s32)((a >> 23) & 0xff);
    s32 expB = (s32)((b >> 23) & 0xff);
    u32 sigA = a & 0x007fffffu;
    u32 sigB = b & 0x007fffffu;
    u64 p;
    u32 sig;
    s32 exp;

    if (F32IsNan(a) || F32IsNan(b)) {
        return (s32)F32NanResult(a, b);
    }
    if (expA == 0xff || expB == 0xff) {
        if ((expA == 0 && sigA == 0) || (expB == 0 && sigB == 0)) {
            return (s32)F32Invalid();
        }
        return (s32)((sign << 31) | 0x7f800000u);
    }
    if ((expA == 0 && sigA == 0) || (expB == 0 && sigB == 0)) {
        return (s32)(sign << 31);
    }
    if (expA == 0) {
        sigA = F32NormSub(sigA, &expA);
    } else {
        sigA |= 0x00800000u;
    }
    if (expB == 0) {
        sigB = F32NormSub(sigB, &expB);
    } else {
        sigB |= 0x00800000u;
    }
    /* leading bits at 30 and 31: product leading bit at 61 or 62 */
    exp = expA + expB - 0x7f;
    p = (u64)(sigA << 7) * (u64)(sigB << 8);
    sig = (u32)(p >> 32) | ((u32)p != 0);
    if (sig < 0x40000000u) {
        exp--;
        sig <<= 1;
    }
    return (s32)F32RoundPack(sign, exp, sig);
}
