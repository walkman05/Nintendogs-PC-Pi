#pragma avoid_byte strb
#include "ctx.h"

#ifdef NDOGS_PC /* NDOGS_PC: return type of func_0203833c as defined (x86 extends by the caller's prototype, the DS by the callee) */
extern s16 func_0203833c(void);
#else
extern u16 func_0203833c(void);
#endif
extern void func_02025dd4(u8 *p);
extern u8 func_020254ec(u8 *p);

u8 func_0205885c(u8 *p)
{
    u16 h;
    h = func_0203833c();
    *(u16 *)(p + 256 + 82) = h;
    *(u8 *)(p + 147) = 5;
    func_02025dd4(p);
    return func_020254ec(p);
}
