#include "ctx.h"
/* Zeroes nine words (a 3x3 fx32 matrix, MTX_Zero33-style). */

void func_0206cae8(u32 *m)
{
    int i;
    for (i = 0; i < 9; i++) {
        m[i] = 0;
    }
}
