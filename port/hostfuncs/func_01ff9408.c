#include "ctx.h"
/* Node callback: func_01ff9600 for side 1. */

extern void func_01ff9600(void *unused, s32 *m, s32 side);

void func_01ff9408(void *unused, s32 *m)
{
    func_01ff9600(unused, m, 1);
}
