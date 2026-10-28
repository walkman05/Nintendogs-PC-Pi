/* Force-included first in every decompiled file when building for the PC host (NDOGS_PC).
 * Maps mwccarm-isms onto GCC/Clang; nothing here is used by the matching DS build. */
#ifndef HOST_COMPAT_H
#define HOST_COMPAT_H
#ifdef NDOGS_PC
/* Inline `asm {}` blocks are mwcc-only: each one is wrapped as #ifdef NDOGS_PC <C equivalent> #else asm {...} #endif
 * in its file, using the helpers in host_cpu.h. */
#include "host_cpu.h"
/* Hardware registers without a trap (port/src/hw.c): the hottest register users call these under NDOGS_PC, since
 * a trapped access costs ~17 us. A write that may make an interrupt pending is followed by hw_irq_poll(). */
unsigned int HostIoRead32(unsigned int addr);
unsigned short HostIoRead16(unsigned int addr);
void HostIoWrite32(unsigned int addr, unsigned int value);
void HostIoWrite16(unsigned int addr, unsigned short value);
void HostIoWrite8(unsigned int addr, unsigned char value);
static inline unsigned long long HostIoRead64(unsigned int addr)
{
    unsigned int lo = HostIoRead32(addr);
    return lo | (unsigned long long)HostIoRead32(addr + 4) << 32;
}
/* Full-game mode (port/src/main.c; default on, --demo turns it off): demo-only restrictions in the game code are
 * lifted behind `if (ndogs_full)`. */
extern int ndogs_full;
/* The retail build (host_compile.py --target retail) runs on the Dachshund & Friends cartridge, whose sound archive
 * is renumbered (one sequence dropped, groups added). The game objects are shared by both exes, so the few
 * functions retail changed for that pick their numbers at run time: NDOGS_R(retail value, demo value). */
extern int ndogs_retail;
#define NDOGS_R(retail, demo) (ndogs_retail ? (retail) : (demo))
static inline void HostIoWrite64(unsigned int addr, unsigned long long value)
{
    HostIoWrite32(addr, (unsigned int)value);
    HostIoWrite32(addr + 4, (unsigned int)(value >> 32));
}
#endif
#endif
