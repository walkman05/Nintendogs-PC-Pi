#include "ctx.h"

extern u32 data_02142468;

#ifdef NDOGS_PC
u64 func_020ae688(void)
#else
void func_020ae688(void)
#endif
{
    data_02142468 = 0;
#ifdef NDOGS_PC
    return (u32)&data_02142468; /* DS: r0 = &data_02142468, r1 = 0 */
#endif
}
