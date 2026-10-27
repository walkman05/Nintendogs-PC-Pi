#include "ctx.h"
/* Debugger exit (semihosting ReportException / SYS_EXIT): stops the program; the host exits the process. */
extern void exit(int status);

void func_02063b4c(void)
{
    exit(0);
}
