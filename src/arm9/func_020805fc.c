#include "ctx.h"

void func_020805fc(u32 v)
{
#ifdef NDOGS_PC
    (void)v; HostCp15Write(); /* protection region 2 (no-op on PC) */
#else
    asm { mcr p15, 0, v, c6, c2, 0 }
#endif
}
