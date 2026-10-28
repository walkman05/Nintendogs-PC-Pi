#include "ctx.h"

/* Overlay 7 tail-call thunk: returns func_020d6c94() (a u16, zero-extended). */

#ifdef NDOGS_PC /* NDOGS_PC: return type of func_020d6c94 as defined (x86 extends by the caller's prototype, the DS by the callee) */
extern u32 func_020d6c94(void);
#else
extern u16 func_020d6c94(void);
#endif

u32 func_ov007_0215daa8(void)
{
    return (u32)func_020d6c94();
}
