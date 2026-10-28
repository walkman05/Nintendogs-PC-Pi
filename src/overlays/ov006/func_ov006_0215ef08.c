#include "ctx.h"

#ifdef NDOGS_PC /* NDOGS_PC: return type of func_ov006_021611a0 as defined (x86 extends by the caller's prototype, the DS by the callee) */
extern u32 func_ov006_021611a0(int p);
#else
extern s16 func_ov006_021611a0(int p);
#endif
extern void func_ov006_0215fb74(int p, int a, s16 b, int c, int d, int e);

void func_ov006_0215ef08(int param_1)
{
#ifdef NDOGS_PC /* NDOGS_PC: widened: holds func_ov006_021611a0's value unchanged, as the DS register does */
    u32 sVar1;
#else
    s16 sVar1;
#endif

    sVar1 = func_ov006_021611a0(param_1);
    func_ov006_0215fb74(param_1, 0, sVar1, 0, 1, 1);
}
