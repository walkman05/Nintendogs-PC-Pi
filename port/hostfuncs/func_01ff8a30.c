#include "ctx.h"
/* Node callback: when mgr+0x99 is 0, rotate the node by func_01ff9184 side 2 (translation kept); then, if
 * func_02027428(mgr) == 2, transform the node's translation by the matrix at mgr+0x1e0 into mgr+0x360. */

extern u8 *data_027c0360;
extern void func_01ff9184(u8 *mgr, s32 *m, s32 side);
extern s32 func_02027428(u8 *p);
extern void func_01ff8894(const s32 *v, const s32 *m, s32 *out);

void func_01ff8a30(void *unused, s32 *m)
{
    u8 *mgr = data_027c0360;
    s32 tx;
    s32 ty;
    s32 tz;

    (void)unused;
    if (*(s8 *)(mgr + 0x99) == 0) {
        tz = m[11];
        ty = m[10];
        tx = m[9];
        func_01ff9184(mgr, m, 2);
        m[9] = tx;
        m[10] = ty;
        m[11] = tz;
    }
    if (func_02027428(mgr) != 2)
        return;
    func_01ff8894(m, (const s32 *)(mgr + 0x1e0), (s32 *)(mgr + 0x360));
}
