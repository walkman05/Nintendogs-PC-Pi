#include "ctx.h"
#include "nitro.h"
/* Node callback (side 0/1) for the dog in data_027c0360: unless the side state (s8 +0x91+side) is 0, 8 or 9,
 * turns row0 a quarter of the way towards the target +0x3f0+side*12 and, except in states 6/7, keeps it within
 * limits against the head axes +0x7f0 and +0x7fc (scaled by the size factor; side 1 mirrors the second limit),
 * then rebuilds the basis around +0x7fc between the transforms at +0x1e0 and +0x210; finally blends with the
 * previous matrix at +0x64c+side*0x30 by the weight at +0x448+side*4 and stores it there. */

extern u8 *data_027c0360;

extern u64 func_0206c8d4(u32 *src, u32 *dst);                                        /* 48-byte copy */
extern void func_01ff810c(const MtxFx43 *a, const MtxFx43 *b, MtxFx43 *ab);          /* MTX_Concat43 */
extern void func_01ff86bc(const s32 *a, const s32 *b, s32 *out, s32 t);              /* rotation blend */
extern fx32 func_01ff832c(fx32 a, const VecFx32 *v1, const VecFx32 *v2, VecFx32 *dst); /* VEC_MultAdd */
extern BOOL func_01ff89d0(VecFx32 *v);                                                /* guarded VEC_Normalize */
extern void func_02067e34(const VecFx32 *a, const VecFx32 *b, VecFx32 *ab);           /* VEC_Subtract */
extern void func_02067d04(const VecFx32 *a, const VecFx32 *b, VecFx32 *axb);          /* VEC_CrossProduct */
extern fx32 func_02067df0(const VecFx32 *a, const VecFx32 *b);                        /* VEC_DotProduct */
extern s32 func_02028130(u8 *p);

#define V(o) ((VecFx32 *)(dog + (o)))
#define ROW(m, i) ((VecFx32 *)((s32 *)(m) + (i) * 3))
#define NEG(x) ((s32)(0u - (u32)(x)))
#define SUB(a, b) ((s32)((u32)(a) - (u32)(b)))

static s32 RoundMul(s32 a, s32 b)
{
    return (s32)(u32)(((u64)((s64)a * b) + 0x800u) >> 12);
}

void func_01ffb044(void *unused, s32 *obj, s32 side)
{
    u8 *dog = data_027c0360;
    s8 *stp = (s8 *)(dog + 0x91) + side;
    s32 tz = obj[11], ty = obj[10], tx = obj[9];
    s8 st = *stp;
    u8 *prev;
    s32 w;

    (void)unused;
    if (st != 0 && st != 8 && st != 9) {
        VecFx32 org, d, r0, r1, r2;
        fx32 dot, t;

        func_01ff810c((const MtxFx43 *)obj, (const MtxFx43 *)(dog + 0x1e0), (MtxFx43 *)obj);
        org = *ROW(obj, 3);
        r0 = *ROW(obj, 0);
        func_02067e34(V(0x3f0 + side * 12), &org, &d);
        func_01ff832c(0x400, &d, &r0, &r0);
        st = *stp;
        if (st != 6 && st != 7) {
            dot = func_02067df0(V(0x7f0), &r0);
            t = RoundMul(func_02028130(dog), 0x1806);
            if (dot < t) {
                func_01ff832c(SUB(t, dot), V(0x7f0), &r0, &r0);
            }
            dot = func_02067df0(V(0x7fc), &r0);
            t = RoundMul(func_02028130(dog), 0x19a);
            if (side == 0) {
                if (dot < t) {
                    func_01ff832c(SUB(t, dot), V(0x7fc), &r0, &r0);
                }
            } else if (dot > NEG(t)) {
                func_01ff832c(NEG((u32)dot + (u32)t), V(0x7fc), &r0, &r0);
            }
        }
        func_01ff89d0(&r0);
        func_02067d04(V(0x7fc), &r0, &r1);
        func_01ff89d0(&r1);
        func_02067d04(&r0, &r1, &r2);
        *ROW(obj, 0) = r0;
        *ROW(obj, 1) = r1;
        *ROW(obj, 2) = r2;
        func_01ff810c((const MtxFx43 *)obj, (const MtxFx43 *)(dog + 0x210), (MtxFx43 *)obj);
    }
    w = *(s32 *)(dog + 0x448 + side * 4);
    prev = dog + 0x64c + side * 0x30;
    if (w < 0x1000) {
        func_01ff86bc((const s32 *)prev, obj, obj, w);
    }
    obj[9] = tx;
    obj[10] = ty;
    obj[11] = tz;
    func_0206c8d4((u32 *)obj, (u32 *)prev);
}
