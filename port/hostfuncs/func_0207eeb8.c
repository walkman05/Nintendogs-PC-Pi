#include "ctx.h"
/* OS_ReleaseLockID: sets the ID's bit again in the shared lock-ID bitmap at 0x027fffb0. */

void func_0207eeb8(u32 id)
{
    vu32 *w = (vu32 *)0x027fffb0;
    u32 n;

    if ((s32)(id - 0x60) >= 0) {
        w++;
        n = id - 0x60;
    } else {
        n = id - 0x40;
    }
    n &= 0xff; /* ARM register shifts use the low byte; 32 or more shifts everything out */
    if (n < 32) {
        *w = *w | (0x80000000u >> n);
    }
}
