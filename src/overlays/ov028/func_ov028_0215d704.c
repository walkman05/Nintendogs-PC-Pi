#include "ctx.h"
extern u8 *data_ov028_0215df90;
#ifdef NDOGS_PC /* NDOGS_PC: return type of func_020d6c84 as defined (x86 extends by the caller's prototype, the DS by the callee) */
extern u32 func_020d6c84(void);
#else
extern s16 func_020d6c84(void);
#endif
u32 func_ov028_0215d704(void)
{
    u8 *p = data_ov028_0215df90;
    if (*(s32 *)(p + 0x1c) == 2) {
        if (func_020d6c84() != 0) {
            return 1;
        }
    }
    return 0;
}
