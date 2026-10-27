#include "ctx.h"

#ifdef NDOGS_PC
/* host: hardware registers through HostIo* (no trap) */
u32 func_0207eb98(u32 param_1)
{
    u16 saved = HostIoRead16(0x04000208);
    u32 old;
    HostIoWrite16(0x04000208, 0);
    old = HostIoRead32(0x04000214);
    HostIoWrite32(0x04000214, param_1);
    HostIoWrite16(0x04000208, saved);
    hw_irq_poll();
    return old;
}
#else
u32 func_0207eb98(u32 param_1)
{
    vu16 *ime = (vu16 *)0x04000208;
    vu32 *ie = (vu32 *)0x04000214;
    u16 saved;
    u32 old;
    saved = *ime;
    *ime = 0;
    old = *ie;
    *ie = param_1;
    (void)*ime;
    *ime = saved;
    return old;
}
#endif
