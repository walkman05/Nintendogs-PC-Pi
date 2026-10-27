#include "ctx.h"

#ifdef NDOGS_PC
/* host: hardware registers through HostIo* (no trap) */
void func_02065f04(u32 param_1, u32 param_2)
{
    while (HostIoRead32(0x040001a4) & 0x80000000) {
    }
    HostIoWrite8(0x040001a1, 0xc0);
    HostIoWrite8(0x040001a8, (u8)(param_1 >> 24));
    HostIoWrite8(0x040001a9, (u8)(param_1 >> 16));
    HostIoWrite8(0x040001aa, (u8)(param_1 >> 8));
    HostIoWrite8(0x040001ab, (u8)param_1);
    HostIoWrite8(0x040001ac, (u8)(param_2 >> 24));
    HostIoWrite8(0x040001ad, (u8)(param_2 >> 16));
    HostIoWrite8(0x040001ae, (u8)(param_2 >> 8));
    HostIoWrite8(0x040001af, (u8)param_2);
}
#else
void func_02065f04(u32 param_1, u32 param_2)
{
    vu32 *flag = (vu32 *)0x040001a4;
    vu8 *r1 = (vu8 *)0x040001a1;
    vu8 *b0 = (vu8 *)0x040001a8;
    vu8 *b1 = (vu8 *)0x040001a9;
    vu8 *b2 = (vu8 *)0x040001aa;
    vu8 *b3 = (vu8 *)0x040001ab;
    vu8 *b4 = (vu8 *)0x040001ac;
    vu8 *b5 = (vu8 *)0x040001ad;
    vu8 *b6 = (vu8 *)0x040001ae;
    vu8 *b7 = (vu8 *)0x040001af;
    while (*flag & 0x80000000) {
    }
    *r1 = 0xc0;
    *b0 = (u8)(param_1 >> 24);
    *b1 = (u8)(param_1 >> 16);
    *b2 = (u8)(param_1 >> 8);
    *b3 = (u8)param_1;
    *b4 = (u8)(param_2 >> 24);
    *b5 = (u8)(param_2 >> 16);
    *b6 = (u8)(param_2 >> 8);
    *b7 = (u8)param_2;
}
#endif
