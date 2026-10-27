#include "ctx.h"
#include "nitro.h"
/* MTX_RotY43: 4x3 rotation about Y from sin/cos, zero translation. */

void func_01ff8020(MtxFx43 *m, fx32 sinVal, fx32 cosVal)
{
    m->_00 = cosVal;   m->_01 = 0;        m->_02 = (fx32)(0u - (u32)sinVal);
    m->_10 = 0;        m->_11 = FX32_ONE; m->_12 = 0;
    m->_20 = sinVal;   m->_21 = 0;        m->_22 = cosVal;
    m->_30 = 0;        m->_31 = 0;        m->_32 = 0;
}
