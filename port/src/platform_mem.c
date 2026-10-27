#include <stdio.h>
#include "platform_mem.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif
#endif

const DsRegion ds_regions[] = {
    { 0x01ff0000ul, 0x00010000ul, "ITCM" },          /* 0x01ff8000, rounded to 64 KiB for VirtualAlloc */
    { 0x02000000ul, 0x00400000ul, "main RAM" },
    { 0x027c0000ul, 0x00010000ul, "DTCM" },
    { 0x027f0000ul, 0x00010000ul, "system area" },   /* 0x027ff000-0x027fffff: SDK shared work area */
    { 0x037f0000ul, 0x00010000ul, "shared WRAM" },
    { 0x04000000ul, 0x01000000ul, "I/O (as RAM)" },   /* milestone 3 placeholder: the hardware layer replaces it */
    { 0x05000000ul, 0x00010000ul, "palette" },
    { 0x06000000ul, 0x00900000ul, "VRAM" },
    { 0x07000000ul, 0x00010000ul, "OAM" },
};
const int ds_region_count = sizeof(ds_regions) / sizeof(ds_regions[0]);

static void *map_fixed(unsigned long base, unsigned long size)
{
#ifdef _WIN32
    return VirtualAlloc((void *)base, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void *p = mmap((void *)base, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    return p == MAP_FAILED ? NULL : p;
#endif
}

static void unmap(unsigned long base, unsigned long size)
{
#ifdef _WIN32
    (void)size;
    VirtualFree((void *)base, 0, MEM_RELEASE);
#else
    munmap((void *)base, size);
#endif
}

int platform_map_ds_memory(void)
{
    int i, bad = 0;
    for (i = 0; i < ds_region_count; i++) {
        void *p = map_fixed(ds_regions[i].base, ds_regions[i].size);
        if (p != (void *)ds_regions[i].base) {
            fprintf(stderr, "could not map %s at 0x%08lx\n", ds_regions[i].name, ds_regions[i].base);
            bad = 1;
        }
    }
    return bad;
}

void platform_unmap_ds_memory(void)
{
    int i;
    for (i = 0; i < ds_region_count; i++)
        unmap(ds_regions[i].base, ds_regions[i].size);
}
