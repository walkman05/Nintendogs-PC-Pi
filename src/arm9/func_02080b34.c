#include "ctx.h"

u32 func_02080b34(void)
{
    u32 t;
    u32 r;
#ifdef NDOGS_PC
    r = HostCpsrRead();
    t = r | 192;
    HostCpsrWriteControl(t);
#else
    asm {
        mrs r, cpsr
        orr t, r, #192
        msr cpsr_c, t
    }
#endif
    return r & 192;
}
