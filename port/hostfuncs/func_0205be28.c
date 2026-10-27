#include "ctx.h"
#include "host_softfp.h"
/* _fadd: IEEE float addition on raw bits (ARM soft-float runtime). */

s32 func_0205be28(s32 a, s32 b)
{
    return (s32)F32AddSub((u32)a, (u32)b, 0);
}
