#include "ctx.h"
#include "nitro.h"
/* MTX_RotX43: 4x3 rotation about X from sin/cos, zero translation. */

void func_01ff8000(MtxFx43 *m, fx32 sinVal, fx32 cosVal)
{
    m->_00 = FX32_ONE; m->_01 = 0;       m->_02 = 0;
    m->_10 = 0;        m->_11 = cosVal;  m->_12 = sinVal;
    m->_20 = 0;        m->_21 = (fx32)(0u - (u32)sinVal); m->_22 = cosVal;
    m->_30 = 0;        m->_31 = 0;       m->_32 = 0;
}
