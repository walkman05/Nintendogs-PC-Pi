#include "ctx.h"

#ifdef NDOGS_PC
/* host: hardware registers through HostIo* (no trap) */
u32 func_020677c8(void)
{
    u64 v;
    while ((HostIoRead16(0x04000280) & 0x8000) != 0) {
    }
    v = HostIoRead64(0x040002a0);
    return (u32)((v + 0x80000) >> 20);
}
#else
u32 func_020677c8(void)
{
    vu16 *poll = (vu16 *)0x04000280;
    u64 *pv = (u64 *)0x040002a0;
    u64 v;

    while ((*poll & 0x8000) != 0) {
    }
    v = *pv;
    return (u32)((v + 0x80000) >> 20);
}
#endif
