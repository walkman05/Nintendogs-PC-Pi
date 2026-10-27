#include "ctx.h"
#include "nitro.h"
/* Node callback (side 0/1, a leg joint) for the dog in data_027c0360: unless the side state (s8 +0x91+side) is
 * 0, 8 or 9, solves the two-bone joint position +0x408+side*12 from the node origin to the target +0x3f0+side*12
 * (bone lengths = size factor * per-type tables data_0213b598/data_0213b59c, bend hint = -row1) and aims row0 from
 * the node towards it (row2 as up hint), between the transforms at +0x1e0 and +0x210; then blends with the previous
 * matrix at +0x6ac+side*0x30 by the weight at +0x440+side*4 and stores it there. obj is the node's 4x3 matrix. */

extern u8 *data_027c0360;
extern s32 data_0213b598[];   /* per dog type, stride 8 bytes */
extern s32 data_0213b59c[];

extern u64 func_0206c8d4(u32 *src, u32 *dst);                                        /* 48-byte copy */
extern void func_01ff810c(const MtxFx43 *a, const MtxFx43 *b, MtxFx43 *ab);          /* MTX_Concat43 */
extern void func_01ff86bc(const s32 *a, const s32 *b, s32 *out, s32 t);              /* rotation blend */
extern BOOL func_01ff89d0(VecFx32 *v);                                                /* guarded VEC_Normalize */
extern void func_02067e34(const VecFx32 *a, const VecFx32 *b, VecFx32 *ab);           /* VEC_Subtract */
extern void func_02067d04(const VecFx32 *a, const VecFx32 *b, VecFx32 *axb);          /* VEC_CrossProduct */
extern s32 func_02028130(u8 *p);
extern s32 func_02027468(u8 *p);
extern void func_0200babc(VecFx32 *out, fx32 ax, fx32 ay, fx32 az, fx32 bx, fx32 by, fx32 bz,
                          fx32 lenA, fx32 lenB, fx32 nx, fx32 ny, fx32 nz);

#define V(o) ((VecFx32 *)(dog + (o)))
#define ROW(m, i) ((VecFx32 *)((s32 *)(m) + (i) * 3))
#define NEG(x) ((s32)(0u - (u32)(x)))

static s32 RoundMul(s32 a, s32 b)
{
    return (s32)(u32)(((u64)((s64)a * b) + 0x800u) >> 12);
}

void func_01ffada4(void *unused, s32 *obj, s32 side)
{
    u8 *dog = data_027c0360;
    s32 tz = obj[11], ty = obj[10], tx = obj[9];
    s8 st = *(s8 *)(dog + side + 0x91);
    u8 *prev;
    s32 w;

    (void)unused;
    if (st != 0 && st != 8 && st != 9) {
        VecFx32 org, r0, r1, r2;
        const VecFx32 *tgt;
        s32 f1, k1, f0, k0;

        func_01ff810c((const MtxFx43 *)obj, (const MtxFx43 *)(dog + 0x1e0), (MtxFx43 *)obj);
        org = *ROW(obj, 3);
        r1 = *ROW(obj, 1);
        r2 = *ROW(obj, 2);
        f1 = func_02028130(dog);
        k1 = data_0213b59c[func_02027468(dog) * 2];
        f0 = func_02028130(dog);
        k0 = data_0213b598[func_02027468(dog) * 2];
        tgt = V(0x3f0 + side * 12);
        func_0200babc(V(0x408 + side * 12), org.x, org.y, org.z, tgt->x, tgt->y, tgt->z,
                      RoundMul(k0, f0), RoundMul(k1, f1), NEG(r1.x), NEG(r1.y), NEG(r1.z));
        func_02067e34(V(0x408 + side * 12), &org, &r0);
        func_01ff89d0(&r0);
        func_02067d04(&r2, &r0, &r1);
        func_01ff89d0(&r1);
        func_02067d04(&r0, &r1, &r2);
        *ROW(obj, 0) = r0;
        *ROW(obj, 1) = r1;
        *ROW(obj, 2) = r2;
        func_01ff810c((const MtxFx43 *)obj, (const MtxFx43 *)(dog + 0x210), (MtxFx43 *)obj);
    }
    w = *(s32 *)(dog + 0x440 + side * 4);
    prev = dog + 0x6ac + side * 0x30;
    if (w < 0x1000) {
        func_01ff86bc((const s32 *)prev, obj, obj, w);
    }
    obj[9] = tx;
    obj[10] = ty;
    obj[11] = tz;
    func_0206c8d4((u32 *)obj, (u32 *)prev);
}
