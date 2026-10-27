#include "ctx.h"
/* OSi exception context save + user handler: func_02080694 (save registers/CPSR into the context, r0 passed
 * through) then func_02080724 (call the user exception handler). The mode argument is passed on in r1 by the
 * original but func_02080694 overwrites r1 before using it, so it is unused here. */
extern void func_02080694(u32 thumb);
extern void func_02080724(void);

void func_02080680(u32 thumb, u32 mode)
{
    (void)mode;
    func_02080694(thumb);
    func_02080724();
}
