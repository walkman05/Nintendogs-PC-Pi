#include "ctx.h"
#ifdef NDOGS_PC
u32 func_02080af4(void) { u32 t; u32 r; r = HostCpsrRead(); t = r & ~0x80u; HostCpsrWriteControl(t); return r & 0x80; }
#else
u32 func_02080af4(void) { u32 t; u32 r; asm { mrs r, cpsr; bic t, r, #0x80; msr cpsr_c, t } return r & 0x80; }
#endif
