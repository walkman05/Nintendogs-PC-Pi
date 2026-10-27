#include <stdio.h>
#include "platform_mem.h"

/* Host entry point. Milestone 0: map the DS memory layout and exit; the game loop arrives in M3-M5. */
int main(int argc, char **argv)
{
    int i;
    (void)argc;
    (void)argv;
    printf("nintendogs_pc: %d-bit build\n", (int)(sizeof(void *) * 8));
    if (platform_map_ds_memory() != 0) {
        fprintf(stderr, "DS memory map failed (a 32-bit build is required)\n");
        return 1;
    }
    for (i = 0; i < ds_region_count; i++)
        printf("  %-12s 0x%08lx +0x%lx\n", ds_regions[i].name, ds_regions[i].base, ds_regions[i].size);
    platform_unmap_ds_memory();
    printf("ok\n");
    return 0;
}
