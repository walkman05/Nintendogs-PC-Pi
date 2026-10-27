#include "ctx.h"

extern u32 data_02150f30;

void func_0206d00c(void)
{
    u32 v;
#ifdef NDOGS_PC
    do {
        v = *(vu32 *)&data_02150f30;
    } while (v == 1);
#else
    asm { ldr r12, =data_02150f30 }
    do {
        asm { ldr v, [r12] }
    } while (v == 1);
#endif
}
