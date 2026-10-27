#include "ctx.h"
#include "nitro.h"
/* Joint result blend: interpolates the rotation rows 0 and 1 and the translation from the 4x3 matrix mtx towards
 * the joint result src by w (fx32), re-orthonormalizes the rotation (row2 = row0 x row1, normalizing row0 and row2
 * when their squared length reaches data_02126728, row1 = row2 x row0) and stores rotation (+0x28) and
 * translation (+0x4c) into the joint result dst. */

extern u32 data_02126728[2];        /* s64 squared-length threshold, low word first */
extern void func_02067e34(const VecFx32 *a, const VecFx32 *b, VecFx32 *ab);   /* VEC_Subtract: ab = a - b */
extern void func_02067d04(const VecFx32 *a, const VecFx32 *b, VecFx32 *axb);  /* VEC_CrossProduct */
extern fx32 func_01ff832c(fx32 a, const VecFx32 *v1, const VecFx32 *v2, VecFx32 *dst); /* VEC_MultAdd */
extern s32 func_02006a30(s32 *param_1, s32 *param_2);                          /* VEC_Normalize */

static s64 LenSq(const VecFx32 *v)
{
    /* 64-bit smull/smlal sum; accumulated unsigned so the wrap is defined. */
    return (s64)((u64)((s64)v->y * v->y) + (u64)((s64)v->x * v->x) + (u64)((s64)v->z * v->z));
}

static void LoadVec(VecFx32 *v, const u8 *p)
{
    v->x = *(const s32 *)p;
    v->y = *(const s32 *)(p + 4);
    v->z = *(const s32 *)(p + 8);
}

static void StoreVec(u8 *p, const VecFx32 *v)
{
    *(s32 *)p = v->x;
    *(s32 *)(p + 4) = v->y;
    *(s32 *)(p + 8) = v->z;
}

void func_01ffe7b4(const s32 *mtx, const u8 *src, u8 *dst, s32 w)
{
    VecFx32 a, b, d, row0, row1, row2, tr;
    s64 thr = (s64)(((u64)data_02126728[1] << 32) | data_02126728[0]);

    LoadVec(&a, (const u8 *)mtx);
    LoadVec(&b, src + 0x28);
    func_02067e34(&b, &a, &d);
    func_01ff832c(w, &d, &a, &row0);
    LoadVec(&a, (const u8 *)mtx + 0xc);
    LoadVec(&b, src + 0x34);
    func_02067e34(&b, &a, &d);
    func_01ff832c(w, &d, &a, &row1);
    func_02067d04(&row0, &row1, &row2);
    if (LenSq(&row0) >= thr) {
        func_02006a30(&row0.x, &row0.x);
    }
    if (LenSq(&row2) >= thr) {
        func_02006a30(&row2.x, &row2.x);
    }
    func_02067d04(&row2, &row0, &row1);
    LoadVec(&a, (const u8 *)mtx + 0x24);
    LoadVec(&b, src + 0x4c);
    func_02067e34(&b, &a, &d);
    func_01ff832c(w, &d, &a, &tr);
    StoreVec(dst + 0x28, &row0);
    StoreVec(dst + 0x34, &row1);
    StoreVec(dst + 0x40, &row2);
    StoreVec(dst + 0x4c, &tr);
}
