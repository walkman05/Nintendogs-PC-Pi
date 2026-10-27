#include "ctx.h"
#include "nitro.h"
/* Node callback (side 0/1) for the dog in data_027c0360: unless the side state (s8 +0x91+side) is 0, 8 or 9,
 * re-aims the node so row0 points along +0x3f0+side*12 - +0x408+side*12 (row2 as the up hint), between the
 * transforms at +0x1e0 and +0x210; then blends with the previous matrix at +0x70c+side*0x30 by the weight at
 * +0x440+side*4 and stores it there. obj is the node's 4x3 matrix. */

extern u8 *data_027c0360;

extern u64 func_0206c8d4(u32 *src, u32 *dst);                                        /* 48-byte copy */
extern void func_01ff810c(const MtxFx43 *a, const MtxFx43 *b, MtxFx43 *ab);          /* MTX_Concat43 */
extern void func_01ff86bc(const s32 *a, const s32 *b, s32 *out, s32 t);              /* rotation blend */
extern BOOL func_01ff89d0(VecFx32 *v);                                                /* guarded VEC_Normalize */
extern void func_02067e34(const VecFx32 *a, const VecFx32 *b, VecFx32 *ab);           /* VEC_Subtract */
extern void func_02067d04(const VecFx32 *a, const VecFx32 *b, VecFx32 *axb);          /* VEC_CrossProduct */

#define V(o) ((VecFx32 *)(dog + (o)))
#define ROW(m, i) ((VecFx32 *)((s32 *)(m) + (i) * 3))

void func_01ffac04(void *unused, s32 *obj, s32 side)
{
    u8 *dog = data_027c0360;
    s32 tz = obj[11], ty = obj[10], tx = obj[9];
    s8 st = *(s8 *)(dog + side + 0x91);
    u8 *prev;
    s32 w;

    (void)unused;
    if (st != 0 && st != 8 && st != 9) {
        VecFx32 r0, r1, r2;

        func_01ff810c((const MtxFx43 *)obj, (const MtxFx43 *)(dog + 0x1e0), (MtxFx43 *)obj);
        r2 = *ROW(obj, 2);
        func_02067e34(V(0x3f0 + side * 12), V(0x408 + side * 12), &r0);
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
    prev = dog + 0x70c + side * 0x30;
    if (w < 0x1000) {
        func_01ff86bc((const s32 *)prev, obj, obj, w);
    }
    obj[9] = tx;
    obj[10] = ty;
    obj[11] = tz;
    func_0206c8d4((u32 *)obj, (u32 *)prev);
}
