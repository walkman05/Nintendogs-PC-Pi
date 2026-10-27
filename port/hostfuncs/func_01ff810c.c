#include "ctx.h"
#include "nitro.h"
/* MTX_Concat43: ab = a * b for 4x3 matrices (64-bit sums truncated >> 12); ab may alias a or b. */

static u64 mul64(s32 a, s32 b) { return (u64)((s64)a * (s64)b); }
static fx32 hi12(u64 v) { return (fx32)(u32)(v >> 12); }

void func_01ff810c(const MtxFx43 *a, const MtxFx43 *b, MtxFx43 *ab)
{
    /* The original builds the result in place unless ab == b (then on the stack); it reads each row of a before
       writing that row, so computing into a temporary gives the same result for ab == a, ab == b or neither. */
    const fx32 *pa = &a->_00;
    const fx32 *pb = &b->_00;
    fx32 r[12];
    int i;

    for (i = 0; i < 4; i++) {
        fx32 a0 = pa[i * 3 + 0];
        fx32 a1 = pa[i * 3 + 1];
        fx32 a2 = pa[i * 3 + 2];
        r[i * 3 + 0] = hi12(mul64(a0, pb[0]) + mul64(a1, pb[3]) + mul64(a2, pb[6]));
        r[i * 3 + 1] = hi12(mul64(a0, pb[1]) + mul64(a1, pb[4]) + mul64(a2, pb[7]));
        r[i * 3 + 2] = hi12(mul64(a0, pb[2]) + mul64(a1, pb[5]) + mul64(a2, pb[8]));
    }
    r[9] = (fx32)((u32)r[9] + (u32)pb[9]);
    r[10] = (fx32)((u32)r[10] + (u32)pb[10]);
    r[11] = (fx32)((u32)r[11] + (u32)pb[11]);
    for (i = 0; i < 12; i++)
        (&ab->_00)[i] = r[i];
}
