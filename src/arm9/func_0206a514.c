#include "ctx.h"

extern void func_0206a384(void);

#ifdef NDOGS_PC
/* host: hardware registers through HostIo* (no trap) */
void func_0206a514(void)
{
    while ((HostIoRead32(0x04000600) & 0x08000000) != 0) {
    }
    HostIoWrite32(0x04000600, HostIoRead32(0x04000600) | 0x8000);
    HostIoWrite16(0x04000060, HostIoRead16(0x04000060) | 0x2000);
    HostIoWrite16(0x04000060, HostIoRead16(0x04000060) | 0x1000);
    func_0206a384();
    HostIoWrite32(0x040004a4, 0x001f0080);
    HostIoWrite32(0x040004a8, 0);
    HostIoWrite32(0x040004ac, 0);
}
#else
void func_0206a514(void)
{
    while ((*(vu32 *)0x04000600 & 0x08000000) != 0) {
    }
    *(vu32 *)0x04000600 = *(vu32 *)0x04000600 | 0x8000;
    *(vu16 *)0x04000060 = *(vu16 *)0x04000060 | 0x2000;
    *(vu16 *)0x04000060 = *(vu16 *)0x04000060 | 0x1000;
    func_0206a384();
    *(vu32 *)0x040004a4 = 0x001f0080;
    *(vu32 *)0x040004a8 = 0;
    *(vu32 *)0x040004ac = 0;
}
#endif
