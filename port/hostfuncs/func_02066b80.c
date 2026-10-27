#include "ctx.h"
/* MTX_Identity43: 4x3 fx32 identity. Returns m + 12 (r0 is left past the matrix; one caller returns it). */

u32 *func_02066b80(u32 *m)
{
    m[0] = 0x1000; m[1] = 0;       m[2] = 0;
    m[3] = 0;      m[4] = 0x1000;  m[5] = 0;
    m[6] = 0;      m[7] = 0;       m[8] = 0x1000;
    m[9] = 0;      m[10] = 0;      m[11] = 0;
    return m + 12;
}
