#include "ctx.h"

void func_02080148(u32 param_1, s32 param_2)
{
    s32 end = param_2 + param_1;
    u32 a = param_1 & ~31u;
    do {
#ifdef NDOGS_PC
        HostCp15Write();
#else
        asm { mcr p15, 0, a, c7, c5, 1 }
#endif
        a += 32;
    } while ((s32)a < end);
}
