#include "ctx.h"
#include "nitro.h"
/* FX_Div with a zero guard: returns numer when denom is 0, else (numer << 32) / denom rounded (q + 0x80000) >> 20. */

fx32 func_01ff8958(fx32 numer, fx32 denom)
{
    s64 n;
    s64 q;

    if (denom == 0)
        return numer;
    /* The original goes through the divider (DIVCNT mode 1: 64-bit numer numer << 32, 32-bit denom); the quotient
       is computed here directly. */
    n = (s64)numer * ((s64)1 << 32);
    if (n == (s64)0x8000000000000000ULL && denom == -1)
        q = n; /* -2^63 / -1 overflows; the 64-bit quotient wraps to -2^63 */
    else
        q = n / (s64)denom;
    return (fx32)(u32)(((u64)q + 0x80000u) >> 20);
}
