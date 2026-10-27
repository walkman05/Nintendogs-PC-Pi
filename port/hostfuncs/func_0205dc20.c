#include "ctx.h"
#include "host_softfp.h"
/* _fsub: IEEE float subtraction a - b on raw bits (ARM soft-float runtime). */

s32 func_0205dc20(s32 a, s32 b)
{
    return (s32)F32AddSub((u32)a, (u32)b, 1);
}
