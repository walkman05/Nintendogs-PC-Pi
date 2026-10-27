#include "ctx.h"

u32 func_020805c0(void)
{
    u32 m;
    u32 r;
#ifdef NDOGS_PC
    r = 0x027c000a; /* DTCM region register: base 0x027c0000 (this game's DTCM), size 16 KiB */
    m = 0xfffff000;
#else
    asm {
        mrc p15, 0, r, c9, c1, 0
        ldr m, =0xfffff000
    }
#endif
    return r & m;
}
