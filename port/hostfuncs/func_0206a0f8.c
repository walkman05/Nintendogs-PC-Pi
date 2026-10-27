#include "ctx.h"
/* GX FIFO clear: writes 128 zero words (NOP commands) to the four words at fifo (normally GXFIFO 0x04000400). */
/* I/O registers (0x04xxxxxx) are not mapped on the host: those accesses go through the hardware layer. */
extern void HostIoWrite32(u32 addr, u32 value);
extern u32 HostIoRead32(u32 addr);
static void Wr32(u32 a, u32 v) { if ((a >> 24) == 0x04) HostIoWrite32(a, v); else *(u32 *)a = v; }

void func_0206a0f8(u32 fifo)
{
    int i;
    for (i = 0; i < 128; i++) {
        Wr32(fifo + (u32)(i & 3) * 4, 0);
    }
}
