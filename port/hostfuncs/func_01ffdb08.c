#include "ctx.h"
/* Returns entry i (0..2) of the pointer table at data_0214f464 + 0x54, or NULL when i is out of range. */

extern u8 *data_0214f464;

void *func_01ffdb08(s32 i)
{
    if (i < 0 || i >= 3) {
        return NULL;
    }
    return *(void **)(data_0214f464 + 0x54 + i * 4);
}
