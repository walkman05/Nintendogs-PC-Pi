#include "ctx.h"

extern u32 func_020e71ec(u32 p);
#ifdef NDOGS_PC /* NDOGS_PC: return type of func_020c5fbc as defined (x86 extends by the caller's prototype, the DS by the callee) */
extern BOOL func_020c5fbc(u32 p);
#else
extern u8 func_020c5fbc(u32 p);
#endif

u32 func_020e71a8(u32 param_1)
{
    u8 bVar1;

    if (*(s16 *)(param_1 + 0x90) != 1) {
        if (*(s16 *)(param_1 + 0x90) != 2) {
            return param_1;
        }
        bVar1 = func_020c5fbc(param_1);
        return (u32)bVar1;
    }
    return func_020e71ec(param_1);
}
