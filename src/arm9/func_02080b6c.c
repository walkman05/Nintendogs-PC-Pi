#include "ctx.h"
#ifdef NDOGS_PC
extern void hw_halt(void);
#endif

u32 func_02080b6c(void)
{
    u32 z = 0;
#ifdef NDOGS_PC
    hw_halt(); /* CP15 wait-for-interrupt: the host advances to the next interrupt and delivers it */
#else
    asm { mcr p15, 0, z, c7, c0, 4 }
#endif
    return z;
}
