#include "ctx.h"
#include "host_cpu.h"
/* OS_GetLockID: takes the first free ID from the shared lock-ID bitmap at 0x027fffb0 (bit 31 of word 0 = ID 0x40,
 * word 1 = IDs 0x60..0x7f), clears its bit and returns it; 0xfffffffd (OS_LOCK_ID_ERROR) when none is free.
 * u64 return: low = r0 (the ID), high = r1 (the bit taken, 0 on failure); func_02080b98 reads both. */

u64 func_0207ee60(void)
{
    vu32 *w = (vu32 *)0x027fffb0;
    u32 base = 0x40;
    u32 v = w[0];
    u32 n;
    u32 bit;

    if (v == 0) {
        w++;
        v = *w;
        if (v == 0) {
            return 0xfffffffdu;
        }
        base = 0x60;
    }
    n = HostClz(v);
    bit = 0x80000000u >> n;
    *w = *w & ~bit;
    return (u64)(base + n) | ((u64)bit << 32);
}
