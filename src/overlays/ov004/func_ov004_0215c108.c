#include "ctx.h"

#ifdef NDOGS_PC
static inline u8 SwapByte(u32 v, vu8 *p) { return HostSwpb(p, v); }
#else
static inline u8 SwapByte(u32 v, vu8 *p) { u8 old; asm { swpb old, v, [p] } return old; }
#endif

extern u8 *data_ov004_02170320;

u8 func_ov004_0215c108(void)
{
    u8 *q = data_ov004_02170320 + 0x3000;
    return SwapByte(1, q + 0x3cd);
}
