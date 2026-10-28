#include "ctx.h"
extern u8 data_021594ec[];
extern u8 data_021594e8[];
extern u8 data_021594d8[];
#ifdef NDOGS_PC /* NDOGS_PC: return type of func_020647d0 as defined (x86 extends by the caller's prototype, the DS by the callee) */
extern u16 func_020647d0(void);
#else
extern s32 func_020647d0(void);
#endif
u32 func_020d68ec(u8 *param_1)
{
    s32 flag;
    if (*(u16 *)(param_1 + 2) == 0) {
        *(u32 *)data_021594ec = 0;
        *(u32 *)data_021594e8 = 3;
        flag = 1;
    } else {
        *(u32 *)data_021594ec = 0;
        flag = 0;
    }
    if (flag != 0) {
        if (*(u16 *)data_021594d8 == 0) {
            *(s16 *)data_021594d8 = (s16)func_020647d0();
        }
        return 1;
    }
    return 0;
}
