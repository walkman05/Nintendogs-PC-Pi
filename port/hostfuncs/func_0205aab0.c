#include "ctx.h"
#include "host_softfp.h"
/* _dadd: IEEE double addition (ARM soft-float runtime). */

double func_0205aab0(double a, double b)
{
    return SfBitsDouble(F64AddSub(SfDoubleBits(a), SfDoubleBits(b), 0));
}
