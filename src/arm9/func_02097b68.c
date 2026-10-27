#include "ctx.h"

extern u8 *volatile data_02159060;

#ifdef NDOGS_PC
u8 *func_02097b68(void)
#else
void func_02097b68(void)
#endif
{
    u8 *p = data_02159060;
    if (*(s16 *)(p + 108) == 2) {
        *(u16 *)(p + 108) = 3;
        *(u16 *)(data_02159060 + 110) = 0;
    }
#ifdef NDOGS_PC
    return data_02159060;
#endif
}
