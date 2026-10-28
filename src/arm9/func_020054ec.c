#pragma avoid_byte strb
#include "ctx.h"

#ifdef NDOGS_PC /* NDOGS_PC: widened parameter 1: the DS callee uses the whole register */
#ifdef NDOGS_PC /* NDOGS_PC: widened parameter 2: the DS callee uses the whole register */
void func_020054ec(void *p, u32 a, u32 b)
{
#else
void func_020054ec(void *p, u32 a, u8 b)
{
#endif
#else
void func_020054ec(void *p, u8 a, u8 b)
{
#endif
    vu8 *t;
    vu8 *u;
    u8 old;
    t = (vu8 *)(*(u32 *)((u8 *)p + 0x74) + 2);
    *(u8 *)(t) = (u8)(a);
    u = (vu8 *)(*(u32 *)((u8 *)p + 0x74) + 3);
    *(u8 *)(u) = (u8)(b);
    return;
}
