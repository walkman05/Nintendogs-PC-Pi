#include "ctx.h"
#include "nitro.h"
/* Node callback: unless state mgr+0x93 is 0, 1, 14-17, 23 or 24, aim the node in world space (node * mgr+0x1e0,
 * whose translation is stored at mgr+0x2dc) along mgr+0x3b4 - position, first pushing that direction d out of the
 * half-spaces given by mgr+0x95 (d += (lim - d.n) * n while d.n < lim, for the normals n at mgr+0x18c, 0x1a4,
 * 0x7cc, 0x7d8, 0x7e4 and limits scaled from func_02028130(mgr)), build the basis with func_02067e68(mgr+0x198, ..)
 * and map back with mgr+0x210. Then roll about row 0 by the angle at mgr+0x12e, blend with mgr+0x4fc when the
 * weight at mgr+0x454 is below 1.0, restore the translation, store the node at mgr+0x4fc, and with world = node *
 * mgr+0x1e0 store its translation at mgr+0x2dc and three transformed offsets at mgr+0x2e8 (per-breed,
 * data_0213c464), mgr+0x36c (data_0213b3e8) and mgr+0x378 (data_0213b3d0). */

extern u8 *data_027c0360;
extern const s16 data_02126868[]; /* FX_SinCosTable_: sin, cos pairs */
extern const VecFx32 data_0213b3e8;
extern const VecFx32 data_0213b3d0;
extern u8 data_0213c464[];        /* per-breed records of 100 bytes */
extern void func_01ff810c(const MtxFx43 *a, const MtxFx43 *b, MtxFx43 *ab);  /* MTX_Concat43 */
extern void func_02067e34(s32 *a, s32 *b, s32 *out);                          /* VEC_Subtract: out = a - b */
extern u32 func_02067df0(s32 *a, s32 *b);                                     /* VEC_DotProduct */
extern void func_02067e68(s32 *a, s32 *b, s32 *out);
extern u32 func_02028130(u8 *p);
extern fx32 func_01ff832c(fx32 a, const VecFx32 *v1, const VecFx32 *v2, VecFx32 *dst); /* VEC_MultAdd */
extern BOOL func_01ff89d0(VecFx32 *v);                                        /* guarded VEC_Normalize */
extern void func_02067d04(s32 *a, s32 *b, u32 *out);                          /* VEC_CrossProduct */
extern void func_0206695c(MtxFx33 *pDst, const VecFx32 *vec, fx32 sinVal, fx32 cosVal); /* MTX_RotAxis33 */
extern void func_020664c8(const MtxFx33 *a, const MtxFx33 *b, MtxFx33 *ab);             /* MTX_Concat33 */
extern void func_01ff86bc(const s32 *a, const s32 *b, s32 *out, s32 t);
extern u64 func_0206c8d4(u32 *src, u32 *dst);
extern void func_01ff8058(const VecFx32 *v, const MtxFx43 *m, VecFx32 *dst); /* MTX_MultVec43 */

enum { LIM_1, LIM_SHR4, LIM_SHL2 };

/* d.n is taken first, then the limit from func_02028130(mgr), in the original's call order */
static void PushOut(u8 *mgr, u32 off, VecFx32 *d, int kind)
{
    s32 dot = (s32)func_02067df0((s32 *)(mgr + off), &d->x);
    s32 lim = (s32)func_02028130(mgr);

    if (kind == LIM_SHR4)
        lim >>= 4; /* asr */
    else if (kind == LIM_SHL2)
        lim = (s32)((u32)lim << 2);
    if (dot < lim)
        func_01ff832c((fx32)((u32)lim - (u32)dot), (const VecFx32 *)(mgr + off), d, d);
}

void func_01ff9b1c(void *unused, s32 *m)
{
    u8 *mgr = data_027c0360;
    VecFx32 x;
    VecFx32 y;
    VecFx32 z;
    VecFx32 axis;
    VecFx32 o0;
    VecFx32 o1;
    VecFx32 o2;
    MtxFx33 rot;
    MtxFx43 world;
    s32 tx;
    s32 ty;
    s32 tz;
    s32 t;
    s32 dot;
    s32 lim;
    u32 a;

    (void)unused;
    tz = m[11];
    ty = m[10];
    tx = m[9];
    switch (*(s8 *)(mgr + 0x93)) {
    case 0:
    case 1:
    case 14:
    case 15:
    case 16:
    case 17:
    case 23:
    case 24:
        break;
    default:
        func_01ff810c((const MtxFx43 *)m, (const MtxFx43 *)(mgr + 0x1e0), (MtxFx43 *)m);
        *(s32 *)(mgr + 0x2dc) = m[9];
        *(s32 *)(mgr + 0x2e0) = m[10];
        *(s32 *)(mgr + 0x2e4) = m[11];
        func_02067e34((s32 *)(mgr + 0x3b4), (s32 *)(mgr + 0x2dc), &x.x);
        switch (*(s8 *)(mgr + 0x95)) {
        case 0:
            PushOut(mgr, 0x1a4, &x, LIM_1);
            PushOut(mgr, 0x7d8, &x, LIM_SHR4);
            PushOut(mgr, 0x7cc, &x, LIM_1);
            break;
        case 1:
        case 2:
            dot = (s32)func_02067df0((s32 *)(mgr + 0x7e4), &x.x);
            if (*(s8 *)(mgr + 0x93) == 6)
                lim = (s32)func_02028130(mgr);
            else
                lim = (s32)(func_02028130(mgr) * 20u);
            if (dot < lim)
                func_01ff832c((fx32)((u32)lim - (u32)dot), (const VecFx32 *)(mgr + 0x7e4), &x, &x);
            break;
        case 4:
            PushOut(mgr, 0x1a4, &x, LIM_SHL2);
            PushOut(mgr, 0x7cc, &x, LIM_SHL2);
            break;
        case 5:
            PushOut(mgr, 0x18c, &x, LIM_1);
            PushOut(mgr, 0x1a4, &x, LIM_SHL2);
            PushOut(mgr, 0x7cc, &x, LIM_SHL2);
            break;
        case 6:
            /* opposite side: while d.n > -lim, d -= (d.n + lim) * n */
            dot = (s32)func_02067df0((s32 *)(mgr + 0x18c), &x.x);
            lim = (s32)func_02028130(mgr);
            if (dot > (s32)(0u - (u32)lim))
                func_01ff832c((fx32)(0u - ((u32)dot + (u32)lim)), (const VecFx32 *)(mgr + 0x18c), &x, &x);
            PushOut(mgr, 0x1a4, &x, LIM_SHL2);
            PushOut(mgr, 0x7cc, &x, LIM_SHL2);
            break;
        default:
            break;
        }
        func_01ff89d0(&x);
        func_02067e68((s32 *)(mgr + 0x198), &x.x, &y.x);
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
    a = *(u16 *)(mgr + 0x12e);
    if (a != 0) {
        axis.x = m[0];
        axis.y = m[1];
        axis.z = m[2];
        a >>= 4;
        func_0206695c(&rot, &axis, data_02126868[a * 2], data_02126868[a * 2 + 1]);
        func_020664c8((const MtxFx33 *)m, &rot, (MtxFx33 *)m);
    }
    t = *(s32 *)(mgr + 0x454);
    if (t < 0x1000)
        func_01ff86bc((const s32 *)(mgr + 0x4fc), m, m, t);
    m[9] = tx;
    m[10] = ty;
    m[11] = tz;
    func_0206c8d4((u32 *)m, (u32 *)(mgr + 0x4fc));
    o1 = data_0213b3e8;
    o2 = data_0213b3d0;
    o0 = *(const VecFx32 *)(data_0213c464 + (u32)**(u16 **)(mgr + 0x9e8) * 100);
    func_01ff810c((const MtxFx43 *)m, (const MtxFx43 *)(mgr + 0x1e0), &world);
    *(s32 *)(mgr + 0x2dc) = world._30;
    *(s32 *)(mgr + 0x2e0) = world._31;
    *(s32 *)(mgr + 0x2e4) = world._32;
    func_01ff8058(&o0, &world, (VecFx32 *)(mgr + 0x2e8));
    func_01ff8058(&o1, &world, (VecFx32 *)(mgr + 0x36c));
    func_01ff8058(&o2, &world, (VecFx32 *)(mgr + 0x378));
}
