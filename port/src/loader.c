/* Milestone 3: put the game's images into the mapped DS memory and make its data point at the compiled C.
 * Inputs come from extracted/ (made from the user's ROM by port/tools/extract_rom.py):
 *   host/modules.txt   name file base bss_start bss_end ctor_start ctor_end
 *   host/fix_<m>.txt   from to target-module   (words in module m's data that hold a DS address)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "loader.h"

#define MAX_MODULES 40
#define OV_REGION_LO 0x0215c000u

typedef struct {
    char name[16];
    char file[96];
    unsigned int base, bss_start, bss_end, ctor_start, ctor_end;
} Module;

typedef struct {
    unsigned int from, to;
    unsigned int overlays;  /* bit N: the target is code in overlay N (several bits: whichever is loaded) */
    int target;             /* -1 main, -2 itcm, -3 dtcm, >=0 one overlay, -4 several overlays, -5 none */
} Fix;

static char data_dir[260];
static Module modules[MAX_MODULES];
static int module_count;
static Fix *main_fixes;     /* main/itcm/dtcm data pointing into the overlay region, re-patched on each load */
static int main_fix_count;
static int loaded_overlay[32];
static unsigned int unresolved;

void (*ndogs_host_func(int module, unsigned int ds_addr))(void)
{
    int lo = 0, hi = host_functab_count - 1;
    ds_addr &= ~1u;  /* Thumb bit */
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        const HostFunc *h = &host_functab[mid];
        if (h->module == module && h->ds_addr == ds_addr)
            return h->fn;
        if (h->module < module || (h->module == module && h->ds_addr < ds_addr))
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return 0;
}

static int parse_target(const char *s, unsigned int *mask)
{
    *mask = 0;
    if (!strcmp(s, "main"))
        return -1;
    if (!strcmp(s, "itcm"))
        return -2;
    if (!strcmp(s, "dtcm"))
        return -3;
    if (!strncmp(s, "overlay(", 8) || !strncmp(s, "overlays(", 9)) {
        const char *p = strchr(s, '(') + 1;
        int n = 0;
        while (*p && *p != ')') {
            int id = atoi(p);
            *mask |= 1u << id;
            n++;
            while (*p && *p != ',' && *p != ')')
                p++;
            if (*p == ',')
                p++;
        }
        if (n == 1) {
            int id = 0;
            while (!(*mask & (1u << id)))
                id++;
            return id;
        }
        return -4;
    }
    return -5;
}

static int read_file(const char *rel, void *dst, unsigned int max)
{
    char path[400];
    FILE *f;
    long n;
    snprintf(path, sizeof path, "%s/rom/%s", data_dir, rel);
    f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "loader: cannot open %s\n", path);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if ((unsigned long)n > max) {
        fprintf(stderr, "loader: %s is larger than its region\n", path);
        fclose(f);
        return -1;
    }
    if (fread(dst, 1, (size_t)n, f) != (size_t)n) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return (int)n;
}

static Module *module_named(const char *name)
{
    int i;
    for (i = 0; i < module_count; i++)
        if (!strcmp(modules[i].name, name))
            return &modules[i];
    return 0;
}

/* Highest function address of a module: anything above is data (rodata/data follow the code). */
static unsigned int code_end(int module)
{
    static unsigned int cache[2];
    unsigned int end = 0;
    int i;
    if (cache[module == -2])
        return cache[module == -2];
    for (i = 0; i < host_functab_count; i++)
        if (host_functab[i].module == module && host_functab[i].ds_addr > end)
            end = host_functab[i].ds_addr;
    cache[module == -2] = end;
    return end;
}

/* Rewrite one data word: DS code address -> host function, when the target's code is known. */
static void patch(const Fix *fx, int overlay_now)
{
    unsigned int *slot = (unsigned int *)fx->from;
    void (*fn)(void) = 0;
    if (fx->target == -1)
        fn = ndogs_host_func(-1, fx->to);
    else if (fx->target == -2)
        fn = ndogs_host_func(-2, fx->to);
    else if (fx->target >= 0 || fx->target == -4)
        fn = (overlay_now >= 0 && (fx->overlays & (1u << overlay_now))) ? ndogs_host_func(overlay_now, fx->to) : 0;
    else
        return;  /* pointer to data: the address is valid as it is */
    if (fn)
        *slot = (unsigned int)fn;
    else if ((fx->target == -1 || fx->target == -2) && fx->to <= code_end(fx->target))
        unresolved++;  /* inside code but not a known function start; pointers to data stay as they are */
}

static int apply_fixes(const char *module, int overlay_now, int keep_overlay_refs)
{
    char path[400], tgt[160];
    unsigned int from, to;
    FILE *f;
    snprintf(path, sizeof path, "%s/host/fix_%s.txt", data_dir, module);
    f = fopen(path, "r");
    if (!f)
        return -1;
    while (fscanf(f, "%x %x %159s", &from, &to, tgt) == 3) {
        Fix fx;
        fx.from = from;
        fx.to = to;
        fx.target = parse_target(tgt, &fx.overlays);
        if (fx.target >= 0 || fx.target == -4) {
            if (keep_overlay_refs) {
                Fix *n = realloc(main_fixes, sizeof(Fix) * (main_fix_count + 1));
                if (!n)
                    break;
                main_fixes = n;
                main_fixes[main_fix_count++] = fx;
                continue;
            }
        }
        patch(&fx, overlay_now);
    }
    fclose(f);
    return 0;
}

int ndogs_load_main(const char *dir)
{
    char path[400];
    FILE *f;
    int i;
    snprintf(data_dir, sizeof data_dir, "%s", dir);
    snprintf(path, sizeof path, "%s/host/modules.txt", dir);
    f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "loader: %s missing - run port/tools/extract_rom.py first\n", path);
        return -1;
    }
    while (module_count < MAX_MODULES) {
        Module *m = &modules[module_count];
        if (fscanf(f, "%15s %95s %x %x %x %x %x", m->name, m->file, &m->base, &m->bss_start, &m->bss_end,
                   &m->ctor_start, &m->ctor_end) != 7)
            break;
        module_count++;
    }
    fclose(f);
    for (i = 0; i < 3; i++) {
        Module *m = module_named(i == 0 ? "main" : i == 1 ? "itcm" : "dtcm");
        unsigned int max = i == 0 ? 0x0215c000u - 0x02000000u : i == 1 ? 0x8000u : 0x4000u;
        if (!m || read_file(m->file, (void *)m->base, max) < 0)
            return -1;
        if (m->bss_end > m->bss_start)
            memset((void *)m->bss_start, 0, m->bss_end - m->bss_start);
    }
    /* boot-time state the DS firmware leaves in the system area: the cartridge header at 0x027ffe00 */
    {
        char hp[400];
        FILE *hf;
        extern int ndogs_full;
        snprintf(hp, sizeof hp, "%s/host/full_header.bin", data_dir);  /* port/tools/make_full_rom.py */
        hf = ndogs_full ? fopen(hp, "rb") : 0;
        if (!hf) {
            snprintf(hp, sizeof hp, "%s/host/header.bin", data_dir);
            hf = fopen(hp, "rb");
        }
        if (hf) {
            if (fread((void *)0x027ffe00u, 1, 0x170, hf) != 0x170)
                fprintf(stderr, "loader: short cartridge header\n");
            fclose(hf);
        } else
            fprintf(stderr, "loader: %s missing - rerun port/tools/extract_rom.py\n", hp);
    }
    apply_fixes("main", -1, 1);
    apply_fixes("itcm", -1, 1);
    apply_fixes("dtcm", -1, 1);
    printf("loader: main, itcm, dtcm loaded; %d data pointers into overlays deferred, %u unresolved\n",
           main_fix_count, unresolved);
    return 0;
}

int ndogs_load_overlay(int id)
{
    char name[16];
    Module *m;
    unsigned int *p;
    int i, n;
    snprintf(name, sizeof name, "ov%03d", id);
    m = module_named(name);
    if (!m)
        return 0;
    n = read_file(m->file, (void *)m->base, 0x02400000u - m->base);
    if (n < 0)
        return 0;
    if (m->bss_end > m->bss_start)
        memset((void *)m->bss_start, 0, m->bss_end - m->bss_start);
    apply_fixes(name, id, 0);
    for (i = 0; i < main_fix_count; i++)
        if (main_fixes[i].overlays & (1u << id))
            patch(&main_fixes[i], id);
    loaded_overlay[id] = 1;
    for (p = (unsigned int *)m->ctor_start; p < (unsigned int *)m->ctor_end; p++) {
        if (*p == 0)
            continue;
        if (*p >= 0x01ff8000u && *p < 0x08000000u)  /* still a DS address: no compiled function for it */
            fprintf(stderr, "loader: %s constructor 0x%08x has no host function\n", name, *p);
        else
            ((void (*)(void))*p)();
    }
    if (getenv("NDOGS_STATUS"))
        fprintf(stderr, "loader: overlay %d loaded\n", id);
    return 1;
}

int ndogs_unload_overlay(int id)
{
    if (id >= 0 && id < 32)
        loaded_overlay[id] = 0;
    return 1;
}
