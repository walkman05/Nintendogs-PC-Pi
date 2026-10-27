#include "ctx.h"
/* MIi_DmaSetParams_wait (no interrupt guard): program DMA channel dmaNo, do the dummy reads, and on channel 0
 * apply the SDK's DMA0 workaround (reprogram it with a 1-word dummy transfer). */

extern void HostIoWrite32(u32 addr, u32 value);
extern u32 HostIoRead32(u32 addr);

void func_01ffdb38(u32 dmaNo, u32 src, u32 dest, u32 ctrl)
{
    u32 reg = 0x040000b0u + dmaNo * 12u;

    HostIoWrite32(reg, src);
    HostIoWrite32(reg + 4, dest);
    HostIoWrite32(reg + 8, ctrl);
    (void)HostIoRead32(0x040000b0u);
    (void)HostIoRead32(0x040000b0u);
    if (dmaNo == 0) {
        HostIoWrite32(reg, 0);
        HostIoWrite32(reg + 4, 0);
        HostIoWrite32(reg + 8, 0x81400001u);
    }
    (void)HostIoRead32(0x040000b0u);
    (void)HostIoRead32(0x040000b0u);
}
