#include "ctx.h"
/* Node callback: transform the node's translation (obj+0x24) by the matrix at mgr+0x1e0 into mgr+0x318. */

extern u8 *data_027c0360;
extern void func_01ff8894(const s32 *v, const s32 *m, s32 *out);

void func_01ffab98(void *unused, s32 *obj)
{
    u8 *mgr = data_027c0360;

    (void)unused;
    func_01ff8894(obj, (const s32 *)(mgr + 0x1e0), (s32 *)(mgr + 0x318));
}
