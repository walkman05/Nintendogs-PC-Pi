#include "ctx.h"
extern void func_02044ca0(u32 a, u32 b);
extern void func_020bdee8(u32 a);
#ifdef NDOGS_PC
u32 func_020cedd0(u32 p0, u32 p1)
{
    func_02044ca0(0, p1);
    func_020bdee8((p1 + 6) & 0xff);
    return (p1 + 6) & 0xff; /* DS: func_020bdee8 is empty, r0 keeps its argument */
}
#else
void func_020cedd0(u32 p0, u32 p1)
{
    func_02044ca0(0, p1);
    func_020bdee8((p1 + 6) & 0xff);
}
#endif
