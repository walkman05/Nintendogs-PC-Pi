#pragma avoid_byte strb
#include "ctx.h"
#ifdef NDOGS_PC
u32 func_020293d8(u8 *p, u16 a, u8 b)
#else
void func_020293d8(u8 *p, u16 a, u8 b)
#endif
{
    p[168] = 1;
    *(u16 *)(p + 256 + 18) = 0;
    *(u16 *)(p + 240) = a;
#ifdef NDOGS_PC
    return HostSwpb((vu8 *)(p + 165), b);
#else
    p[165] = b;
#endif
}
