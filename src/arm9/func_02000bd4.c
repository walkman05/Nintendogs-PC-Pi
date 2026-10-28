#include "ctx.h"

u32 func_02080b08(void);
#ifdef NDOGS_PC /* NDOGS_PC: return type of func_0207eef8 as defined (x86 extends by the caller's prototype, the DS by the callee) */
u16 func_0207eef8(u32 a);
#else
u32 func_0207eef8(u32 a);
#endif
u32 func_0207efb4(u32 a);
void func_02080b1c(u32 a);
void WaitByLoop(u32 n);

void func_02000bd4(u32 param_1, u32 *param_2)
{
    u32 iVar1 = 0x027fffe8;

    for (;;) {
        param_2[1] = func_02080b08();
        param_2[0] = func_0207eef8(iVar1) & 0x40;
        if (param_2[0] != 0) {
            return;
        }
        if (func_0207efb4(param_1) == 0) {
            return;
        }
        func_02080b1c(param_2[1]);
        WaitByLoop(1);
    }
}
