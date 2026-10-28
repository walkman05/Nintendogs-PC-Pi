#include "ctx.h"

extern u32 data_021594ec;
extern u16 data_021594dc;
extern u32 data_02159620;
extern u32 data_021594e8;
extern u32 func_02080b08(void);
extern void func_02080b1c(u32 r);
#ifdef NDOGS_PC /* NDOGS_PC: return type of func_02064590 as defined (x86 extends by the caller's prototype, the DS by the callee) */
extern s32 func_02064590(u32 a, u32 b);
#else
extern u16 func_02064590(u32 a, u32 b);
#endif

u32 func_020d6bb8(u32 param_1, u16 *param_2)
{
    u16 sVar1;
    u32 r4;

    r4 = func_02080b08();
    if (data_021594ec == 1 || data_021594dc == 1) {
        *param_2 = 0x17;
        return 0;
    }
    sVar1 = func_02064590((u32)&data_02159620, param_1);
    *param_2 = sVar1;
    if (*param_2 == 0) {
        data_021594e8 = 1;
        func_02080b1c(r4);
        return 1;
    }
    func_02080b1c(r4);
    return 0;
}
