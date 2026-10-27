#include "ctx.h"
#include "host_cpu.h"
/* OS_LoadContext: restores the divider/sqrt state (func_02066224, from +0x48) and the CPSR from a thread context,
 * then (on the DS) jumps into that context's saved registers and pc and never returns. A C function cannot switch
 * register sets or stacks, so the host restores the state and returns to its caller; real thread switching has to
 * be provided by the host thread layer. */
extern void func_02066224(u32 *cp);

void func_0207fa30(u32 *ctx)
{
    func_02066224(ctx + 0x48 / 4);
    host_cpsr = ctx[0]; /* the original returns through SPSR = saved CPSR */
}
