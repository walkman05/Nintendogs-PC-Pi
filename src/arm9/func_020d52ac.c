#include "ctx.h"
extern u8 *data_021594d0;
#ifdef NDOGS_PC /* NDOGS_PC: return type of func_020d6c84 as defined (x86 extends by the caller's prototype, the DS by the callee) */
extern u32 func_020d6c84(void);
#else
extern s16 func_020d6c84(void);
#endif
u32 func_020d52ac(void)
{
    s32 v = *(s32 *)(data_021594d0 + 0x20);
    if (v == 2) {
        if (func_020d6c84() != 0) {
            return 1;
        }
    } else if (v == 3) {
        return 1;
    }
    return 0;
}
