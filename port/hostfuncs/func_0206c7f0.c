#include "ctx.h"
/* MIi_CpuClear32: fills size bytes (rounded up to words) at dst with the word data. */
/* I/O registers (0x04xxxxxx) are not mapped on the host: those accesses go through the hardware layer. */
extern void HostIoWrite32(u32 addr, u32 value);
extern u32 HostIoRead32(u32 addr);
static void Wr32(u32 a, u32 v) { if ((a >> 24) == 0x04) HostIoWrite32(a, v); else *(u32 *)a = v; }

void func_0206c7f0(u32 data, void *dst, u32 size)
{
    u32 d = (u32)dst;
    u32 end = d + size;
    while ((s32)d < (s32)end) {
        Wr32(d, data);
        d += 4;
    }
}
