#include "ctx.h"

#ifdef NDOGS_PC
extern void func_0206be70(u32 *src, u32 *dst);
#else
extern void func_0206be70(u32 a);
#endif

#ifdef NDOGS_PC
/* MTX_MULT_4x3: the command, then the 12 words of the matrix at a. On the DS r1 still holds the FIFO address for
 * func_0206be70's destination; the C dropped it (the PC passed whatever was on the stack: once the function's own
 * address, a crash). Passed explicitly. */
void func_0206a0a4(u32 a)
{
    *(vu32 *)0x04000400 = 25;
    func_0206be70((u32 *)a, (u32 *)0x04000400);
}
#else
void func_0206a0a4(u32 a)
{
    void (*fp)(u32) = func_0206be70;
    *(vu32 *)0x04000400 = 25;
    fp(a);
}
#endif
