#include "ctx.h"
#include "nitro.h"
/* Node callback: when mgr+0x99 is 0, rotate the node by func_01ff9184 side 1; otherwise, in world space
 * (node * mgr+0x1e0), aim row 0 along mgr+0x420 - mgr+0x42c (row 2 as up hint), scale row 0 by 0x1338 and rows 1-2
 * by 0x10d1 (fx32, rounded) and map back with mgr+0x210. Then blend with mgr+0x79c when the weight at mgr+0x45c is
 * below 1.0, restore the translation, store the node at mgr+0x79c, and if func_02027428(mgr) == 1 transform the
 * translation by mgr+0x1e0 into mgr+0x360. */

extern u8 *data_027c0360;
extern void func_01ff9184(u8 *mgr, s32 *m, s32 side);
extern void func_01ff810c(const MtxFx43 *a, const MtxFx43 *b, MtxFx43 *ab);  /* MTX_Concat43 */
extern void func_02067e34(s32 *a, s32 *b, s32 *out);                          /* VEC_Subtract: out = a - b */
extern BOOL func_01ff89d0(VecFx32 *v);                                        /* guarded VEC_Normalize */
extern void func_02067d04(s32 *a, s32 *b, u32 *out);                          /* VEC_CrossProduct */
extern void func_01ff86bc(const s32 *a, const s32 *b, s32 *out, s32 t);
extern u64 func_0206c8d4(u32 *src, u32 *dst);
extern s32 func_02027428(u8 *p);
extern void func_01ff8894(const s32 *v, const s32 *m, s32 *out);

/* FX_Mul with rounding: low 32 bits of (a * b + 0x800) >> 12 */
static s32 MulRound(s32 a, s32 b) { return (s32)(u32)(((u64)((s64)a * b) + 0x800u) >> 12); }

void func_01ff8aa0(void *unused, s32 *m)
{
    u8 *mgr = data_027c0360;
    s32 r[9];
    s32 tx;
    s32 ty;
    s32 tz;
    s32 t;
    int i;

    (void)unused;
    tz = m[11];
    ty = m[10];
    tx = m[9];
    if (*(s8 *)(mgr + 0x99) == 0) {
        func_01ff9184(mgr, m, 1);
    } else {
        func_01ff810c((const MtxFx43 *)m, (const MtxFx43 *)(mgr + 0x1e0), (MtxFx43 *)m);
        func_02067e34((s32 *)(mgr + 0x420), (s32 *)(mgr + 0x42c), &r[0]);
        r[6] = m[6];
        r[7] = m[7];
        r[8] = m[8];
        func_01ff89d0((VecFx32 *)&r[0]);
        func_02067d04(&r[6], &r[0], (u32 *)&r[3]);
        func_01ff89d0((VecFx32 *)&r[3]);
        func_02067d04(&r[0], &r[3], (u32 *)&r[6]);
        for (i = 0; i < 3; i++)
            r[i] = MulRound(r[i], 0x1338);
        for (i = 3; i < 9; i++)
            r[i] = MulRound(r[i], 0x10d1);
        for (i = 0; i < 9; i++)
            m[i] = r[i];
        func_01ff810c((const MtxFx43 *)m, (const MtxFx43 *)(mgr + 0x210), (MtxFx43 *)m);
    }
    t = *(s32 *)(mgr + 0x45c);
    if (t < 0x1000)
        func_01ff86bc((const s32 *)(mgr + 0x79c), m, m, t);
    m[9] = tx;
    m[10] = ty;
    m[11] = tz;
    func_0206c8d4((u32 *)m, (u32 *)(mgr + 0x79c));
    if (func_02027428(mgr) != 1)
        return;
    func_01ff8894(m, (const s32 *)(mgr + 0x1e0), (s32 *)(mgr + 0x360));
}
