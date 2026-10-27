#include "ctx.h"
#include "host_softfp.h"
/* _dsub: IEEE double subtraction a - b (ARM soft-float runtime); returned as raw bits (r0 low, r1 high). */

u64 func_0205b8c0(u32 alo, u32 ahi, u32 blo, u32 bhi)
{
    return F64AddSub(((u64)ahi << 32) | alo, ((u64)bhi << 32) | blo, 1);
}
