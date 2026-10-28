#include "ctx.h"

extern u16 *data_0215942c;
void func_020a85c4(u16 *p, u32 v);

#ifdef NDOGS_PC /* NDOGS_PC: widened parameter 0: the DS callee uses the whole register */
#ifdef NDOGS_PC /* NDOGS_PC: widened parameter 1: the DS callee uses the whole register */
#ifdef NDOGS_PC /* NDOGS_PC: widened parameter 2: the DS callee uses the whole register */
void func_020a8028(u32 param_1, u32 param_2, u32 param_3)
{
#else
void func_020a8028(u32 param_1, u32 param_2, u16 param_3)
{
#endif
#else
void func_020a8028(u32 param_1, u16 param_2, u16 param_3)
{
#endif
#else
void func_020a8028(u16 param_1, u16 param_2, u16 param_3)
{
#endif
    u16 *p = data_0215942c;
    func_020a85c4(p, 0xc002);
    p[2] = param_1;
    p[3] = param_2;
    p[4] = param_3;
}
