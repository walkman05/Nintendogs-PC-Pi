#include "ctx.h"

extern u32 data_021592b4;
extern u32 data_02159074;
#ifdef NDOGS_PC /* NDOGS_PC: return type of func_020a366c as defined (x86 extends by the caller's prototype, the DS by the callee) */
u32 func_020a366c(void);
#else
u8 func_020a366c(void);
#endif

void func_020a0958(int param_1)
{
    u8 *iVar1;
#ifdef NDOGS_PC /* NDOGS_PC: widened: holds func_020a366c's value unchanged, as the DS register does */
    u32 cVar2;
#else
    u8 cVar2;
#endif

    cVar2 = func_020a366c();
    if (cVar2 == 0xff) {
        return;
    }
    iVar1 = (u8 *)&data_021592b4;
    *(u16 *)(iVar1 + 4) = 0;
    if (param_1 == 0x18) {
        *(u32 *)(iVar1 + 0x14) = 1;
        return;
    }
    if (param_1 == 0xd && *(u32 *)((u8 *)&data_02159074 + 0x40) == 0) {
        return;
    }
    *(u32 *)(iVar1 + 0x14) = 0;
}
