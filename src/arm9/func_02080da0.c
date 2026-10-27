#include "ctx.h"
#ifdef NDOGS_PC
u32 func_02080da0(u32 x) { u32 r; r = HostClz(x); return r; }
#else
u32 func_02080da0(u32 x) { u32 r; asm { clz r, x } return r; }
#endif
