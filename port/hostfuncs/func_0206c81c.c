#include "ctx.h"
/* MIi_CpuSend32: writes size bytes (rounded up to words) from src to one fixed address (a FIFO register). */
/* I/O registers (0x04xxxxxx) are not mapped on the host: those accesses go through the hardware layer. */
extern void HostIoWrite32(u32 addr, u32 value);
extern u32 HostIoRead32(u32 addr);
static void Wr32(u32 a, u32 v) { if ((a >> 24) == 0x04) HostIoWrite32(a, v); else *(u32 *)a = v; }
static u32 Rd32(u32 a) { return ((a >> 24) == 0x04) ? HostIoRead32(a) : *(const u32 *)a; }

void func_0206c81c(const void *src, void *dst, u32 size)
{
    u32 s = (u32)src;
    u32 end = s + size;
    while ((s32)s < (s32)end) {
        Wr32((u32)dst, Rd32(s));
        s += 4;
    }
}
