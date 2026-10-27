#include "ctx.h"
/* Node callback: copy the node matrix (func_0206c8d4) to mgr+0x52c. */

extern u8 *data_027c0360;
extern u64 func_0206c8d4(u32 *src, u32 *dst);

void func_01ff93c0(void *unused, s32 *obj)
{
    u8 *mgr = data_027c0360;

    (void)unused;
    func_0206c8d4((u32 *)obj, (u32 *)(mgr + 0x52c)); /* tail call; its r0/r1 result is not used by the caller */
}
