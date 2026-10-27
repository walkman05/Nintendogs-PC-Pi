#include "ctx.h"

void func_02080068(void)
{
    u32 z = 0;
#ifdef NDOGS_PC
    (void)z;
    HostCp15Write();
#else
    asm { mcr p15, 0, z, c7, c6, 0 }
#endif
}
