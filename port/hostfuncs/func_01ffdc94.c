#include "ctx.h"

/* OS IRQ dispatcher (installed at DTCM+0x3ffc): if IME is set, acknowledge the lowest pending enabled interrupt
 * in IF and call its handler from the table at DTCM+4. The original then tail-calls the thread rescheduler
 * (func_01ffdcec); host threads arrive with the hardware layer, so that step is skipped for now. */
extern u32 HostIoRead32(u32 addr);
extern void HostIoWrite32(u32 addr, u32 value);
extern u8 data_027c0004[];

void func_01ffdc94(void)
{
    u32 pending, bit, irq;
    if (HostIoRead32(0x04000208) == 0)
        return;
    pending = HostIoRead32(0x04000210) & HostIoRead32(0x04000214);
    if (pending == 0)
        return;
    for (irq = 0; !(pending & (1u << irq)); irq++)
        ;
    bit = 1u << irq;
    HostIoWrite32(0x04000214, bit);
    ((void (*)(void))((u32 *)data_027c0004)[irq])();
}
