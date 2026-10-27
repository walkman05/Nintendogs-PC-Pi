/* DS I/O register access (0x04000000+) for host-only C. Milestone 2 placeholder: writes are dropped and reads
 * return 0; the hardware layer (milestone 4) replaces these. */
#include "ctx.h"

void HostIoWrite32(u32 addr, u32 value) { (void)addr; (void)value; }
void HostIoWrite16(u32 addr, u16 value) { (void)addr; (void)value; }
void HostIoWrite8(u32 addr, u8 value) { (void)addr; (void)value; }
u32 HostIoRead32(u32 addr) { (void)addr; return 0; }
u16 HostIoRead16(u32 addr) { (void)addr; return 0; }
u8 HostIoRead8(u32 addr) { (void)addr; return 0; }
