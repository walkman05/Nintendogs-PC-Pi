#include "ctx.h"
#include "nitro.h"
/* VEC_MultAdd: dst = (a * v1 >> 12, truncated) + v2, per component in x, y, z order; returns dst->z (left in r0). */

static fx32 hi12(u64 v) { return (fx32)(u32)(v >> 12); }

fx32 func_01ff832c(fx32 a, const VecFx32 *v1, const VecFx32 *v2, VecFx32 *dst)
{
    fx32 t;

    t = hi12((u64)((s64)a * v1->x));
    dst->x = (fx32)((u32)v2->x + (u32)t);
    t = hi12((u64)((s64)a * v1->y));
    dst->y = (fx32)((u32)v2->y + (u32)t);
    t = hi12((u64)((s64)a * v1->z));
    t = (fx32)((u32)v2->z + (u32)t);
    dst->z = t;
    return t;
}
