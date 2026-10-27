/* Game card (ARM9 side): command bytes at 0x040001a8, ROMCTRL at 0x040001a4, data words at 0x04100010, transfer-
 * complete interrupt (IF bit 19) when AUXSPICNT bit 14 is set. Data comes from the user's ROM file (its path is
 * in extracted/host/rom_path.txt). The cartridge is treated as already in data mode (as after the DS boot). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hw.h"
#include "platform_mem.h"

#define R16(a) (*(volatile u16 *)io_reg(a))
#define R32(a) (*(volatile u32 *)io_reg(a))

static FILE *rom;
static u32 rom_size;
static u8 buf[0x4000];
static u32 len, pos;
u32 hw_card_reads;

void hw_card_open(const char *data_dir)
{
    char p[400], path[400];
    FILE *f;
    snprintf(p, sizeof p, "%s/host/rom_path.txt", data_dir);
    f = fopen(p, "r");
    if (!f || !fgets(path, sizeof path, f)) {
        fprintf(stderr, "card: %s missing - rerun port/tools/extract_rom.py\n", p);
        if (f)
            fclose(f);
        return;
    }
    fclose(f);
    path[strcspn(path, "\r\n")] = 0;
    rom = fopen(path, "rb");
    if (!rom) {
        fprintf(stderr, "card: cannot open the ROM at %s\n", path);
        return;
    }
    fseek(rom, 0, SEEK_END);
    rom_size = (u32)ftell(rom);
}

static void rom_read(u32 addr, u8 *dst, u32 n)
{
    memset(dst, 0xff, n);
    if (!rom || addr >= rom_size)
        return;
    if (addr < 0x8000)
        addr = 0x8000 + (addr & 0x1ff);  /* the secure area reads back like this once the card is in data mode */
    fseek(rom, (long)addr, SEEK_SET);
    if (fread(dst, 1, n, rom) != n)
        ;
}

/* ROMCTRL written with bit 31: run the command in the 8 command bytes. */
void hw_card_start(void)
{
    u32 ctrl = R32(0x040001a4);
    u8 cmd[8];
    u32 size = (ctrl >> 24) & 7, i;
    for (i = 0; i < 8; i++)
        cmd[i] = *(volatile u8 *)io_reg(0x040001a8 + i);
    len = size == 0 ? 0 : size == 7 ? 4 : 0x100u << size;
    pos = 0;
    switch (cmd[0]) {
    case 0xb7: { /* data read: 0x200-byte aligned windows of the ROM */
        u32 addr = (u32)cmd[1] << 24 | (u32)cmd[2] << 16 | (u32)cmd[3] << 8 | cmd[4];
        rom_read(addr, buf, len);
        break;
    }
    case 0xb8: case 0x90: /* chip ID */
        for (i = 0; i < len; i += 4)
            *(u32 *)(buf + i) = 0x00001fc2u;
        break;
    case 0x00: /* header */
        rom_read(0, buf, len);
        break;
    default:
        memset(buf, 0xff, len);
        break;
    }
    hw_card_reads++;
    if (hw_iolog)
        fprintf(stderr, "card: command %02x %02x%02x%02x%02x, %u bytes\n", cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], len);
    if (len == 0)
        R32(0x040001a4) = ctrl & ~0x80800000u;
    else
        R32(0x040001a4) = ctrl | 0x00800000u;  /* busy, data word ready */
}

int hw_card_pending(void) { return pos < len; }

/* A read of 0x04100010: next data word; the end of the transfer clears busy and raises IRQ 19 if enabled. */
u32 hw_card_data(void)
{
    u32 v = 0xffffffffu;
    if (pos < len) {
        memcpy(&v, buf + pos, 4);
        pos += 4;
        if (pos >= len) {
            R32(0x040001a4) &= ~0x80800000u;
            if (R16(0x040001a0) & 0x4000)
                R32(0x04000214) |= 1u << 19;
        }
    }
    return v;
}
