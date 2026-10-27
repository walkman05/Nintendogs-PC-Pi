#include "ctx.h"
#include "host_cpu.h"
/* OS_SaveContext: saves the divider/sqrt state (func_02066290, at +0x48) and the CPSR into a thread context and
 * returns 0. The original also stores r0..r14, the SVC sp and a resume pc so that OS_LoadContext later "returns"
 * here with 1 (setjmp-like); plain C cannot capture registers, so only the CPSR and the r0 = 1 slot are written. */
extern void func_02066290(u32 *cp);

s32 func_0207f9e4(u32 *ctx)
{
    func_02066290(ctx + 0x48 / 4);
    ctx[0] = HostCpsrRead();
    ctx[1] = 1; /* saved r0: the value the context resumes with */
    return 0;
}
