#include "ctx.h"

void func_020800d4(u32 a, s32 n)
{
    u32 end = n + a;
    u32 p = a & 0xffffffe0u;
    do {
#ifdef NDOGS_PC
        HostCp15Write();
#else
        asm { mcr p15, 0, p, c7, c6, 1 }
#endif
        p = p + 0x20;
    } while ((s32)p < (s32)end);
}
