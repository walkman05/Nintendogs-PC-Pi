#include "ctx.h"

#ifdef NDOGS_PC
extern void func_0206be70(u32 *src, u32 *dst);
#else
extern void func_0206be70(u32 a);
#endif

#ifdef NDOGS_PC
/* MTX_LOAD_4x3: as func_0206a0a4, the FIFO destination passed explicitly (r1 on the DS) */
void func_0206a0c0(u32 a)
{
    *(vu32 *)0x04000400 = 0x17;
    func_0206be70((u32 *)a, (u32 *)0x04000400);
}
#else
void func_0206a0c0(u32 a)
{
    vu32 *r = (vu32 *)0x04000400;
    *r = 0x17;
    func_0206be70(a);
}
#endif
