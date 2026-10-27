#include "ctx.h"
#include "host_softfp.h"
/* _fflt: signed int to float (raw bits), rounded per the current rounding mode (IXC when inexact).
 * Entry point inside func_0205d5ec (the unsigned variant). */

s32 func_0205d668(s32 a)
{
    u32 sign = (u32)a >> 31;
    u32 mag = sign ? 0u - (u32)a : (u32)a;
    int s;

    if (mag == 0) {
        return 0;
    }
    s = SfClz32(mag);
    /* leading bit at 31 -> shift to 30 with sticky; biased exponent - 1 = 0x9d - s */
    return (s32)F32RoundPack(sign, 0x9d - s, SfShr32Jam(mag << s, 1));
}
