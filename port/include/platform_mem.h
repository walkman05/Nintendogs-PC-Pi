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

#endif
