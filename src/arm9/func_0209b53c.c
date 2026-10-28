#include "ctx.h"
#ifdef NDOGS_PC /* NDOGS_PC: return type of func_02072d28 as defined (x86 extends by the caller's prototype, the DS by the callee) */
extern BOOL func_02072d28(u32 a, u32 *b);
#else
extern u8 func_02072d28(u32 a, u32 *b);
#endif
u32 func_0209b53c(u32 p0, u32 *p1)
{
    if (p0 != 0xff) {
        return func_02072d28(p0, p1);
    }
    return p0;
}
