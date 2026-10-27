#include "ctx.h"
#include "nitro.h"
/* Per-side node callback (side 0/1): when mgr+0x8f+side is 3, aim the node (in world space, node * mgr+0x1e0) along
 * the vector mgr+0x3c0[side] - mgr+0x3d8[side] keeping its row 2 as the up hint, then map back with mgr+0x210;
 * blend with mgr+0x5ec[side] when the weight at mgr+0x438[side] is below 1.0, restore the translation, store the
 * node at mgr+0x5ec[side], and unless the breed flag (data_0213c42d) is set, transform (len, 0, 0) by
 * node * mgr+0x1e0 into mgr+0x2f4[side]. */

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

void func_01ff9428(void *unused, s32 *m, s32 side)
{
    u8 *mgr = data_027c0360;
    VecFx32 x;
    VecFx32 y;
    VecFx32 z;
    VecFx32 ofs;
    MtxFx43 world;
    s32 tx;
    s32 ty;
    s32 tz;
    s32 t;
    u32 rec;

    (void)unused;
    tz = m[11];
    ty = m[10];
    tx = m[9];
    if (*(s8 *)(mgr + side + 0x8f) == 3) {
        func_01ff810c((const MtxFx43 *)m, (const MtxFx43 *)(mgr + 0x1e0), (MtxFx43 *)m);
        func_02067e34((s32 *)(mgr + 0x3c0 + side * 12), (s32 *)(mgr + 0x3d8 + side * 12), &x.x);
        z.x = m[6];
        z.y = m[7];
        z.z = m[8];
        func_01ff89d0(&x);
        func_02067d04(&z.x, &x.x, (u32 *)&y.x);
        func_01ff89d0(&y);
        func_02067d04(&x.x, &y.x, (u32 *)&z.x);
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
    }
    t = *(s32 *)(mgr + side * 4 + 0x438);
    if (t < 0x1000)
        func_01ff86bc((const s32 *)(mgr + 0x5ec + side * 0x30), m, m, t);
    m[9] = tx;
    m[10] = ty;
    m[11] = tz;
    func_0206c8d4((u32 *)m, (u32 *)(mgr + 0x5ec + side * 0x30));
    rec = (u32)**(u16 **)(mgr + 0x9e8) * 100;
    if (*(s8 *)(data_0213c42d + rec) != 0)
        return;
    ofs.x = *(s32 *)(data_0213c424 + rec);
    ofs.y = 0;
    ofs.z = 0;
    func_01ff810c((const MtxFx43 *)m, (const MtxFx43 *)(mgr + 0x1e0), &world);
    func_01ff8058(&ofs, &world, (VecFx32 *)(mgr + 0x2f4 + side * 12));
}
