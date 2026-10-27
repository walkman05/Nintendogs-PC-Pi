/* Self-test: can this process map every DS region at its real address? (Runs in CI without the game.) */
#include <stdio.h>
#include "platform_mem.h"

int main(void)
{
    int i;
    printf("memtest: %d-bit build\n", (int)(sizeof(void *) * 8));
    if (platform_map_ds_memory() != 0)
        return 1;
    for (i = 0; i < ds_region_count; i++) {
        volatile unsigned int *p = (volatile unsigned int *)ds_regions[i].base;
        *p = 0x12345678u;
        if (*p != 0x12345678u)
            return 1;
        printf("  %-14s 0x%08lx +0x%lx ok\n", ds_regions[i].name, ds_regions[i].base, ds_regions[i].size);
    }
    platform_unmap_ds_memory();
    return 0;
}
