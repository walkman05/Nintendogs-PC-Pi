#include "ctx.h"

extern u32 func_02080b08(void);
extern void func_02080b1c(u32 irq);

typedef union {
    u32 w;
    struct { u32 lo : 5; u32 b : 1; u32 hi : 26; } f;
} Reg;

#ifdef NDOGS_PC
/* host: hardware registers through HostIo* (no trap) */
s32 func_0206cdb8(u32 param_1, s32 param_2, u32 param_3)
{
    Reg r;
    u32 irq;

    r.f.lo = param_1;
    r.f.b = param_3;
    r.f.hi = param_2;
    if (HostIoRead16(0x04000184) & 0x4000) {
        HostIoWrite16(0x04000184, HostIoRead16(0x04000184) | 0xc000);
        return -1;
    }
    irq = func_02080b08();
    if (HostIoRead16(0x04000184) & 2) {
        func_02080b1c(irq);
        return -2;
    }
    HostIoWrite32(0x04000188, r.w);
    func_02080b1c(irq);
    return 0;
}
#else
s32 func_0206cdb8(u32 param_1, s32 param_2, u32 param_3)
{
    Reg r;
    u32 irq;

    r.f.lo = param_1;
    r.f.b = param_3;
    r.f.hi = param_2;
    if (*(vu16 *)0x04000184 & 0x4000) {
        *(vu16 *)0x04000184 = *(vu16 *)0x04000184 | 0xc000;
        return -1;
    }
    irq = func_02080b08();
    if (*(vu16 *)0x04000184 & 2) {
        func_02080b1c(irq);
        return -2;
    }
    *(vu32 *)0x04000188 = r.w;
    func_02080b1c(irq);
    return 0;
}
#endif
