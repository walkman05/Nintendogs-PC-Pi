#include "ctx.h"
/* G3 4x4 matrix load body: sends 16 words from src to the GX FIFO (MTX_LOAD_4x4 parameters).
 * The original takes the FIFO address in r1, which its only caller (func_0206a0dc) leaves at 0x04000400 without
 * passing it as a C argument, so the host version uses that address directly (GXFIFO accepts writes anywhere in
 * 0x04000400-0x0400043f). */
/* I/O registers (0x04xxxxxx) are not mapped on the host: those accesses go through the hardware layer. */
extern void HostIoWrite32(u32 addr, u32 value);
extern u32 HostIoRead32(u32 addr);
static void Wr32(u32 a, u32 v) { if ((a >> 24) == 0x04) HostIoWrite32(a, v); else *(u32 *)a = v; }

void func_0206be94(const u32 *src)
{
    int i;
    for (i = 0; i < 16; i++) {
        Wr32(0x04000400 + (u32)(i & 7) * 4, src[i]); /* 8-word bursts at 0x04000400..0x0400041c */
    }
}
