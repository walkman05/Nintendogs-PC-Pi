#include "ctx.h"

#ifdef NDOGS_PC
/* host: hardware registers through HostIo* (no trap) */
u32 func_0207ec14(u32 param_1)
{
    u16 saved = HostIoRead16(0x04000208);
    u32 old;
    HostIoWrite16(0x04000208, 0);
    old = HostIoRead32(0x04000210);
    HostIoWrite32(0x04000210, old | param_1);
    HostIoWrite16(0x04000208, saved);
    hw_irq_poll();
    return old;
}
#else
u32 func_0207ec14(u32 param_1)
{
    u16 old;
    u32 prev;
    old = *(vu16 *)0x04000208;
    *(vu16 *)0x04000208 = 0;
    prev = *(vu32 *)0x04000210;
    *(vu32 *)0x04000210 = prev | param_1;
    *(vu16 *)0x04000208;
    *(vu16 *)0x04000208 = old;
    return prev;
}
#endif
