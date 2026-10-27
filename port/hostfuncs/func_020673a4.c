#include "ctx.h"
/* MTX_Identity44: 4x4 fx32 identity. */

void func_020673a4(u32 *m)
{
    int i;
    for (i = 0; i < 16; i++) {
        m[i] = (i % 5 == 0) ? 0x1000 : 0;
    }
}
