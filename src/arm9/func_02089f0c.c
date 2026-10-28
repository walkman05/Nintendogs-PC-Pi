#include "ctx.h"
#ifdef NDOGS_PC /* NDOGS_PC: return type of func_02089e78 as defined (x86 extends by the caller's prototype, the DS by the callee) */
extern BOOL func_02089e78(u32 a);
#else
extern u8 func_02089e78(u32 a);
#endif
extern u32 *func_0208a240(u32 a);
extern void func_02089d08(void);
void func_02089f0c(u32 param_1)
{
    u8 bVar1;
    u32 *piVar2;
    bVar1 = func_02089e78(param_1);
    if (bVar1) {
        return;
    }
    do {
        piVar2 = func_0208a240(0);
    } while (piVar2 != NULL);
    bVar1 = func_02089e78(param_1);
    if (bVar1) {
        return;
    }
    func_02089d08();
    bVar1 = func_02089e78(param_1);
    if (bVar1) {
        return;
    }
    do {
        func_0208a240(1);
        bVar1 = func_02089e78(param_1);
    } while (!bVar1);
}
