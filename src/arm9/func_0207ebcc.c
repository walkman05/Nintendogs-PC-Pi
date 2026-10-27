#include "ctx.h"

/* r0 still holds the IE value at bx lr in the target: try returning the old IE value. */
#ifdef NDOGS_PC
/* host: hardware registers through HostIo* (no trap) */
u32 func_0207ebcc(u32 param_1)
{
    u16 saved = HostIoRead16(0x04000208);
    u32 old;
    HostIoWrite16(0x04000208, 0);
    old = HostIoRead32(0x04000210);
    HostIoWrite32(0x04000210, old & ~param_1);
    HostIoWrite16(0x04000208, saved);
    hw_irq_poll();
    return old;
}
#else
u32 func_0207ebcc(u32 param_1)
{
    vu16 *ime = (vu16 *)0x04000208;
    vu32 *ie = (vu32 *)0x04000210;
    u16 old;
    u32 v;
    old = *ime;
    *ime = 0;
    v = *ie;
    *ie = v & ~param_1;
    (void)*ime;
    *ime = old;
    return v;
}
#endif
