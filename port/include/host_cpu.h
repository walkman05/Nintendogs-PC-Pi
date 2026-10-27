/* Host stand-ins for ARM-only operations used by the decompiled code's inline assembly (PC build only).
 * CPSR and CP15 are not real on the PC: the CPSR is a variable the hardware layer (milestone 4) consults for
 * the IRQ/FIQ mask, and cache / protection-unit / TCM operations do nothing. */
#ifndef HOST_CPU_H
#define HOST_CPU_H
#include "ctx.h"

extern u32 host_cpsr;   /* starts as 0x1f (system mode, interrupts enabled) */
extern void hw_irq_poll(void);  /* takes a pending interrupt if the CPSR now allows it */

static inline u32 HostCpsrRead(void) { return host_cpsr; }
static inline void HostCpsrWriteControl(u32 v)
{
    host_cpsr = (host_cpsr & ~0xffu) | (v & 0xffu);
    if (!(host_cpsr & 0x80))
        hw_irq_poll();
}
static inline void HostCp15Write(void) { }
static inline u32 HostCp15Read(void) { return 0; }

static inline u32 HostSwp(vu32 *p, u32 v) { u32 old = *p; *p = v; return old; }
static inline u8 HostSwpb(vu8 *p, u32 v) { u8 old = *p; *p = (u8)v; return old; }
static inline u32 HostClz(u32 x) { return x ? (u32)__builtin_clz(x) : 32; }

#endif
