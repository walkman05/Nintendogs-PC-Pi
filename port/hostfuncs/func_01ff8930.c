#include "ctx.h"
#include "nitro.h"
/* FX_Inv with a zero guard: 0 for 0, else (1.0 << 32) / a on the 64/32 divider, rounded (q + 0x80000) >> 20. */

fx32 func_01ff8930(fx32 a)
{
    s64 q;

    if (a == 0)
        return 0;
    /* The original goes through the divider (DIVCNT mode 1: 64-bit numer 0x1000 << 32, 32-bit denom a); the
       quotient is computed here directly. Numerator is positive, so no overflow case exists. */
    q = ((s64)FX32_ONE << 32) / (s64)a;
    return (fx32)(u32)(((u64)q + 0x80000u) >> 20);
}
