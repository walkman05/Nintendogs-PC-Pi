#include "ctx.h"
/* _ull_div: unsigned 64-bit divide a / b (r0:r1 / r2:r3); divide by zero returns a unchanged. */

u64 func_0205e068(u64 a, u64 b)
{
    if (b == 0) {
        return a;
    }
    return a / b;
}
