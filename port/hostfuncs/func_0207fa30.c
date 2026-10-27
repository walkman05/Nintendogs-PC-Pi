#include "ctx.h"
#include "host_cpu.h"

/* OS_LoadContext: restores the divider/sqrt state (func_02066224, from +0x48) and the CPSR, then switches to the
 * context's thread. On the host that is a fiber switch (port/src/host_threads.c); when this thread is switched
 * back to later, the call returns, which the SDK's callers treat like OS_SaveContext having returned 1. */
extern void func_02066224(u32 *cp);
extern void host_ctx_load(u32 *ctx);

void func_0207fa30(u32 *ctx)
{
    func_02066224(ctx + 0x48 / 4);
    host_cpsr = ctx[0]; /* the original returns through SPSR = saved CPSR */
    host_ctx_load(ctx);
}
