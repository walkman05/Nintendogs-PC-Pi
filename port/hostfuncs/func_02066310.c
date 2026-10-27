#include "ctx.h"
/* MTX_Rot22: m = { cos, sin, -sin, cos } (fx32). */

void func_02066310(s32 *m, s32 sinVal, s32 cosVal)
{
    m[0] = cosVal;
    m[1] = sinVal;
    m[2] = -sinVal;
    m[3] = cosVal;
}
