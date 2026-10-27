#include "ctx.h"
#include "nitro.h"
/* Node callback: rotate the node's 3x3 part about its row 1 by the angle at mgr+0x818 (translation kept), copy the
 * node to mgr+0x55c, then with world = node * mgr+0x1e0 store world's translation at mgr+0x33c and the
 * data_0213b3f4 offset transformed by world at mgr+0x348. */

extern u8 *data_027c0360;
extern const s16 data_02126868[];      /* FX_SinCosTable_: sin, cos pairs */
extern const VecFx32 data_0213b3f4;
extern void func_0206695c(MtxFx33 *pDst, const VecFx32 *vec, fx32 sinVal, fx32 cosVal); /* MTX_RotAxis33 */
extern void func_020664c8(const MtxFx33 *a, const MtxFx33 *b, MtxFx33 *ab);             /* MTX_Concat33 */
extern u64 func_0206c8d4(u32 *src, u32 *dst);
extern void func_01ff810c(const MtxFx43 *a, const MtxFx43 *b, MtxFx43 *ab);             /* MTX_Concat43 */
extern void func_01ff8058(const VecFx32 *v, const MtxFx43 *m, VecFx32 *dst);            /* MTX_MultVec43 */

void func_01ff92c0(void *unused, s32 *m)
{
    u8 *mgr = data_027c0360;
    VecFx32 axis;
    VecFx32 ofs;
    MtxFx33 rot;
    MtxFx43 world;
    s32 tx;
    s32 ty;
    s32 tz;
    u32 a;

    (void)unused;
    tz = m[11];
    ty = m[10];
    tx = m[9];
    axis.x = m[3];
    axis.y = m[4];
    axis.z = m[5];
    a = (*(u32 *)(mgr + 0x818) & 0xffff) >> 4;
    func_0206695c(&rot, &axis, data_02126868[a * 2], data_02126868[a * 2 + 1]);
    func_020664c8((const MtxFx33 *)m, &rot, (MtxFx33 *)m);
    m[9] = tx;
    m[10] = ty;
    m[11] = tz;
    func_0206c8d4((u32 *)m, (u32 *)(mgr + 0x55c));
    ofs = data_0213b3f4;
    func_01ff810c((const MtxFx43 *)m, (const MtxFx43 *)(mgr + 0x1e0), &world);
    *(s32 *)(mgr + 0x33c) = world._30;
    *(s32 *)(mgr + 0x340) = world._31;
    *(s32 *)(mgr + 0x344) = world._32;
    func_01ff8058(&ofs, &world, (VecFx32 *)(mgr + 0x348));
}
