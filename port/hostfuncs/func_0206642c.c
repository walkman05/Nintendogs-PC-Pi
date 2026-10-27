#include "ctx.h"
/* MTX_RotZ33: rotation about Z from sin/cos (fx32). */

void func_0206642c(s32 *m, s32 sinVal, s32 cosVal)
{
    m[0] = cosVal;  m[1] = sinVal; m[2] = 0;
    m[3] = -sinVal; m[4] = cosVal; m[5] = 0;
    m[6] = 0;       m[7] = 0;      m[8] = 0x1000;
}
