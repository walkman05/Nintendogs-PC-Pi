#include "ctx.h"

extern u32 data_02150e38;
extern s32 func_02068c64(void *p);

s32 func_02068c08(void)
{
    return func_02068c64(&data_02150e38);
}
