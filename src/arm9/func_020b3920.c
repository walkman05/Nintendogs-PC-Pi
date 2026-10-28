#include "ctx.h"

#ifdef NDOGS_PC /* NDOGS_PC: return type of func_020ad77c as defined (x86 extends by the caller's prototype, the DS by the callee) */
extern s32 func_020ad77c(u8 *p);
#else
extern u8 func_020ad77c(u8 *p);
#endif
#ifdef NDOGS_PC /* NDOGS_PC: return type of func_020ad690 as defined (x86 extends by the caller's prototype, the DS by the callee) */
extern u32 func_020ad690(u8 *p);
#else
extern u8 func_020ad690(u8 *p);
#endif
extern u32 func_020ad8f0(u8 *p);

u32 func_020b3920(u8 *param_1)
{
#ifdef NDOGS_PC /* NDOGS_PC: widened: holds func_020ad690's value unchanged, as the DS register does */
    u32 t;
#else
    u8 t;
#endif
    switch (*(s16 *)(param_1 + 0x2e)) {
    case 0:
        return func_020ad77c(param_1);
    case 1:
        break;
    case 2:
        break;
    case 3:
        break;
    case 4:
        t = func_020ad690(param_1);
        if (t == 0) {
            return t;
        }
        return func_020ad8f0(param_1);
    }
}
