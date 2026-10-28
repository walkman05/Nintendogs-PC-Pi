#pragma avoid_byte strb
#include "ctx.h"

#ifdef NDOGS_PC /* NDOGS_PC: return type of func_020382d8 as defined (x86 extends by the caller's prototype, the DS by the callee) */
extern u32 func_020382d8(u8 *p);
#else
extern u16 func_020382d8(u8 *p);
#endif
extern void func_02026994(u8 *p);
extern u8 func_02025bb0(u8 *p);

u8 func_0201d918(u8 *p)
{
    *(u16 *)(p + 256 + 82) = func_020382d8(p);
    p[147] = 5;
    func_02026994(p);
    return func_02025bb0(p);
}
