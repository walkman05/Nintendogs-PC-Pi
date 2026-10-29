#include "ctx.h"

extern s32 func_02067760(void);

#ifdef NDOGS_PC
/* host: hardware registers through HostIo* (no trap) */
s32 func_0206782c(s32 param_1)
{
    if (param_1 > 0) {
        HostIoWrite16(0x040002b0, 1);
        HostIoWrite32(0x040002b8, 0);
        HostIoWrite32(0x040002bc, (u32)param_1);
        return func_02067760();
    }
    return 0;
}
#else
s32 func_0206782c(s32 param_1)
{
    vu32 *p;
    if (param_1 > 0) {
        *(vu16 *)0x040002b0 = 1;
        p = (vu32 *)0x040002b8;
        p[0] = 0;
        p[1] = (u32)param_1;
        return func_02067760();
    }
    return 0;
}
#endif
