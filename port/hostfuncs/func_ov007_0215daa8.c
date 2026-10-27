#include "ctx.h"

/* Overlay 7 tail-call thunk: returns func_020d6c94() (a u16, zero-extended). */

extern u16 func_020d6c94(void);

u32 func_ov007_0215daa8(void)
{
    return (u32)func_020d6c94();
}
