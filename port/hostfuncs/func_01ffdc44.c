#include "ctx.h"
/* MIi_DmaSetParams: program DMA channel dmaNo (source, destination, control) with interrupts disabled. */

extern void HostIoWrite32(u32 addr, u32 value);
extern u32 func_02080b08(void);       /* OS_DisableInterrupts */
extern void func_02080b1c(u32 mode);  /* OS_RestoreInterrupts */

void func_01ffdc44(u32 dmaNo, u32 src, u32 dest, u32 ctrl)
{
    u32 reg = 0x040000b0u + dmaNo * 12u;
    u32 irq = func_02080b08();

    HostIoWrite32(reg, src);
    HostIoWrite32(reg + 4, dest);
    HostIoWrite32(reg + 8, ctrl);
    func_02080b1c(irq);
}
