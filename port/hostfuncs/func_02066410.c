#include "ctx.h"
/* MTX_RotY33: rotation about Y from sin/cos (fx32). */

void func_02066410(s32 *m, s32 sinVal, s32 cosVal)
{
    m[0] = cosVal; m[1] = 0;      m[2] = -sinVal;
    m[3] = 0;      m[4] = 0x1000; m[5] = 0;
    m[6] = sinVal; m[7] = 0;      m[8] = cosVal;
}
