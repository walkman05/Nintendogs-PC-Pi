#include "ctx.h"
/* Debugger console putchar (semihosting SYS_WRITEC on the character at p); the host writes it to stdout. */
extern int putchar(int c);

void func_02063b24(const u8 *p)
{
    putchar(*p);
}
