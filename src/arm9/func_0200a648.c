#include "ctx.h"
#include "nitro.h"

/* NON-MATCHING: equivalent C. With the original's registers forced it compiles to the exact original bytes;
 * the natural build differs only in register allocation and scheduling. */


extern void func_02067e68(const VecFx32 *a, const VecFx32 *b, VecFx32 *ab);
extern BOOL func_0200b280(VecFx32 *prev, VecFx32 *c, fx32 cs, fx32 sn, fx32 w, fx32 d);
extern s16 data_02126868[];

#define FX_Mul(a, b) ((fx32)(((fx64)(fx32)(a) * (fx32)(b) + 0x800) >> 12))
#define FX_Mul64(a, b) ((fx32)(((a) * (b) + 0x800) >> 12))

static inline void Push(VecFx32 *o, fx64 a, fx64 b, fx32 s)
{
    o->x += FX_Mul64(a, s);
    o->z += FX_Mul64(b, s);
}

BOOL func_0200a648(VecFx32 *pos, VecFx32 *prev, fx32 r, VecFx32 *out, VecFx32 *c, u16 ang, fx32 h, fx32 w, fx32 d)
{
    fx32 z10;
    BOOL ret;
    fx32 sn;
    fx32 s1;
    fx32 s2;
    fx32 z9;
    fx64 S;
    fx64 C;
    fx32 v;
    fx32 e;
    fx32 s3;
    fx64 N;
    fx32 u;
    fx32 nc;
    fx32 hd;
    fx64 cs;
    fx32 s4;
    fx32 f;
    fx32 x8;
    VecFx32 p;
    fx32 hw;
    fx32 x7;
    ret = 0;
    func_02067e68(pos, out, &p);
    if (p.y - r - c->y > h) {
        return 0;
    }
    sn = data_02126868[(ang >> 4) * 2 + 1];
    cs = data_02126868[(ang >> 4) * 2];
    if (!func_0200b280(prev, c, cs, sn, w, d)) {
        return 0;
    }
    hw = (w >> 1) - r;
    hd = (d >> 1) - r;
    nc = -(fx32)cs;
    N = nc;
    u = FX_Mul(sn, hw);
    v = FX_Mul(cs, hd);
    e = FX_Mul(nc, hw);
    f = FX_Mul(sn, hd);
    z9 = c->z + e;
    z9 = z9 + f - p.z;
    S = sn;
    x7 = c->x + u;
    x7 = x7 + v - p.x;
    C = (fx64)(fx32)cs;
    z10 = c->z - e - f - p.z;
    x8 = c->x - u - v - p.x;
    s1 = FX_Mul(sn, x7) + FX_Mul(nc, z9);
    s2 = FX_Mul(cs, x7) + FX_Mul(sn, z9);
    v = x8;
    s3 = FX_Mul(sn, v) + FX_Mul(nc, z10);
    s4 = FX_Mul(cs, v) + FX_Mul(sn, z10);
    if (s1 < 0) { ret = 1; Push(out, S, N, s1); }
    if (s2 < 0) { Push(out, C, S, s2); ret = 1; }
    if (s3 > 0) { Push(out, S, N, s3); ret = 1; }
    if (s4 > 0) { Push(out, C, S, s4); ret = 1; }
    return ret;
}
