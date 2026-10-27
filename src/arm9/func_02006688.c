#pragma avoid_byte strb
#include "ctx.h"

extern u8 data_0214f004[];

static inline u8 SwapByte(u32 v, vu8 *p)
{
    u8 old;
#ifdef NDOGS_PC
    old = *(u8 *)(p); /* swpb returns the previous byte */
#endif
    *(u8 *)(p) = (u8)(v);
    return old;
}

#ifdef NDOGS_PC
u32 func_02006688(void)
#else
void func_02006688(void)
#endif
{
    u32 v = 0xff;
    u16 z;
    vu8 *p = data_0214f004;
    vu8 *q = p + 1;
    u8 r;
    SwapByte(v, p);
    z = 0;
    r = SwapByte(v, q);
    *(vu16 *)(p + 2) = z;
#ifdef NDOGS_PC
    return r;
#else
    return;
#endif
}
