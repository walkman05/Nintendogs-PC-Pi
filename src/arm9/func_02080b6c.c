#include "ctx.h"

u32 func_02080b6c(void)
{
    u32 z = 0;
#ifdef NDOGS_PC
    ; /* CP15 wait-for-interrupt (halt until IRQ): nothing to do on PC */
#else
    asm { mcr p15, 0, z, c7, c0, 4 }
#endif
    return z;
}
