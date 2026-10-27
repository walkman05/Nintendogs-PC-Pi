#include "ctx.h"
#include "host_cpu.h"
/* OSi_ExceptionHandler: calls the debugger hook in data_02155f4c if set, then, in abort (0x17) or undefined (0x1b)
 * mode, func_02080680 (exception context save / user handler), and finally halts forever. The original also
 * switches to a stack at 0x02000000 and passes sp & 1 in r0 and the mode in r1; the host passes 0 and the mode. */
extern u32 data_02155f4c;
extern void func_02080680(u32 thumb, u32 mode);

void func_0208060c(void)
{
    u32 mode;

    if (data_02155f4c != 0) {
        ((void (*)(void))data_02155f4c)();
    }
    mode = HostCpsrRead() & 0x1f;
    if (mode == 0x17 || mode == 0x1b) {
        func_02080680(0, mode);
    }
    /* Both of the original's exits are endless loops (with or without the debugger hook). */
    for (;;) {
    }
}
