#include "ctx.h"
#include "host_cpu.h"
/* DC_StoreAll: cleans every data-cache line by set/way (CP15 c7,c10,2). No cache on the host. */

void func_02080074(void)
{
    HostCp15Write();
}
