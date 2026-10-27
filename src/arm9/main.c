#include "ctx.h"

void func_0200304c(void);
void func_02003018(void *p);
void func_020a44ec(void);

#ifdef NDOGS_PC
#define main ndogs_game_main  /* the host provides the process entry point */
#endif
void main(void)
{
    func_0200304c();
    func_02003018((void *)func_020a44ec);
}
