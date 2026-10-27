#include "ctx.h"
/* MTX_Identity33: 3x3 fx32 identity (1.0 = 0x1000). */

void func_020663d0(s32 *m)
{
    m[0] = 0x1000; m[1] = 0;      m[2] = 0;
    m[3] = 0;      m[4] = 0x1000; m[5] = 0;
    m[6] = 0;      m[7] = 0;      m[8] = 0x1000;
}
