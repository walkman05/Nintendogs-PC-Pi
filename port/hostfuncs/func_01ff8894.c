#include "ctx.h"
/* MTX_MultVec43-style transform: out = m(4x3, fx32) * (v[9], v[10], v[11]), i.e. the vector stored at +0x24 of v
 * (the translation row of a 4x3 matrix) transformed by the 3x3 part of m plus m's translation. */

static s32 Dot3Shift(s32 a0, s32 b0, s32 a1, s32 b1, s32 a2, s32 b2)
{
    u64 acc = (u64)((s64)a0 * b0) + (u64)((s64)a1 * b1) + (u64)((s64)a2 * b2);  /* wraps like smlal */
    return (s32)(u32)(acc >> 12);
}

void func_01ff8894(const s32 *v, const s32 *m, s32 *out)
{
    s32 x = v[9];
    s32 y = v[10];
    s32 z = v[11];

    out[0] = (s32)((u32)m[9] + (u32)Dot3Shift(y, m[3], x, m[0], z, m[6]));
    out[1] = (s32)((u32)m[10] + (u32)Dot3Shift(y, m[4], x, m[1], z, m[7]));
    out[2] = (s32)((u32)m[11] + (u32)Dot3Shift(y, m[5], x, m[2], z, m[8]));
}
