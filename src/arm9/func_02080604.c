#include "ctx.h"
#ifdef NDOGS_PC
u32 func_02080604(void) { u32 r; r = HostCp15Read(); /* protection region 2 */ return r; }
#else
u32 func_02080604(void) { u32 r; asm { mrc p15, 0, r, c6, c2, 0 } return r; }
#endif
