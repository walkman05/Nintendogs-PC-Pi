#include "ctx.h"
#include "host_cpu.h"

/* OS_SaveContext: saves the divider/sqrt state (func_02066290, at +0x48) and the CPSR into a thread context and
 * returns 0. On the DS the register set is saved too, so OS_LoadContext later resumes here returning 1; on the
 * host the thread is a fiber (port/src/host_threads.c), which resumes inside OS_LoadContext instead. */
extern void func_02066290(u32 *cp);
extern int host_ctx_save(u32 *ctx);

s32 func_0207f9e4(u32 *ctx)
{
    func_02066290(ctx + 0x48 / 4);
    ctx[0] = HostCpsrRead();
    ctx[1] = 1; /* saved r0: the value the context resumes with */
    return host_ctx_save(ctx);
}
