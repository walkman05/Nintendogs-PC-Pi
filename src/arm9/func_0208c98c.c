#include "ctx.h"

typedef struct { s32 x; s32 y; } P2;

#ifdef NDOGS_PC
/* b and c are the two words of the file id (r1/r2 on the DS). Reading them as one struct through &b
 * relies on b and c being adjacent in memory: true for x86 stack arguments, but ARM gcc spills
 * register arguments downwards, so &b + 1 is a (the file object, not the id). Pass them explicitly. */
extern u32 func_02088064(u32 *a, u32 b, ...);

void func_0208c98c(s32 a, s32 b, s32 c, s32 d)
{
    func_02088064((u32 *)a, b, c);
}
#else
extern void func_02088064(s32 a, P2 s);

void func_0208c98c(s32 a, s32 b, s32 c, s32 d)
{
    func_02088064(a, *(P2 *)&b);
}
#endif
