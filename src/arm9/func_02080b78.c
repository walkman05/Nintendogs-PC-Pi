#include "ctx.h"

u32 func_02080b78(u32 x)
{
#ifdef NDOGS_PC
    /* Busy-wait: subtract 4 until the subtraction borrows (always at least once). Same final value. */
    x = (x & 3u) - 4u;
#else
    asm {
        loop:
        subs x, x, #4
        bhs loop
    }
#endif
    return x;
}
