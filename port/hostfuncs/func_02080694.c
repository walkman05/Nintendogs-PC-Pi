#include "ctx.h"
#include "host_cpu.h"

/* OSi exception context save (fills the context at data_02155f50 from the exception stack). The host has no ARM
 * register file, so only the CPSR/SPSR (emulated) and the Thumb flag are recorded; register slots are zeroed. */

extern u8 data_02155f50[];

void func_02080694(u32 thumb)
{
    u32 *ctx = (u32 *)data_02155f50;
    int i;
    for (i = 0; i < 0x80 / 4; i++)
        ctx[i] = 0;
    ctx[0x74 / 4] = HostCpsrRead();
    ctx[0x6c / 4] = thumb;
    ctx[0x7c / 4] = HostCpsrRead();
}
