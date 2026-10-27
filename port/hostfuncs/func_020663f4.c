#include "ctx.h"
/* MTX_RotX33: rotation about X from sin/cos (fx32). */

void func_020663f4(s32 *m, s32 sinVal, s32 cosVal)
{
    m[0] = 0x1000; m[1] = 0;       m[2] = 0;
    m[3] = 0;      m[4] = cosVal;  m[5] = sinVal;
    m[6] = 0;      m[7] = -sinVal; m[8] = cosVal;
}
