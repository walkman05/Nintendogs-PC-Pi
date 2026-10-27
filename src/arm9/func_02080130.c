#include "ctx.h"

void func_02080130(void)
{
    u32 z = 0;
#ifdef NDOGS_PC
    (void)z;
    HostCp15Write();
#else
    asm { mcr p15, 0, z, c7, c10, 4 }
#endif
}
