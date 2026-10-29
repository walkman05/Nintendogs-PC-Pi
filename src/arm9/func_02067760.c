#include "ctx.h"

#ifdef NDOGS_PC
/* host: hardware registers through HostIo* (no trap) */
u32 func_02067760(void)
{
    while (HostIoRead16(0x040002b0) & 0x8000) {
    }
    return (HostIoRead32(0x040002b4) + 0x200) >> 10;
}
#else
u32 func_02067760(void)
{
    while (*(vu16 *)0x040002b0 & 0x8000) {
    }
    return (*(vu32 *)0x040002b4 + 0x200) >> 10;
}
#endif
