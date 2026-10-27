#include "ctx.h"
/* Debugger console getchar (semihosting SYS_READC); the host reads stdin, end of input reads as a newline. */
extern int getchar(void);

u32 func_02063b38(void)
{
    int c = getchar();
    if (c < 0) {
        return '\n';
    }
    return (u32)(c & 0xff);
}
