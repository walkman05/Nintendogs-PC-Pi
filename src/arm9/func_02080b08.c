#include "ctx.h"

u32 func_02080b08(void)
{
    u32 nw;
    u32 old;
#ifdef NDOGS_PC
    old = HostCpsrRead();
    nw = old | 128;
    HostCpsrWriteControl(nw);
#else
    asm {
        mrs old, cpsr
        orr nw, old, #128
        msr CPSR_c, nw
    }
#endif
    return old & 128;
}
