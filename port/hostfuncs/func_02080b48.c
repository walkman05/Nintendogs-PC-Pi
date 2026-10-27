#include "ctx.h"
#include "host_cpu.h"
/* OS_RestoreIrqAndFiq-style: replaces the CPSR I/F bits (0xc0) with bits and returns the previous I/F bits. */

u32 func_02080b48(u32 bits)
{
    u32 old = HostCpsrRead();
    HostCpsrWriteControl((old & ~0xc0u) | bits);
    return old & 0xc0;
}
