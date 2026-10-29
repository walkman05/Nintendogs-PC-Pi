/* VRAM banks A-I and their mapping (VRAMCNT). The DS memory map is flat on the host, so a bank mapped to a CPU
 * address (LCDC, engine BG/OBJ areas) keeps its contents right there, where the game reads and writes them. A bank
 * mapped to a role the CPU can't see (3D textures, texture palettes, extended palettes) lives in its own storage.
 * On a mapping change the bank is moved: copied out of its old CPU window into storage, and into the new one. */
#include <string.h>
#include "ds_video.h"

typedef struct {
    u32 size;
    u32 lcdc;        /* LCDC address */
    u8 cnt;          /* last VRAMCNT value */
    u32 cpu;         /* CPU address the bank currently occupies, 0 if none */
    int role;        /* non-CPU role, or -1 */
    u32 role_off;    /* offset of the bank inside that role */
    u8 *store;
} Bank;

static u8 store_a[0x20000], store_b[0x20000], store_c[0x20000], store_d[0x20000], store_e[0x10000];
static u8 store_f[0x4000], store_g[0x4000], store_h[0x8000], store_i[0x4000];

static Bank banks[9] = {
    { 0x20000, 0x06800000, 0, 0, -1, 0, store_a }, { 0x20000, 0x06820000, 0, 0, -1, 0, store_b },
    { 0x20000, 0x06840000, 0, 0, -1, 0, store_c }, { 0x20000, 0x06860000, 0, 0, -1, 0, store_d },
    { 0x10000, 0x06880000, 0, 0, -1, 0, store_e }, { 0x4000, 0x06890000, 0, 0, -1, 0, store_f },
    { 0x4000, 0x06894000, 0, 0, -1, 0, store_g }, { 0x8000, 0x06898000, 0, 0, -1, 0, store_h },
    { 0x4000, 0x068a0000, 0, 0, -1, 0, store_i },
};

/* CPU base of each CPU-visible role */
static const u32 role_base[VRAM_ROLE_COUNT] = {
    0x06000000, 0x06400000, 0x06200000, 0x06600000, 0, 0, 0, 0, 0, 0, 0x06800000,
};
static const u32 role_size[VRAM_ROLE_COUNT] = {
    0x80000, 0x40000, 0x20000, 0x20000, 0x80000, 0x18000, 0x8000, 0x2000, 0x8000, 0x2000, 0xa4000,
};

/* Where VRAMCNT puts a bank: a CPU address, or a hidden role + offset. */
static void decode(int b, u8 cnt, u32 *cpu, int *role, u32 *roff)
{
    u32 mst = cnt & 7, ofs = (cnt >> 3) & 3;
    *cpu = 0;
    *role = -1;
    *roff = 0;
    if (!(cnt & 0x80))
        return;
    if (mst == 0) {
        *cpu = banks[b].lcdc;
        return;
    }
    switch (b) {
    case 0: case 1: case 2: case 3: /* A-D */
        if (mst == 1)
            *cpu = 0x06000000 + 0x20000 * ofs;
        else if (mst == 2 && b < 2)
            *cpu = 0x06400000 + 0x20000 * (ofs & 1);
        else if (mst == 3)
            *role = VRAM_TEX, *roff = 0x20000 * ofs;
        else if (mst == 4 && b == 2)
            *cpu = 0x06200000;
        else if (mst == 4 && b == 3)
            *cpu = 0x06600000;
        break; /* C/D MST 2: ARM7 memory, not visible here */
    case 4: /* E */
        if (mst == 1)
            *cpu = 0x06000000;
        else if (mst == 2)
            *cpu = 0x06400000;
        else if (mst == 3)
            *role = VRAM_TEXPAL;
        else if (mst == 4)
            *role = VRAM_BGEXTPAL_A;
        break;
    case 5: case 6: { /* F, G */
        u32 o = 0x4000 * (ofs & 1) + 0x10000 * (ofs >> 1);
        if (mst == 1)
            *cpu = 0x06000000 + o;
        else if (mst == 2)
            *cpu = 0x06400000 + o;
        else if (mst == 3)
            *role = VRAM_TEXPAL, *roff = 0x4000 * ((ofs & 1) + (ofs >> 1) * 4);
        else if (mst == 4)
            *role = VRAM_BGEXTPAL_A, *roff = (ofs & 1) ? 0x4000 : 0;
        else if (mst == 5)
            *role = VRAM_OBJEXTPAL_A;
        break;
    }
    case 7: /* H */
        if (mst == 1)
            *cpu = 0x06200000;
        else if (mst == 2)
            *role = VRAM_BGEXTPAL_B;
        break;
    case 8: /* I */
        if (mst == 1)
            *cpu = 0x06208000;
        else if (mst == 2)
            *cpu = 0x06600000;
        else if (mst == 3)
            *role = VRAM_OBJEXTPAL_B;
        break;
    }
}

void vram_cnt_written(int b, u8 value)
{
    Bank *k;
    u32 cpu, roff;
    int role;
    if (b < 0 || b > 8)
        return;
    k = &banks[b];
    if (value == k->cnt)
        return;
    if (k->cpu)  /* take the bank's live contents out of its old CPU window */
        memcpy(k->store, (void *)k->cpu, k->size);
    decode(b, value, &cpu, &role, &roff);
    if (cpu)
        memcpy((void *)cpu, k->store, k->size);
    k->cnt = value;
    k->cpu = cpu;
    k->role = role;
    k->role_off = roff;
}

void vram_init(void)
{
    int i;
    for (i = 0; i < 9; i++) {
        banks[i].cnt = 0;
        banks[i].cpu = 0;
        banks[i].role = -1;
    }
}

/* Byte at <offset> of <role>: CPU-visible roles read the DS memory map, hidden roles the banks mapped there. */
static const u8 *locate(int role, u32 offset)
{
    int i;
    if (role < 0 || role >= VRAM_ROLE_COUNT || offset >= role_size[role])
        return 0;
    if (role_base[role])
        return (const u8 *)(role_base[role] + offset);
    for (i = 0; i < 9; i++)
        if (banks[i].role == role && offset - banks[i].role_off < banks[i].size)
            return banks[i].store + (offset - banks[i].role_off);
    return 0;
}

u8 vram_read8(int role, u32 offset)
{
    const u8 *p = locate(role, offset);
    return p ? *p : 0;
}

u16 vram_read16(int role, u32 offset)
{
    const u8 *p = locate(role, offset & ~1u);
    return p ? *(const u16 *)p : 0;
}

u32 vram_read32(int role, u32 offset)
{
    const u8 *p = locate(role, offset & ~3u);
    return p ? *(const u32 *)p : 0;
}

/* A CPU-visible role's memory (and its size), 0 for a hidden role */
const u8 *vram_cpu_view(int role, u32 *size)
{
    if (role < 0 || role >= VRAM_ROLE_COUNT || !role_base[role])
        return 0;
    if (size)
        *size = role_size[role];
    return (const u8 *)role_base[role];
}

/* Bank A-D contents for display capture: where the bank currently lives (its CPU window, normally LCDC). */
u16 *vram_bank_ptr(int bank)
{
    if (bank < 0 || bank > 3)
        return 0;
    return (u16 *)(banks[bank].cpu ? (u8 *)banks[bank].cpu : banks[bank].store);
}

/* A hidden role's whole contents as one flat image (unmapped parts zero): the 3D engine reads textures and
 * texture palettes from this copy, made once per frame, instead of locating the bank for every texel. */
void vram_flatten(int role, u8 *dst, u32 size)
{
    int i;
    memset(dst, 0, size);
    for (i = 0; i < 9; i++)
        if (banks[i].role == role && banks[i].role_off < size) {
            u32 n = banks[i].size < size - banks[i].role_off ? banks[i].size : size - banks[i].role_off;
            memcpy(dst + banks[i].role_off, banks[i].store, n);
        }
}

/* Which bank holds byte <offset> of a CPU-visible role, and where inside it (-1 if none). */
int vram_bank_at(int role, u32 offset, u32 *bank_off)
{
    u32 a;
    int i;
    if (role < 0 || role >= VRAM_ROLE_COUNT || !role_base[role] || offset >= role_size[role])
        return -1;
    a = role_base[role] + offset;
    for (i = 0; i < 9; i++)
        if (banks[i].cpu && a - banks[i].cpu < banks[i].size) {
            *bank_off = a - banks[i].cpu;
            return i;
        }
    return -1;
}
