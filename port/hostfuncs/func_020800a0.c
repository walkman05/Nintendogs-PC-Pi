#include "ctx.h"
#include "host_cpu.h"
/* DC_FlushAll: drains the write buffer and cleans+invalidates every data-cache line (CP15 c7,c10,4 / c7,c14,2).
 * No cache on the host. */

void func_020800a0(void)
{
    HostCp15Write();
}
