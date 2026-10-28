#include "ctx.h"

#ifdef NDOGS_PC /* NDOGS_PC: widened parameter 1: the DS callee uses the whole register */
#ifdef NDOGS_PC /* NDOGS_PC: widened parameter 2: the DS callee uses the whole register */
#ifdef NDOGS_PC /* NDOGS_PC: widened parameter 3: the DS callee uses the whole register */
BOOL func_020bd824(u8 *param_1, u32 param_2, u32 param_3, u32 param_4)
{
#else
BOOL func_020bd824(u8 *param_1, u32 param_2, u32 param_3, u16 param_4)
{
#endif
#else
BOOL func_020bd824(u8 *param_1, u32 param_2, u16 param_3, u16 param_4)
{
#endif
#else
BOOL func_020bd824(u8 *param_1, u16 param_2, u16 param_3, u16 param_4)
{
#endif
    if (*(s16 *)(param_1 + 0x8a) == *(s16 *)(param_1 + 0x90)) {
        *(u16 *)(param_1 + 0x8a) = param_2;
        *(u16 *)(param_1 + 0x8c) = param_3;
        *(u16 *)(param_1 + 0x8e) = param_4;
        return TRUE;
    }
    return FALSE;
}
