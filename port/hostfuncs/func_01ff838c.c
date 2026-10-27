#include "ctx.h"
/* Three-way rotation blend: rows 0-1 of out = a*(1-wb-wc) + b*wb + c*wc (each product rounded, fx32), then
 * orthonormalize (normalize row0, row2 = row0 x row1 normalized, row1 = row2 x row0) and store 9 words. */

extern const s64 data_02126728;                    /* minimum squared length worth normalizing */
extern void func_02006a30(s32 *src, s32 *dst);      /* VEC_Normalize */
extern void func_02067d04(s32 *a, s32 *b, s32 *out); /* VEC_CrossProduct */

static u32 MulRound(s32 a, s32 b)
{
    return (u32)((u64)((u64)((s64)a * b) + 0x800u) >> 12);
}

static s64 LenSq(const s32 *v)
{
    u64 acc = (u64)((s64)v[1] * v[1]) + (u64)((s64)v[0] * v[0]) + (u64)((s64)v[2] * v[2]);
    return (s64)acc;
}

void func_01ff838c(const s32 *a, const s32 *b, const s32 *c, s32 *out, s32 wb, s32 wc)
{
    s32 m[9];
    s32 wa = (s32)(0x1000u - (u32)wb - (u32)wc);
    s64 thr = data_02126728;
    int i;

    for (i = 0; i < 6; i++) {
        m[i] = (s32)(MulRound(c[i], wc) + MulRound(a[i], wa) + MulRound(b[i], wb));
    }
    if (LenSq(&m[0]) >= thr) {
        func_02006a30(&m[0], &m[0]);
    }
    func_02067d04(&m[0], &m[3], &m[6]);
    if (LenSq(&m[6]) >= thr) {
        func_02006a30(&m[6], &m[6]);
    }
    func_02067d04(&m[6], &m[0], &m[3]);
    for (i = 0; i < 9; i++) {
        out[i] = m[i];
    }
}
