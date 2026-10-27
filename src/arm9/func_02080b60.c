#include "ctx.h"
#ifdef NDOGS_PC
u32 func_02080b60(void) { u32 r; r = HostCpsrRead(); return r & 0x1f; }
#else
u32 func_02080b60(void) { u32 r; asm { mrs r, cpsr } return r & 0x1f; }
#endif
