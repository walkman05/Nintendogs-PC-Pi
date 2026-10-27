#include "ctx.h"
/* MTX_Copy43To44: 4x3 fx32 matrix to 4x4, adding the column { 0, 0, 0, 1.0 }. */

void func_02066ba8(const u32 *src, u32 *dst)
{
    int row;
    for (row = 0; row < 4; row++) {
        dst[row * 4 + 0] = src[row * 3 + 0];
        dst[row * 4 + 1] = src[row * 3 + 1];
        dst[row * 4 + 2] = src[row * 3 + 2];
        dst[row * 4 + 3] = (row == 3) ? 0x1000 : 0;
    }
}
