#include "ctx.h"
#include "nitro.h"
/* Node rotation by two angles: m's 3x3 part is post-multiplied by a rotation about its own row 1 by the angle at
 * mgr+0x80c+side*4, then about its row 2 (both rows taken before either product) by the angle at mgr+0x808,
 * halved (as a signed 16-bit value) when side == 0. */

extern const s16 data_02126868[]; /* FX_SinCosTable_: sin, cos pairs */
extern void func_0206695c(MtxFx33 *pDst, const VecFx32 *vec, fx32 sinVal, fx32 cosVal); /* MTX_RotAxis33 */
extern void func_020664c8(const MtxFx33 *a, const MtxFx33 *b, MtxFx33 *ab);             /* MTX_Concat33 */

void func_01ff9184(u8 *mgr, s32 *m, s32 side)
{
    VecFx32 row1;
    VecFx32 row2;
    MtxFx33 rot1;
    MtxFx33 rot2;
    u32 a;
    u32 b;

    row1.x = m[3];
    row1.y = m[4];
    row1.z = m[5];
    row2.x = m[6];
    row2.y = m[7];
    row2.z = m[8];
    a = *(u32 *)(mgr + 0x808) & 0xffff;
    b = *(u32 *)(mgr + side * 4 + 0x80c) & 0xffff;
    if (side == 0)
        a = (a >> 1) | (a & 0x8000); /* (s32)(a << 16) >> 1, then >> 16 (logical) */
    a >>= 4;
    b >>= 4;
    func_0206695c(&rot1, &row1, data_02126868[a * 2], data_02126868[a * 2 + 1]);
    func_0206695c(&rot2, &row2, data_02126868[b * 2], data_02126868[b * 2 + 1]);
    func_020664c8((const MtxFx33 *)m, &rot2, (MtxFx33 *)m);
    func_020664c8((const MtxFx33 *)m, &rot1, (MtxFx33 *)m);
}
