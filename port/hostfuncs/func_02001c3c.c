#include "ctx.h"
/* Busy-waits while the first word of data_0214e6c0 is 1 (cleared from an interrupt / other context). */
extern u32 data_0214e6c0[];

void func_02001c3c(void)
{
    while (*(vu32 *)&data_0214e6c0[0] == 1) {
    }
}
