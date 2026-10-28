#include "ctx.h"

extern u8 *data_0215be48;

#ifdef NDOGS_PC /* NDOGS_PC: widened parameter 0: the DS callee uses the whole register */
void func_02108e50(u32 param_1)
{
#else
void func_02108e50(u16 param_1)
{
#endif
    u8 *iVar1;
    iVar1 = data_0215be48 + 0x5028;
    if (*(u32 *)(data_0215be48 + 0x5028) != 0) {
        *(u16 *)(iVar1 + 0x1e8) = param_1;
        *(u16 *)(iVar1 + 0x208) = param_1;
    }
}
