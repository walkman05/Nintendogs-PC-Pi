#include "ctx.h"
#include "nitro.h"
/* Guarded VEC_Normalize in place: leaves v alone and returns FALSE when x*x+y*y+z*z is below the 64-bit threshold
   at data_02126728, else normalizes v and returns whether the SDK normalize returned nonzero. */

extern s32 func_02006a30(s32 *src, s32 *dst); /* VEC_Normalize */
extern const u32 data_02126728[2];            /* s64 threshold, lo word first */

BOOL func_01ff89d0(VecFx32 *v)
{
    u64 sum;
    u64 thr;

    sum = (u64)((s64)v->y * v->y) + (u64)((s64)v->x * v->x) + (u64)((s64)v->z * v->z);
    thr = ((u64)data_02126728[1] << 32) | data_02126728[0];
    /* signed 64-bit compare (subs/sbcs; blt) on the wrapped sum, done on biased unsigned values */
    if ((sum ^ 0x8000000000000000ULL) < (thr ^ 0x8000000000000000ULL))
        return FALSE;
    return func_02006a30(&v->x, &v->x) != 0;
}
