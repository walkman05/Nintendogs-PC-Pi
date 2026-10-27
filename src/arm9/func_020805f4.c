#include "ctx.h"
#ifdef NDOGS_PC
void func_020805f4(u32 x) { (void)x; HostCp15Write(); /* protection region 1 (no-op on PC) */ }
#else
void func_020805f4(u32 x) { asm { mcr p15, 0, x, c6, c1, 0 } }
#endif
