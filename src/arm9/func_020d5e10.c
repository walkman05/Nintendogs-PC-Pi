#include "ctx.h"

extern u8 data_02159520[];
extern u16 data_021594e4;
extern void func_0206c804(u32 *p, u8 *dst, u32 n);
#ifdef NDOGS_PC /* NDOGS_PC: return type of func_02064668 as defined (x86 extends by the caller's prototype, the DS by the callee) */
extern u16 func_02064668(void);
#else
extern u32 func_02064668(void);
#endif

#ifdef NDOGS_PC /* NDOGS_PC: widened parameter 0: the DS callee uses the whole register */
void func_020d5e10(u32 param_1, u32 *param_2)
{
#else
void func_020d5e10(u16 param_1, u32 *param_2)
{
#endif
    func_0206c804(param_2, data_02159520, 64);
    *(u16 *)(data_02159520 + 24) = (u16)func_02064668();
    *(u16 *)(data_02159520 + 50) = param_1;
    data_021594e4 = data_021594e4 + 1;
    *(u16 *)(data_02159520 + 12) = data_021594e4;
}
