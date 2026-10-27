#include "ctx.h"
/* MTX_Scale43: 4x3 fx32 scale matrix (writes all 12 words, translation zero). */

void func_02066c08(s32 *m, s32 x, s32 y, s32 z)
{
    m[0] = x; m[1] = 0;  m[2] = 0;
    m[3] = 0; m[4] = y;  m[5] = 0;
    m[6] = 0; m[7] = 0;  m[8] = z;
    m[9] = 0; m[10] = 0; m[11] = 0;
}
