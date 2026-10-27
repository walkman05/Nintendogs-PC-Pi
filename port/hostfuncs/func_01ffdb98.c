#include "ctx.h"
/* MIi_DmaSetParams: write source, destination and control of DMA channel dmaNo (no interrupt guard). */

extern void HostIoWrite32(u32 addr, u32 value);

void func_01ffdb98(u32 dmaNo, u32 src, u32 dest, u32 ctrl)
{
    u32 reg = 0x040000b0u + dmaNo * 12u;

    HostIoWrite32(reg, src);
    HostIoWrite32(reg + 4, dest);
    HostIoWrite32(reg + 8, ctrl);
}
