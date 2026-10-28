#include "ctx.h"

extern u32 data_02155f40;
extern u32 func_0207fb7c(void);

#ifdef NDOGS_PC
/* the arena starts are link-time constants that differ between builds of the game (port/src/loader.c) */
extern unsigned int ndogs_arena_main_lo, ndogs_arena_itcm_lo;
#define ARENA_MAIN_LO ndogs_arena_main_lo
#define ARENA_ITCM_LO ndogs_arena_itcm_lo
#else
#define ARENA_MAIN_LO 0x0219a7c0
#define ARENA_ITCM_LO 0x01ffee20
#endif

u32 func_020801d8(u32 param_1)
{
    u32 uVar1;

    switch (param_1) {
    case 0:
        return ARENA_MAIN_LO;
    case 2:
        if (data_02155f40 != 0) {
            uVar1 = func_0207fb7c();
            if ((uVar1 & 3) != 1) goto big;
        }
    zero:
        return 0;
    big:
        return 0x2400000;
    case 3:
        return ARENA_ITCM_LO;
    case 4:
        return 0x027c0be0;
    case 5:
        return 0x027ff000;
    case 6:
        return 0x037f8000;
    case 1:
    default:
        return 0;
    }
}
