#include "ctx.h"
/* BIOS WaitByLoop (SWI 0x03): busy-waits about 4 cycles per count; nothing to wait for on the host. */

void WaitByLoop(s32 count)
{
    (void)count;
}
