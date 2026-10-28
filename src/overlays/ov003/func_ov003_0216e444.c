#pragma avoid_byte strb
#include "ctx.h"

#ifdef NDOGS_PC /* NDOGS_PC: return type of func_0203833c as defined (x86 extends by the caller's prototype, the DS by the callee) */
extern s16 func_0203833c(void);
#else
extern u16 func_0203833c(void);
#endif
extern void func_02026bac(u8 *p);
extern u32 func_02025548(u8 *p);

u32 func_ov003_0216e444(u8 *p)
{
#ifdef NDOGS_PC /* NDOGS_PC: widened: holds func_0203833c's value unchanged, as the DS register does */
    s32 h;
#else
    u16 h;
#endif
    h = func_0203833c();
    *(u16 *)((u32)((u8 *)p + 256) + 82) = h;
    *(u8 *)(p + 147) = 5;
    func_02026bac(p);
    return func_02025548(p);
}
