#include "ctx.h"
#include "nitro.h"
/* MTX_MultVec43: dst = v * M (3x3 part, 64-bit sums truncated >> 12) + M translation; v may alias dst. */

static u64 mul64(s32 a, s32 b) { return (u64)((s64)a * (s64)b); }
static fx32 hi12(u64 v) { return (fx32)(u32)(v >> 12); }

void func_01ff8058(const VecFx32 *v, const MtxFx43 *m, VecFx32 *dst)
{
    fx32 x = v->x;
    fx32 y = v->y;
    fx32 z = v->z;

    /* each component is stored, then re-read and the translation added, as the original does */
    dst->x = hi12(mul64(y, m->_10) + mul64(x, m->_00) + mul64(z, m->_20));
    dst->x = (fx32)((u32)dst->x + (u32)m->_30);
    dst->y = hi12(mul64(y, m->_11) + mul64(x, m->_01) + mul64(z, m->_21));
    dst->y = (fx32)((u32)dst->y + (u32)m->_31);
    dst->z = hi12(mul64(y, m->_12) + mul64(x, m->_02) + mul64(z, m->_22));
    dst->z = (fx32)((u32)dst->z + (u32)m->_32);
}
