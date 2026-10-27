/* DS I/O register access (0x04000000+) for host-only C. Milestone 3: the I/O region is mapped as plain memory
 * (see platform_mem.c), so these read and write it directly, the same as decompiled C that dereferences register
 * addresses. The hardware layer (milestone 4) gives registers their real behaviour. */
#include "ctx.h"

void HostIoWrite32(u32 addr, u32 value) { *(vu32 *)addr = value; }
void HostIoWrite16(u32 addr, u16 value) { *(vu16 *)addr = value; }
void HostIoWrite8(u32 addr, u8 value) { *(vu8 *)addr = value; }
u32 HostIoRead32(u32 addr) { return *(vu32 *)addr; }
u16 HostIoRead16(u32 addr) { return *(vu16 *)addr; }
u8 HostIoRead8(u32 addr) { return *(vu8 *)addr; }
