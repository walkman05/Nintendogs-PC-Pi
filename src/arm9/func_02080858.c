#include "ctx.h"

extern volatile u64 data_02155fdc;
extern u32 func_02080b08(void);
extern void func_02080b1c(u32 e);

#ifdef NDOGS_PC
/* host: hardware registers through HostIo* (no trap) */
u64 func_02080858(void)
{
    vu16 lo;
    volatile u64 hi;
    u32 prev = func_02080b08();

    lo = HostIoRead16(0x04000100);
    hi = data_02155fdc & 0x0000ffffffffffffULL;
    if ((HostIoRead32(0x04000214) & 8) && !(lo & 0x8000)) {
        hi++;
    }
    func_02080b1c(prev);
    return (hi << 16) | lo;
}
#else
u64 func_02080858(void)
{
    vu16 lo;
    volatile u64 hi;
    u32 prev = func_02080b08();

    lo = *(vu16 *)0x04000100;
    hi = data_02155fdc & 0x0000ffffffffffffULL;
    if ((*(vu32 *)0x04000214 & 8) && !(lo & 0x8000)) {
        hi++;
    }
    func_02080b1c(prev);
    return (hi << 16) | lo;
}
#endif
