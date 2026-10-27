#include "ctx.h"
/* _u32_div: unsigned 32-bit divide, quotient in r0 (low word), remainder in r1 (high word).
 * Divide by zero returns (a, 0). */

u64 func_0205e47c(u32 a, u32 b)
{
    if (b == 0) {
        return a;
    }
    return ((u64)(a % b) << 32) | (a / b);
}
