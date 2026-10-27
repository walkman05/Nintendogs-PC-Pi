#include "ctx.h"

/* Overlay 28 tail-call thunk: returns func_020d6c94() (a u16, zero-extended). */

extern u16 func_020d6c94(void);

u32 func_ov028_0215d74c(void)
{
    return (u32)func_020d6c94();
}
