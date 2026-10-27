#include "ctx.h"
#include "nitro.h"
/* Per-side node callback (side 0/1). In world space (node * mgr+0x1e0), by mode mgr+0x8f+side:
 *   1: aim row 0 from the node toward target T = mgr+0x3c0[side];  2: aim row 0 from T toward the node;
 *   3: solve the joint J = mgr+0x3d8[side] with func_0200babc (node position, T, both lengths = breed length *
 *      func_02028130(mgr), bend hint row 1), then aim row 0 from J toward the node;
 * keeping row 2 as the up hint, then map back with mgr+0x210. Then blend with mgr+0x58c[side] when the weight at
 * mgr+0x438[side] is below 1.0, restore the translation, store the node at mgr+0x58c[side], and for breed mode
 * (data_0213c42d) 1 / 2-3 transform (len, 0, 0) / (-len, len, 0) by node * mgr+0x1e0 into mgr+0x2f4[side]. */

extern u8 *data_027c0360;
extern u8 data_0213c42d[]; /* per-breed records of 100 bytes */
extern u8 data_0213c424[];
extern void func_01ff810c(const MtxFx43 *a, const MtxFx43 *b, MtxFx43 *ab);  /* MTX_Concat43 */
extern void func_02067e34(s32 *a, s32 *b, s32 *out);                          /* VEC_Subtract: out = a - b */
extern BOOL func_01ff89d0(VecFx32 *v);                                        /* guarded VEC_Normalize */
extern void func_02067d04(s32 *a, s32 *b, u32 *out);                          /* VEC_CrossProduct */
extern void func_01ff86bc(const s32 *a, const s32 *b, s32 *out, s32 t);
extern u64 func_0206c8d4(u32 *src, u32 *dst);
extern void func_01ff8058(const VecFx32 *v, const MtxFx43 *m, VecFx32 *dst); /* MTX_MultVec43 */
extern u32 func_02028130(u8 *p);
extern void func_0200babc(VecFx32 *out, fx32 ax, fx32 ay, fx32 az, fx32 bx, fx32 by, fx32 bz,
                          fx32 lenA, fx32 lenB, fx32 nx, fx32 ny, fx32 nz);

static u32 BreedRec(u8 *mgr) { return (u32)**(u16 **)(mgr + 0x9e8) * 100; }

/* FX_Mul with rounding: low 32 bits of (a * b + 0x800) >> 12 */
static s32 MulRound(s32 a, s32 b) { return (s32)(u32)(((u64)((s64)a * b) + 0x800u) >> 12); }

/* rows = x (normalized), y = normalize(row2 x x), z = x cross y; then node = node * mgr+0x210 */
static void Aim(u8 *mgr, s32 *m, VecFx32 *x)
{
    VecFx32 y;
    VecFx32 z;

    z.x = m[6];
    z.y = m[7];
    z.z = m[8];
    func_01ff89d0(x);
    func_02067d04(&z.x, &x->x, (u32 *)&y.x);
    func_01ff89d0(&y);
    func_02067d04(&x->x, &y.x, (u32 *)&z.x);
    m[0] = x->x;
    m[1] = x->y;
    m[2] = x->z;
    m[3] = y.x;
    m[4] = y.y;
    m[5] = y.z;
    m[6] = z.x;
    m[7] = z.y;
    m[8] = z.z;
    func_01ff810c((const MtxFx43 *)m, (const MtxFx43 *)(mgr + 0x210), (MtxFx43 *)m);
}

void func_01ff9600(void *unused, s32 *m, s32 side)
{
    u8 *mgr = data_027c0360;
    VecFx32 pos;
    VecFx32 dir;
    VecFx32 target;
    VecFx32 ofs;
    MtxFx43 world;
    s32 tx;
    s32 ty;
    s32 tz;
    s32 t;
    s32 lenB;
    s32 lenA;
    s32 n0;
    s32 n1;
    s32 n2;
    u32 rec;
    s8 mode;

    (void)unused;
    tz = m[11];
    ty = m[10];
    tx = m[9];
    mode = *(s8 *)(mgr + side + 0x8f);
    if (mode == 1 || mode == 2 || mode == 3) {
        func_01ff810c((const MtxFx43 *)m, (const MtxFx43 *)(mgr + 0x1e0), (MtxFx43 *)m);
        pos.x = m[9];
        pos.y = m[10];
        pos.z = m[11];
        if (mode == 1) {
            func_02067e34((s32 *)(mgr + 0x3c0 + side * 12), &pos.x, &dir.x);
        } else if (mode == 2) {
            func_02067e34(&pos.x, (s32 *)(mgr + 0x3c0 + side * 12), &dir.x);
        } else {
            n0 = m[3];
            n1 = m[4];
            n2 = m[5];
            lenB = MulRound(*(s32 *)(data_0213c424 + BreedRec(mgr)), (s32)func_02028130(mgr));
            t = (s32)func_02028130(mgr);
            target = *(VecFx32 *)(mgr + 0x3c0 + side * 12);
            lenA = MulRound(*(s32 *)(data_0213c424 + BreedRec(mgr)), t);
            func_0200babc((VecFx32 *)(mgr + 0x3d8 + side * 12), pos.x, pos.y, pos.z, target.x, target.y, target.z,
                          lenA, lenB, n0, n1, n2);
            func_02067e34((s32 *)(mgr + 0x3d8 + side * 12), &pos.x, &dir.x);
        }
        Aim(mgr, m, &dir);
    }
    t = *(s32 *)(mgr + side * 4 + 0x438);
    if (t < 0x1000)
        func_01ff86bc((const s32 *)(mgr + 0x58c + side * 0x30), m, m, t);
    m[9] = tx;
    m[10] = ty;
    m[11] = tz;
    func_0206c8d4((u32 *)m, (u32 *)(mgr + 0x58c + side * 0x30));
    rec = BreedRec(mgr);
    mode = *(s8 *)(data_0213c42d + rec);
    if (mode == 1) {
        ofs.x = *(s32 *)(data_0213c424 + rec);
        ofs.y = 0;
        ofs.z = 0;
    } else if (mode == 2 || mode == 3) {
        ofs.x = (s32)(0u - *(u32 *)(data_0213c424 + rec));
        ofs.y = *(s32 *)(data_0213c424 + BreedRec(mgr));
        ofs.z = 0;
    } else {
        return;
    }
    func_01ff810c((const MtxFx43 *)m, (const MtxFx43 *)(mgr + 0x1e0), &world);
    func_01ff8058(&ofs, &world, (VecFx32 *)(mgr + 0x2f4 + side * 12));
}
