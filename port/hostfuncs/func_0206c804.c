#include "ctx.h"
/* MIi_CpuCopy32: copies size bytes (rounded up to words) from src to dst, a word at a time. */
/* I/O registers (0x04xxxxxx) are not mapped on the host: those accesses go through the hardware layer. */
extern void HostIoWrite32(u32 addr, u32 value);
extern u32 HostIoRead32(u32 addr);
static void Wr32(u32 a, u32 v) { if ((a >> 24) == 0x04) HostIoWrite32(a, v); else *(u32 *)a = v; }
static u32 Rd32(u32 a) { return ((a >> 24) == 0x04) ? HostIoRead32(a) : *(const u32 *)a; }

void func_0206c804(const void *src, void *dst, u32 size)
{
    u32 s = (u32)src;
    u32 d = (u32)dst;
    u32 end = d + size;
    while ((s32)d < (s32)end) {
        Wr32(d, Rd32(s));
        s += 4;
        d += 4;
    }
}
