#include "ctx.h"
extern u32 data_02132fa0[];
#ifdef NDOGS_PC /* NDOGS_PC: return type of func_02028118 as defined (x86 extends by the caller's prototype, the DS by the callee) */
extern u32 func_02028118(u8 *p);
#else
extern u16 func_02028118(u8 *p);
#endif
u32 func_020ec148(u8 *param_1)
{
    u16 uVar1 = func_02028118(param_1);
    return data_02132fa0[uVar1];
}
