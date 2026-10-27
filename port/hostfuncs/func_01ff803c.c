#include "ctx.h"
#include "nitro.h"
/* MTX_RotZ43: 4x3 rotation about Z from sin/cos, zero translation. */

void func_01ff803c(MtxFx43 *m, fx32 sinVal, fx32 cosVal)
{
    m->_00 = cosVal;   m->_01 = sinVal;   m->_02 = 0;
    m->_10 = (fx32)(0u - (u32)sinVal); m->_11 = cosVal; m->_12 = 0;
    m->_20 = 0;        m->_21 = 0;        m->_22 = FX32_ONE;
    m->_30 = 0;        m->_31 = 0;        m->_32 = 0;
}
