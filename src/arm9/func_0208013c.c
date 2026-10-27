#include "ctx.h"

u32 func_0208013c(void)
{
    u32 z = 0;
#ifdef NDOGS_PC
    HostCp15Write();
#else
    asm { mcr p15, 0, z, c7, c5, 0 }
#endif
    return z;
}
