#include "ctx.h"

#ifdef NDOGS_PC
extern void func_0206c8b8(void *src, void *dst);
#else
extern void func_0206c8b8(void *p);
#endif

#ifdef NDOGS_PC
/* MTX_MULT_3x3: the command, then the 9 words at obj. On the DS r1 still holds the FIFO address for
 * func_0206c8b8's destination; the C dropped it (on the PC the copy went wherever the stack pointed), so it is
 * passed explicitly, as in func_0206a0a4. */
void func_0206a088(void *obj)
{
    *(vu32 *)0x04000400 = 0x1a;
    func_0206c8b8(obj, (void *)0x04000400);
}
#else
void func_0206a088(void *obj)
{
    vu32 *reg = (vu32 *)0x04000400;
    void (*fp)(void *) = func_0206c8b8;
    *reg = 0x1a;
    fp(obj);
}
#endif
