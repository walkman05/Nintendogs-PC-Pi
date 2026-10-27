#include "ctx.h"
#include "nitro.h"
/* Per-node matrix callback (head joint of the dog in data_027c0360): unless the dog's state (s8 +0x93) is one of
 * 0, 1, 10-13, 19, 20, 23, 24, turns the node matrix towards the look target (+0x39c) and applies the per-mode
 * limits (s8 +0x95) against the axes at +0x18c/+0x1a4, re-orthonormalizes it and applies +0x210; then blends it
 * with the previous frame's matrix (+0x49c) and the original by the weight at +0x450, stores it back into +0x49c,
 * derives the head axes/positions at +0x7f0..+0x804, +0x2c4 and +0x384, and offsets a point along one axis into
 * +0x7cc or +0x7e4 depending on the mode. mtx is the node's 4x3 matrix (data_027c0370). */

extern u8 *data_027c0360;
extern VecFx32 data_0213b3dc;

extern u64 func_0206c8d4(u32 *src, u32 *dst);                                        /* 48-byte copy */
extern void func_01ff810c(const MtxFx43 *a, const MtxFx43 *b, MtxFx43 *ab);          /* MTX_Concat43 */
extern fx32 func_01ff832c(fx32 a, const VecFx32 *v1, const VecFx32 *v2, VecFx32 *dst); /* VEC_MultAdd */
extern void func_01ff838c(const s32 *a, const s32 *b, const s32 *c, s32 *out, s32 wb, s32 wc);
extern void func_01ff8058(const VecFx32 *v, const MtxFx43 *m, VecFx32 *dst);         /* MTX_MultVec43 */
extern BOOL func_01ff89d0(VecFx32 *v);                                                /* VEC_Normalize */
extern void func_02067e34(const VecFx32 *a, const VecFx32 *b, VecFx32 *ab);           /* VEC_Subtract */
extern void func_02067d04(const VecFx32 *a, const VecFx32 *b, VecFx32 *axb);          /* VEC_CrossProduct */
extern fx32 func_02067df0(const VecFx32 *a, const VecFx32 *b);                        /* VEC_DotProduct */
extern s32 func_02028130(u8 *p);
extern BOOL func_02009eb8(s32 *a, s32 *b);

#define V(o) ((VecFx32 *)(dog + (o)))
#define E(mp, i, j) (((s32 *)(mp))[(i) * 3 + (j)])
#define NEG(x) ((s32)(0u - (u32)(x)))

/* Keeps row0 at least t along axis (d < t -> row0 += (t - d) * axis). */
static void PushMin(u8 *dog, u32 axisOfs, s32 t, VecFx32 *row0)
{
    fx32 d = func_02067df0(V(axisOfs), row0);

    if (d < t) {
        func_01ff832c((s32)((u32)t - (u32)d), V(axisOfs), row0, row0);
    }
}

static s32 RoundMul(s32 w, s32 k)
{
    return (s32)(u32)(((u64)((s64)w * k) + 0x800u) >> 12);
}

void func_01ffa590(u8 *rs, MtxFx43 *mtx)
{
    u8 *dog = data_027c0360;
    MtxFx43 orig;
    VecFx32 row[3];
    VecFx32 d;
    VecFx32 ofs;
    MtxFx43 m;
    VecFx32 a, b;
    s8 state;
    s32 t;

    (void)rs;
    func_0206c8d4((u32 *)mtx, (u32 *)&orig);
    state = *(s8 *)(dog + 0x93);
    switch (state) {
    case 0: case 1: case 10: case 11: case 12: case 13: case 19: case 20: case 23: case 24:
        break;
    default:
        func_01ff810c(mtx, (const MtxFx43 *)(dog + 0x1e0), mtx);
        V(0x2c4)->x = E(mtx, 3, 0);
        V(0x2c4)->y = E(mtx, 3, 1);
        V(0x2c4)->z = E(mtx, 3, 2);
        row[0].x = E(mtx, 0, 0);
        row[0].y = E(mtx, 0, 1);
        row[0].z = E(mtx, 0, 2);
        row[1].x = E(mtx, 1, 0);
        row[1].y = E(mtx, 1, 1);
        row[1].z = E(mtx, 1, 2);
        func_02067e34(V(0x39c), V(0x2c4), &d);
        func_01ff832c(0x400, &d, &row[0], &row[0]);
        switch (*(s8 *)(dog + 0x95)) {
        case 0:
            PushMin(dog, 0x1a4, func_02028130(dog), &row[0]);
            break;
        case 4:
            PushMin(dog, 0x1a4, (s32)((u32)func_02028130(dog) << 2), &row[0]);
            break;
        case 5:
            PushMin(dog, 0x18c, func_02028130(dog), &row[0]);
            PushMin(dog, 0x1a4, (s32)((u32)func_02028130(dog) << 2), &row[0]);
            break;
        case 6: {
            fx32 dot = func_02067df0(V(0x18c), &row[0]);

            t = func_02028130(dog);
            if (dot > NEG(t)) {
                func_01ff832c(NEG((u32)dot + (u32)t), V(0x18c), &row[0], &row[0]);
            }
            PushMin(dog, 0x1a4, (s32)((u32)func_02028130(dog) << 2), &row[0]);
            break;
        }
        case 1:
            if (func_02009eb8((s32 *)V(0x18c), &row[0].x)) {
                t = (s32)((u32)func_02028130(dog) << 3);
                func_01ff832c(t, V(0x18c), &row[0], &row[0]);
            }
            break;
        case 2:
            if (!func_02009eb8((s32 *)V(0x18c), &row[0].x)) {
                t = (s32)((u32)func_02028130(dog) << 3);
                func_01ff832c(NEG(t), V(0x18c), &row[0], &row[0]);
            }
            break;
        default:
            break;
        }
        func_01ff89d0(&row[0]);
        func_02067d04(&row[0], &row[1], &row[2]);
        func_01ff89d0(&row[2]);
        func_02067d04(&row[2], &row[0], &row[1]);
        {
            int i;

            for (i = 0; i < 3; i++) {
                E(mtx, i, 0) = row[i].x;
                E(mtx, i, 1) = row[i].y;
                E(mtx, i, 2) = row[i].z;
            }
        }
        func_01ff810c(mtx, (const MtxFx43 *)(dog + 0x210), mtx);
        break;
    }

    {
        s32 w = *(s32 *)(dog + 0x450);

        func_01ff838c((const s32 *)(dog + 0x49c), (const s32 *)mtx, (const s32 *)&orig, (s32 *)mtx,
                      RoundMul(w, 0x334), RoundMul(w, 0xccc));
    }
    E(mtx, 3, 0) = E(&orig, 3, 0);
    E(mtx, 3, 1) = E(&orig, 3, 1);
    E(mtx, 3, 2) = E(&orig, 3, 2);
    func_0206c8d4((u32 *)mtx, (u32 *)(dog + 0x49c));

    ofs = data_0213b3dc;
    func_01ff810c(mtx, (const MtxFx43 *)(dog + 0x1e0), &m);
    V(0x7f0)->x = NEG(E(&m, 1, 0));
    V(0x7f0)->y = NEG(E(&m, 1, 1));
    V(0x7f0)->z = NEG(E(&m, 1, 2));
    V(0x7fc)->x = NEG(E(&m, 2, 0));
    V(0x7fc)->y = NEG(E(&m, 2, 1));
    V(0x7fc)->z = NEG(E(&m, 2, 2));
    V(0x2c4)->x = E(&m, 3, 0);
    V(0x2c4)->y = E(&m, 3, 1);
    V(0x2c4)->z = E(&m, 3, 2);
    func_01ff8058(&ofs, &m, V(0x384));

    a.x = E(&m, 0, 0);
    a.y = E(&m, 0, 1);
    a.z = E(&m, 0, 2);
    switch (*(s8 *)(dog + 0x95)) {
    case 0: case 4: case 5: case 6:
        b.x = E(&m, 1, 0);
        b.y = E(&m, 1, 1);
        b.z = E(&m, 1, 2);
        func_01ff832c((fx32)0xfffff7fe, &b, &a, V(0x7cc));
        break;
    case 1:
    case 2:
        b.x = E(&m, 2, 0);
        b.y = E(&m, 2, 1);
        b.z = E(&m, 2, 2);
        func_01ff832c(*(s8 *)(dog + 0x95) == 1 ? (fx32)0xffffdff8 : 0x2008, &b, &a, V(0x7e4));
        break;
    default:
        break;
    }
}
