#include "ctx.h"
/* _ull_mod: unsigned 64-bit remainder a % b (r0:r1 % r2:r3); divide by zero returns a unchanged.
 * Entry point inside func_0205e068 (_ull_div). */

u64 func_0205e074(u64 a, u64 b)
{
    if (b == 0) {
        return a;
    }
    return a % b;
}
