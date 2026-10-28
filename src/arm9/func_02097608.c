#include "ctx.h"

typedef struct Entry {
    u8 pad0[0x10];
    void *ptr;
} Entry;

typedef struct Ctx {
    s32 active;
    s16 idx;
    s16 count;
    s16 state;
    s16 pad;
    Entry entries[1];
} Ctx;

typedef struct Desc {
    u32 unk0;
    u32 size;
    u32 unk8;
    u32 unkc;
    u8 *bufA;
    u8 *bufB;
} Desc;

typedef struct Footer {
    u32 magic;
    u16 sum;
    u16 pad;
} Footer;

extern Ctx *data_02159034;
extern Desc *func_020978d0(s32 a, s32 b);

static inline void SetChecksum(u8 *buf, u32 size)
{
    u32 n;
    Footer *f;
    u16 sum = 0;
    u32 i;

    *(u32 *)buf = 0;
    f = (Footer *)(buf + size) - 1;
    f->magic = 0x444f473f;
    n = size - 4;
    for (i = 0; i < n; i++) {
        sum += buf[i];
    }
    f->sum = sum;
}

#ifdef NDOGS_PC
/* full-game mode: where the retail game starts writing an entry's copy A (at its chip address) or copy B (0x20000
 * higher) to the save chip; entry words: chip address, copy A, copy B, length (port/src/save.c) */
extern void ndogs_save_write(u32 addr, const void *src, u32 len);
static inline void save_write(void *e, int copy_b)
{
    u32 *w = (u32 *)e;
    if (ndogs_full)
        ndogs_save_write(w[0] + (copy_b ? 0x20000 : 0), (void *)w[copy_b ? 2 : 1], w[3]);
}
#define SAVE_WRITE(e, b) save_write(e, b)
#else
#define SAVE_WRITE(e, b)
#endif

BOOL func_02097608(void)
{
    Ctx *ctx = data_02159034;
    Desc *d;

    if (ctx->active == 0) {
        return TRUE;
    }
    if (ctx->count > 0) {
        switch (ctx->state) {
        case 0: {
            Entry *e = &ctx->entries[ctx->idx];
            ctx->state = 1;
            if (e->ptr != NULL) {
                d = func_020978d0(0, 0);
                SetChecksum(d->bufA, d->size);
            }
            SAVE_WRITE(e, 0);
            break;
        }
        case 1:
            ctx->idx++;
            if (data_02159034->idx >= data_02159034->count) {
                data_02159034->state = 2;
                data_02159034->idx = 0;
                if (data_02159034->entries[data_02159034->idx].ptr != NULL) {
                    d = func_020978d0(0, 0);
                    SetChecksum(d->bufB, d->size);
                }
                SAVE_WRITE(&data_02159034->entries[data_02159034->idx], 1);
            } else {
                if (data_02159034->entries[data_02159034->idx].ptr != NULL) {
                    d = func_020978d0(0, 0);
                    SetChecksum(d->bufA, d->size);
                }
                SAVE_WRITE(&data_02159034->entries[data_02159034->idx], 0);
            }
            break;
        case 2:
            ctx->idx++;
            if (data_02159034->idx >= data_02159034->count) {
                data_02159034->state = 3;
            } else {
                if (data_02159034->entries[data_02159034->idx].ptr != NULL) {
                    d = func_020978d0(0, 0);
                    SetChecksum(d->bufB, d->size);
                }
                SAVE_WRITE(&data_02159034->entries[data_02159034->idx], 1);
            }
            break;
        case 3:
            break;
        }
    }
    if (data_02159034->state == 3) {
        data_02159034->active = 0;
    }
    return data_02159034->idx == data_02159034->count;
}
