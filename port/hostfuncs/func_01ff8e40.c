#include "ctx.h"
#include "nitro.h"
/* Node callback: when mgr+0x99 is 0, rotate the node by func_01ff9184 side 0; otherwise, in world space
 * (node * mgr+0x1e0), solve the joint J = mgr+0x42c with func_0200babc (node position, target mgr+0x420, both
 * lengths = breed length (data_0213c428) * func_02028130(mgr), bend hint row 1), aim row 0 from the node toward J
 * (row 2 as up hint), scale row 0 by 0xcd0 (fx32, rounded) and map back with mgr+0x210. Then blend with mgr+0x76c
 * when the weight at mgr+0x45c is below 1.0, restore the translation, store the node at mgr+0x76c, transform the
 * data_0213b400 offset by node * mgr+0x1e0 into mgr+0x354, if func_02027428(mgr) == 0 transform the translation by
 * mgr+0x1e0 into mgr+0x360, and set mgr+0x284 = translation . (mgr+0x40, mgr+0x4c, mgr+0x58) + mgr+0x64. */

extern u8 *data_027c0360;
extern u8 data_0213c428[]; /* per-breed records of 100 bytes */
extern const VecFx32 data_0213b400;
extern void func_01ff9184(u8 *mgr, s32 *m, s32 side);
extern u32 func_02028130(u8 *p);
extern void func_0200babc(VecFx32 *out, fx32 ax, fx32 ay, fx32 az, fx32 bx, fx32 by, fx32 bz,
                          fx32 lenA, fx32 lenB, fx32 nx, fx32 ny, fx32 nz);
extern void func_01ff810c(const MtxFx43 *a, const MtxFx43 *b, MtxFx43 *ab);  /* MTX_Concat43 */
extern void func_02067e34(s32 *a, s32 *b, s32 *out);                          /* VEC_Subtract: out = a - b */
extern BOOL func_01ff89d0(VecFx32 *v);                                        /* guarded VEC_Normalize */
extern void func_02067d04(s32 *a, s32 *b, u32 *out);                          /* VEC_CrossProduct */
extern void func_01ff86bc(const s32 *a, const s32 *b, s32 *out, s32 t);
extern u64 func_0206c8d4(u32 *src, u32 *dst);
extern void func_01ff8058(const VecFx32 *v, const MtxFx43 *m, VecFx32 *dst); /* MTX_MultVec43 */
extern s32 func_02027428(u8 *p);
extern void func_01ff8894(const s32 *v, const s32 *m, s32 *out);

/* FX_Mul with rounding: low 32 bits of (a * b + 0x800) >> 12 */
static s32 MulRound(s32 a, s32 b) { return (s32)(u32)(((u64)((s64)a * b) + 0x800u) >> 12); }

void func_01ff8e40(void *unused, s32 *m)
{
    u8 *mgr = data_027c0360;
    s32 r[9];
    s32 pos[3];
    VecFx32 ofs;
    MtxFx43 world;
    s32 tx;
    s32 ty;
    s32 tz;
    s32 t;
    s32 len;
    u64 acc;
    int i;

    (void)unused;
    tz = m[11];
    ty = m[10];
    tx = m[9];
    if (*(s8 *)(mgr + 0x99) == 0) {
        func_01ff9184(mgr, m, 0);
    } else {
        t = (s32)func_02028130(mgr);
        len = MulRound(*(s32 *)(data_0213c428 + (u32)**(u16 **)(mgr + 0x9e8) * 100), t);
        func_01ff810c((const MtxFx43 *)m, (const MtxFx43 *)(mgr + 0x1e0), (MtxFx43 *)m);
        pos[0] = m[9];
        pos[1] = m[10];
        pos[2] = m[11];
        for (i = 3; i < 9; i++)
            r[i] = m[i];
        func_0200babc((VecFx32 *)(mgr + 0x42c), pos[0], pos[1], pos[2], *(s32 *)(mgr + 0x420),
                      *(s32 *)(mgr + 0x424), *(s32 *)(mgr + 0x428), len, len, r[3], r[4], r[5]);
        func_02067e34((s32 *)(mgr + 0x42c), pos, &r[0]);
        func_01ff89d0((VecFx32 *)&r[0]);
        func_02067d04(&r[6], &r[0], (u32 *)&r[3]);
        func_01ff89d0((VecFx32 *)&r[3]);
        func_02067d04(&r[0], &r[3], (u32 *)&r[6]);
        for (i = 0; i < 3; i++)
            r[i] = MulRound(r[i], 0xcd0);
        for (i = 0; i < 9; i++)
            m[i] = r[i];
        func_01ff810c((const MtxFx43 *)m, (const MtxFx43 *)(mgr + 0x210), (MtxFx43 *)m);
    }
    t = *(s32 *)(mgr + 0x45c);
    if (t < 0x1000)
        func_01ff86bc((const s32 *)(mgr + 0x76c), m, m, t);
    m[9] = tx;
    m[10] = ty;
    m[11] = tz;
    func_0206c8d4((u32 *)m, (u32 *)(mgr + 0x76c));
    ofs = data_0213b400;
    func_01ff810c((const MtxFx43 *)m, (const MtxFx43 *)(mgr + 0x1e0), &world);
    func_01ff8058(&ofs, &world, (VecFx32 *)(mgr + 0x354));
    if (func_02027428(mgr) == 0)
        func_01ff8894(m, (const s32 *)(mgr + 0x1e0), (s32 *)(mgr + 0x360));
    /* smull/smlal: 64-bit wrapping sum, low 32 bits of >> 12, plus mgr+0x64 */
    acc = (u64)((s64)m[10] * *(s32 *)(mgr + 0x4c)) + (u64)((s64)m[9] * *(s32 *)(mgr + 0x40)) +
          (u64)((s64)m[11] * *(s32 *)(mgr + 0x58));
    *(s32 *)(mgr + 0x284) = (s32)((u32)*(s32 *)(mgr + 0x64) + (u32)(acc >> 12));
}
