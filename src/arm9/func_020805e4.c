#include "ctx.h"

u32 func_020805e4(void)
{
    u32 r;
#ifdef NDOGS_PC
    r = HostCp15Read(); /* read CP15 control register */
#else
    asm { mrc p15, 0, r, c1, c0, 0 }
#endif
    r = r & ~1u;
#ifdef NDOGS_PC
    HostCp15Write(); /* write CP15 control register (no-op on PC) */
#else
    asm { mcr p15, 0, r, c1, c0, 0 }
#endif
    return r;
}
