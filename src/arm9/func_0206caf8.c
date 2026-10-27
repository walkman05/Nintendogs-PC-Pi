#include "ctx.h"
#ifdef NDOGS_PC
u32 func_0206caf8(u32 v, vu32 *p) { return HostSwp(p, v); }
#else
u32 func_0206caf8(u32 v, vu32 *p) { u32 old; asm { swp old, v, [p] } return old; }
#endif
