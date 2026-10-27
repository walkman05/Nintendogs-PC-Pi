#include "ctx.h"
/* MIi_DmaSetParams_wait: like func_01ffdb38 (DMA setup, dummy reads, DMA0 workaround) with interrupts disabled. */

extern void HostIoWrite32(u32 addr, u32 value);
extern u32 HostIoRead32(u32 addr);
extern u32 func_02080b08(void);       /* OS_DisableInterrupts */
extern void func_02080b1c(u32 mode);  /* OS_RestoreInterrupts */

void func_01ffdbd0(u32 dmaNo, u32 src, u32 dest, u32 ctrl)
{
    u32 reg = 0x040000b0u + dmaNo * 12u;
    u32 irq = func_02080b08();

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
    func_02080b1c(irq);
}
