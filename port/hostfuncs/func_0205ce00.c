#include "ctx.h"
#include "host_softfp.h"
/* _fdiv: IEEE float division a / b on raw bits (ARM soft-float runtime). */

s32 func_0205ce00(s32 ia, s32 ib)
{
    u32 a = (u32)ia;
    u32 b = (u32)ib;
    u32 sign = (a ^ b) >> 31;
    s32 expA = (s32)((a >> 23) & 0xff);
    s32 expB = (s32)((b >> 23) & 0xff);
    u32 sigA = a & 0x007fffffu;
    u32 sigB = b & 0x007fffffu;
    int zeroA = expA == 0 && sigA == 0;
    int zeroB = expB == 0 && sigB == 0;
    u64 num;
    u32 q;
    s32 exp;

    if (F32IsNan(a) || F32IsNan(b)) {
        return (s32)F32NanResult(a, b);
    }
    if (expA == 0xff) {
        if (expB == 0xff) {
            return (s32)F32Invalid();
        }
        return (s32)((sign << 31) | 0x7f800000u);
    }
    if (expB == 0xff) {
        return (s32)(sign << 31);
    }
    if (zeroB) {
        if (zeroA) {
            return (s32)F32Invalid();
        }
        SfRaise(SF_DZC);
        return (s32)((sign << 31) | 0x7f800000u);
    }
    if (zeroA) {
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
    /* quotient with its leading bit at 30, sticky in bit 0 */
    if (sigA >= sigB) {
        num = (u64)sigA << 30;
        exp = expA - expB + 0x7e;
    } else {
        num = (u64)sigA << 31;
        exp = expA - expB + 0x7d;
    }
    q = (u32)(num / sigB);
    if (num % sigB != 0) {
        q |= 1;
    }
    return (s32)F32RoundPack(sign, exp, q);
}
