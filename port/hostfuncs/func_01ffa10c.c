#include "ctx.h"
#include "nitro.h"
/* Node callback: keep a copy of the node, then unless state mgr+0x93 is 0, 1, 10-13, 19, 20, 23 or 24, aim it in
 * world space (node * mgr+0x1e0, whose translation is stored at mgr+0x2d0) along mgr+0x3a8 - position with row 1
 * as up hint, first pushing that direction d by mode mgr+0x95 (d += (lim - d.n) * n while d.n < lim for the
 * normals at mgr+0x18c / mgr+0x1a4, or by +-8 * func_02028130(mgr) along mgr+0x18c depending on func_02009eb8),
 * and map back with mgr+0x210. Then blend mgr+0x4cc / result / copy with func_01ff838c (weights mgr+0x450 times
 * 0x802 and 0x7fe, rounded), restore the translation, store the node at mgr+0x4cc, and update mgr+0x2d0 (world
 * translation) plus, in mode 0, mgr+0x7d8 (world row 0). */

extern u8 *data_027c0360;
extern void func_01ff810c(const MtxFx43 *a, const MtxFx43 *b, MtxFx43 *ab);  /* MTX_Concat43 */
extern void func_02067e34(s32 *a, s32 *b, s32 *out);                          /* VEC_Subtract: out = a - b */
extern u32 func_02067df0(s32 *a, s32 *b);                                     /* VEC_DotProduct */
extern BOOL func_02009eb8(s32 *a, s32 *b);
extern u32 func_02028130(u8 *p);
extern fx32 func_01ff832c(fx32 a, const VecFx32 *v1, const VecFx32 *v2, VecFx32 *dst); /* VEC_MultAdd */
extern BOOL func_01ff89d0(VecFx32 *v);                                        /* guarded VEC_Normalize */
extern void func_02067d04(s32 *a, s32 *b, u32 *out);                          /* VEC_CrossProduct */
extern void func_01ff838c(const s32 *a, const s32 *b, const s32 *c, s32 *out, s32 wb, s32 wc);
extern u64 func_0206c8d4(u32 *src, u32 *dst);
extern void func_01ff8894(const s32 *v, const s32 *m, s32 *out);

/* FX_Mul with rounding: low 32 bits of (a * b + 0x800) >> 12 */
static s32 MulRound(s32 a, s32 b) { return (s32)(u32)(((u64)((s64)a * b) + 0x800u) >> 12); }

/* d.n is taken first, then the limit from func_02028130(mgr) (times 4 when shl2), in the original's call order */
static void PushOut(u8 *mgr, u32 off, VecFx32 *d, int shl2)
{
    s32 dot = (s32)func_02067df0((s32 *)(mgr + off), &d->x);
    s32 lim = (s32)func_02028130(mgr);

    if (shl2)
        lim = (s32)((u32)lim << 2);
    if (dot < lim)
        func_01ff832c((fx32)((u32)lim - (u32)dot), (const VecFx32 *)(mgr + off), d, d);
}

void func_01ffa10c(void *unused, s32 *m)
{
    u8 *mgr = data_027c0360;
    s32 saved[12];
    VecFx32 x;
    VecFx32 y;
    VecFx32 z;
    MtxFx43 world;
    s32 dot;
    s32 lim;
    s32 w;

    (void)unused;
    func_0206c8d4((u32 *)m, (u32 *)saved);
    switch (*(s8 *)(mgr + 0x93)) {
    case 0:
    case 1:
    case 10:
    case 11:
    case 12:
    case 13:
    case 19:
    case 20:
    case 23:
    case 24:
        break;
    default:
        func_01ff810c((const MtxFx43 *)m, (const MtxFx43 *)(mgr + 0x1e0), (MtxFx43 *)m);
        *(s32 *)(mgr + 0x2d0) = m[9];
        *(s32 *)(mgr + 0x2d4) = m[10];
        *(s32 *)(mgr + 0x2d8) = m[11];
        y.x = m[3];
        y.y = m[4];
        y.z = m[5];
        func_02067e34((s32 *)(mgr + 0x3a8), (s32 *)(mgr + 0x2d0), &x.x);
        switch (*(s8 *)(mgr + 0x95)) {
        case 0:
            PushOut(mgr, 0x1a4, &x, 0);
            break;
        case 4:
            PushOut(mgr, 0x1a4, &x, 1);
            break;
        case 5:
            PushOut(mgr, 0x18c, &x, 0);
            PushOut(mgr, 0x1a4, &x, 1);
            break;
        case 6:
            /* opposite side: while d.n > -lim, d -= (d.n + lim) * n */
            dot = (s32)func_02067df0((s32 *)(mgr + 0x18c), &x.x);
            lim = (s32)func_02028130(mgr);
            if (dot > (s32)(0u - (u32)lim))
                func_01ff832c((fx32)(0u - ((u32)dot + (u32)lim)), (const VecFx32 *)(mgr + 0x18c), &x, &x);
            PushOut(mgr, 0x1a4, &x, 1);
            break;
        case 1:
            if (func_02009eb8((s32 *)(mgr + 0x18c), &x.x) != 0)
                func_01ff832c((fx32)(func_02028130(mgr) << 3), (const VecFx32 *)(mgr + 0x18c), &x, &x);
            break;
        case 2:
            if (func_02009eb8((s32 *)(mgr + 0x18c), &x.x) == 0)
                func_01ff832c((fx32)(0u - (func_02028130(mgr) << 3)), (const VecFx32 *)(mgr + 0x18c), &x, &x);
            break;
        default:
            break;
        }
        func_01ff89d0(&x);
        func_02067d04(&x.x, &y.x, (u32 *)&z.x);
        func_01ff89d0(&z);
        func_02067d04(&z.x, &x.x, (u32 *)&y.x);
        m[0] = x.x;
        m[1] = x.y;
        m[2] = x.z;
        m[3] = y.x;
        m[4] = y.y;
        m[5] = y.z;
        m[6] = z.x;
        m[7] = z.y;
        m[8] = z.z;
        func_01ff810c((const MtxFx43 *)m, (const MtxFx43 *)(mgr + 0x210), (MtxFx43 *)m);
        break;
    }
    w = *(s32 *)(mgr + 0x450);
    func_01ff838c((const s32 *)(mgr + 0x4cc), m, saved, m, MulRound(w, 0x802), MulRound(w, 0x7fe));
    m[9] = saved[9];
    m[10] = saved[10];
    m[11] = saved[11];
    func_0206c8d4((u32 *)m, (u32 *)(mgr + 0x4cc));
    if (*(s8 *)(mgr + 0x95) == 0) {
        func_01ff810c((const MtxFx43 *)m, (const MtxFx43 *)(mgr + 0x1e0), &world);
        *(s32 *)(mgr + 0x2d0) = world._30;
        *(s32 *)(mgr + 0x2d4) = world._31;
        *(s32 *)(mgr + 0x2d8) = world._32;
        *(s32 *)(mgr + 0x7d8) = world._00;
        *(s32 *)(mgr + 0x7dc) = world._01;
        *(s32 *)(mgr + 0x7e0) = world._02;
    } else {
        func_01ff8894(m, (const s32 *)(mgr + 0x1e0), (s32 *)(mgr + 0x2d0));
    }
}
