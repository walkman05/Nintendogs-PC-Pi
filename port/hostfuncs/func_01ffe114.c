#include "ctx.h"
/* OSi_DoResetSystem: wait for the reset request, disable IME, reload the ARM9/ARM7 boot binaries from the card
 * (OSi_ReloadRomData) and jump to the reloaded ARM9 entry point (OSi_DoBoot). Never returns on hardware. */

extern void HostIoWrite32(u32 addr, u32 value);
extern void HostIoWrite16(u32 addr, u16 value);
extern void HostIoWrite8(u32 addr, u8 value);
extern u32 HostIoRead32(u32 addr);
extern u16 HostIoRead16(u32 addr);

extern vu16 data_02156008;            /* set to 1 when the reset request has been acknowledged */
extern u8 data_027c0000[];            /* DTCM base */
extern u32 func_02080b08(void);       /* OS_DisableInterrupts */
extern void func_02080b1c(u32 mode);  /* OS_RestoreInterrupts */
extern void func_02080074(void);
extern void func_02080068(void);
extern void func_0208013c(void);
extern void func_02080130(void);

#define MEM32(a) (*(vu32 *)(u32)(a))

/* Card ROM read in 512-byte pages through the slot-1 registers (command 0xB7), the CARD_ReadRom loop
 * used during reset: copies len bytes of ROM from offset src to dst. */
static void ResetReadRom(u32 src, u32 dst, s32 len)
{
    u32 ctrl = (MEM32(0x027ffe60) & ~0x07000000u) | 0xa1000000u;
    s32 pos = -(s32)(src & 0x1ffu);

    while (HostIoRead32(0x040001a4u) & 0x80000000u) {
    }
    HostIoWrite8(0x040001a1u, 0x80);
    src += (u32)pos;
    if (pos >= len) {
        return;
    }
    do {
        u32 cnt;

        HostIoWrite8(0x040001a8u, 0xb7);
        HostIoWrite8(0x040001a9u, (u8)(src >> 24));
        HostIoWrite8(0x040001aau, (u8)(src >> 16));
        HostIoWrite8(0x040001abu, (u8)(src >> 8));
        HostIoWrite8(0x040001acu, (u8)src);
        HostIoWrite8(0x040001adu, 0);
        HostIoWrite8(0x040001aeu, 0);
        HostIoWrite8(0x040001afu, 0);
        HostIoWrite32(0x040001a4u, ctrl);
        do {
            cnt = HostIoRead32(0x040001a4u);
            if (cnt & 0x00800000u) {
                u32 data = HostIoRead32(0x04100010u);
                if (pos >= 0 && pos < len) {
                    MEM32(dst + (u32)pos) = data;
                }
                pos += 4;
            }
        } while (cnt & 0x80000000u);
        src += 0x200;
    } while (pos < len);
}

/* OSi_ReloadRomData */
static void ResetReloadRomData(void)
{
    u32 romBase = MEM32(0x027ffc2c);
    u32 arm9Rom, arm9Ram, arm9Size, arm7Rom, arm7Ram, arm7Size;
    u32 irq;

    if (romBase >= 0x8000) {
        ResetReadRom(romBase, 0x027ffe00, 0x160);
    }
    arm9Rom = MEM32(0x027ffe20);
    arm9Ram = MEM32(0x027ffe28);
    arm9Size = MEM32(0x027ffe2c);
    arm7Rom = MEM32(0x027ffe30);
    arm7Ram = MEM32(0x027ffe38);
    arm7Size = MEM32(0x027ffe3c);

    irq = func_02080b08();
    func_02080074();
    func_02080068();
    func_02080b1c(irq);
    func_0208013c();
    func_02080130();

    arm9Rom += romBase;
    if (arm9Rom < 0x8000) {
        u32 d = 0x8000 - arm9Rom;
        arm9Ram += d;
        arm9Size -= d;
        arm9Rom = 0x8000;
    }
    arm7Rom += romBase;
    ResetReadRom(arm9Rom, arm9Ram, (s32)arm9Size);
    ResetReadRom(arm7Rom, arm7Ram, (s32)arm7Size);
}

static void ResetFill32(u32 value, u32 dst, u32 size)
{
    u32 end = dst + size;

    while ((s32)dst < (s32)end) {
        MEM32(dst) = value;
        dst += 4;
    }
}

/* OSi_DoBoot */
static void ResetDoBoot(void)
{
    u32 keep;

    HostIoWrite32(0x04000208u, 0x04000000u);
    *(vu32 *)(data_027c0000 + 0x3ffc) = 0;
    while ((HostIoRead16(0x04000180u) & 0xf) != 1) {
    }
    HostIoWrite16(0x04000180u, 0x100);
    keep = MEM32(0x027ffd9c);
    ResetFill32(0, 0x027ffd80, 0x80);
    MEM32(0x027ffd9c) = keep;
    ResetFill32(0, 0x027fff80, 0x80);
    while ((HostIoRead16(0x04000180u) & 0xf) == 1) {
    }
    HostIoWrite16(0x04000180u, 0);
    /* The original loads r0-r10 from the (just cleared) 0x027fff80 block, r11 = 0, and branches to the ARM9
     * entry point from the reloaded ROM header. On the PC this address holds ARM code: the hardware layer must
     * catch the reset before this point. */
    ((void (*)(void))MEM32(0x027ffe24))();
}

void func_01ffe114(void)
{
    while (data_02156008 == 0) {
    }
    HostIoWrite16(0x04000208u, 0);
    ResetReloadRomData();
    ResetDoBoot();
}
