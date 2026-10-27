typedef unsigned short u16;
typedef volatile unsigned short vu16;
typedef unsigned int u32;
typedef volatile unsigned int vu32;
#ifdef NDOGS_PC
/* host: hardware registers through HostIo* (no trap) */
void func_02067728(u32 param_1, u32 param_2)
{
    HostIoWrite16(0x04000280, 1);
    HostIoWrite32(0x04000290, 0);
    HostIoWrite32(0x04000294, param_1);
    HostIoWrite32(0x04000298, param_2);
    HostIoWrite32(0x0400029c, 0);
}
#else
void func_02067728(u32 param_1, u32 param_2)
{
    vu16 *r2;
    vu32 *r3;
    vu32 *r4;
    r2 = (vu16 *)0x04000280;
    *r2 = 1;
    r3 = (vu32 *)0x04000290;
    *r3 = 0;
    r3[1] = param_1;
    r4 = (vu32 *)0x04000298;
    *r4 = param_2;
    r4[1] = 0;
}
#endif
