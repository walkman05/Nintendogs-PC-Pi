#pragma avoid_byte strb
#include "ctx.h"

#ifdef NDOGS_PC
u32 func_02029290(u8 *p, u32 a, u32 b)
#else
void func_02029290(u8 *p, u32 a, u32 b)
#endif
{
    u32 z = 0;
    *(u8 *)(p + 168) = (u8)z;
    *(u16 *)(p + 256 + 18) = (u16)z;
    *(u16 *)(p + 240) = (u16)a;
#ifdef NDOGS_PC
    return HostSwpb((vu8 *)(p + 165), b);
#else
    *(u8 *)(p + 165) = (u8)b;
#endif
}
