#ifndef PLATFORM_MEM_H
#define PLATFORM_MEM_H

/* Reserves the DS memory regions at their real addresses so the decompiled code's fixed pointers work. */

typedef struct {
    unsigned long base;
    unsigned long size;
    const char *name;
} DsRegion;

/* Main RAM, shared WRAM, DTCM and the like (I/O at 0x04000000 is handled by the hardware layer, not mapped). */
extern const DsRegion ds_regions[];
extern const int ds_region_count;

/* Returns 0 when every region is mapped at its exact address. */
int platform_map_ds_memory(void);
void platform_unmap_ds_memory(void);
/* When the map fails: run the exe again with the DS ranges reserved up front; returns its exit code, or -1. */
int platform_relaunch_reserved(void);

/* The two 64 KiB I/O windows holding hardware registers (0x04000000, 0x04100000) are shared memory mapped twice:
 * at the DS address, where register pages are kept inaccessible so the game's accesses trap, and at an alias that
 * is always accessible, through which the register model and host code read and write registers. */
#define IO_WINDOW 0x10000u
extern unsigned char *platform_io_alias[2];
static inline volatile void *io_reg(unsigned int addr)
{
    if (addr - 0x04000000u < IO_WINDOW)
        return platform_io_alias[0] + (addr - 0x04000000u);
    if (addr - 0x04100000u < IO_WINDOW)
        return platform_io_alias[1] + (addr - 0x04100000u);
    return (volatile void *)addr;
}

#endif
