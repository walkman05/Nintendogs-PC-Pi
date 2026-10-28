/* First-run setup: builds the data folder from the user's own ROMs, so the exe needs neither Python nor dsd.
 * The same output as port/tools/extract_rom.py + make_full_rom.py (byte for byte):
 *   rom/arm9/arm9.bin, itcm.bin, dtcm.bin   the ARM9 binary (BLZ-decompressed) and its autoload blocks
 *   rom/arm9_overlays/ovNNN.bin             the overlays (decompressed)
 *   host/header.bin, rom_path.txt           the cartridge header; where the demo ROM is (cartridge reads)
 *   host/modules.txt, fix_*.txt             compiled into the exe from dsd's analysis (gen_hosttabs.py); they
 *                                           only fit the one demo ROM, hence the CRC check
 *   host/full.nds, full_header.bin          with the retail cartridge: the full-game image (make_full_rom.py)
 * ROMs are looked for on the command line, next to the exe and in the current folder. */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir_one(p) _mkdir(p)
#else
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#define mkdir_one(p) mkdir(p, 0755)
#endif

typedef unsigned char u8;
typedef unsigned int u32;

#define DEMO_CRC 0x8a03681au  /* Nintendogs (Europe) (Demo) (Kiosk), MD5 a190eca6a61b3c69ce48f076d2445eb8 */
#define CARD_SIZE (64u << 20)

extern const char *const ndogs_host_tables[][2];

static u32 rd32(const u8 *p) { return p[0] | p[1] << 8 | p[2] << 16 | (u32)p[3] << 24; }
static u32 rd16(const u8 *p) { return p[0] | p[1] << 8; }
static void wr32(u8 *p, u32 v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
static void wr16(u8 *p, u32 v) { p[0] = v; p[1] = v >> 8; }

/* growable byte buffer */
typedef struct { u8 *p; u32 n, cap; } Buf;
static void buf_add(Buf *b, const void *src, u32 n)
{
    if (b->n + n > b->cap) {
        b->cap = (b->n + n) * 2 + 256;
        b->p = realloc(b->p, b->cap);
    }
    if (src)
        memcpy(b->p + b->n, src, n);
    else
        memset(b->p + b->n, 0, n);
    b->n += n;
}
static void buf_byte(Buf *b, int c) { u8 v = (u8)c; buf_add(b, &v, 1); }
static void buf_u32(Buf *b, u32 v) { u8 t[4]; wr32(t, v); buf_add(b, t, 4); }
static void buf_pad(Buf *b, u32 align, int fill)
{
    while (b->n % align)
        buf_byte(b, fill);
}

static u32 crc32(const u8 *p, u32 n)
{
    u32 c = 0xffffffffu, k;
    while (n--) {
        c ^= *p++;
        for (k = 0; k < 8; k++)
            c = c >> 1 ^ (0xedb88320u & -(c & 1));
    }
    return ~c;
}

static u8 *load(const char *path, u32 *len)
{
    FILE *f = fopen(path, "rb");
    u8 *d;
    long n;
    if (!f)
        return 0;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    d = n > 0 ? malloc((size_t)n) : 0;
    if (!d || fread(d, 1, (size_t)n, f) != (size_t)n) {
        free(d);
        fclose(f);
        return 0;
    }
    fclose(f);
    *len = (u32)n;
    return d;
}

static void mkdirs(const char *path)
{
    char t[1024];
    char *p;
    snprintf(t, sizeof t, "%s", path);
    for (p = t + 1; *p; p++)
        if (*p == '/' || *p == '\\') {
            char c = *p;
            *p = 0;
            mkdir_one(t);
            *p = c;
        }
    mkdir_one(t);
}

static int save(const char *dir, const char *rel, const void *data, u32 n)
{
    char path[1024];
    FILE *f;
    char *slash;
    snprintf(path, sizeof path, "%s/%s", dir, rel);
    slash = strrchr(path, '/');
    *slash = 0;
    mkdirs(path);
    *slash = '/';
    f = fopen(path, "wb");
    if (!f || fwrite(data, 1, n, f) != n) {
        if (f)
            fclose(f);
        fprintf(stderr, "setup: cannot write %s\n", path);
        return -1;
    }
    return fclose(f) ? -1 : 0;
}

static int exists(const char *dir, const char *rel)
{
    char path[1024];
    FILE *f;
    snprintf(path, sizeof path, "%s/%s", dir, rel);
    f = fopen(path, "rb");
    if (f)
        fclose(f);
    return f != 0;
}

/* backward LZ (the ARM9 binary and overlays): the footer gives the compressed length and the size gain */
static u8 *blz(const u8 *in, u32 n, u32 *out_n)
{
    u32 hdr = in[n - 5], enc = rd32(in + n - 8) & 0xffffff, inc = rd32(in + n - 4);
    u32 pak = n - hdr, raw = n + inc, start = n - enc;
    u8 *o = malloc(n + inc + 1);
    memcpy(o, in, n);
    *out_n = n + inc;
    if (!inc)
        return o;
    while (pak > start) {
        int flags = o[--pak], i;
        for (i = 0; i < 8 && pak > start; i++)
            if (flags & 0x80 >> i) {
                u32 pos = o[pak - 1] << 8 | o[pak - 2], len;
                pak -= 2;
                len = (pos >> 12) + 3;
                pos = (pos & 0xfff) + 3;
                while (len--) {
                    raw--;
                    o[raw] = o[raw + pos];
                }
            } else
                o[--raw] = o[--pak];
    }
    return o;
}

static int extract_demo(const u8 *rom, const char *rom_path, const char *dir)
{
    u32 a9o = rd32(rom + 0x20), a9r = rd32(rom + 0x28), a9s = rd32(rom + 0x2c);
    u32 ovo = rd32(rom + 0x50), ovs = rd32(rom + 0x54), fat = rd32(rom + 0x48);
    u8 *a9 = malloc(a9s), *t;
    u32 n = a9s, i, p, al, ale, ast, cend, o;
    char path[1100];
    int rc = 0;
    memcpy(a9, rom + a9o, a9s);
    /* the SDK's module parameters sit just before the "nitrocode" magic */
    for (p = 0; p + 8 <= n && memcmp(a9 + p, "\x21\x06\xc0\xde\xde\xc0\x06\x21", 8); p++)
        ;
    if (p + 8 > n || p < 0x1c)
        return -1;
    p -= 0x1c;
    al = rd32(a9 + p), ale = rd32(a9 + p + 4), ast = rd32(a9 + p + 8), cend = rd32(a9 + p + 0x14);
    if (cend) {
        u32 c = cend - a9r, dn;
        u8 *d = blz(a9, c, &dn);
        t = malloc(dn + n - c);
        memcpy(t, d, dn);
        memcpy(t + dn, a9 + c, n - c);
        free(d);
        free(a9);
        a9 = t;
        n = dn + n - c;
        wr32(a9 + p + 0x14, 0);
    }
    rc |= save(dir, "rom/arm9/arm9.bin", a9, ast - a9r);
    for (i = al, o = ast - a9r; i < ale; i += 12) {
        u32 addr = rd32(a9 + i - a9r), size = rd32(a9 + i - a9r + 4);
        rc |= save(dir, addr < 0x02000000u ? "rom/arm9/itcm.bin" : "rom/arm9/dtcm.bin", a9 + o, size);
        o += size;
    }
    free(a9);
    for (i = 0; i < ovs / 32; i++) {
        const u8 *e = rom + ovo + i * 32;
        u32 fid = rd32(e + 24), flags = rd32(e + 28);
        u32 s = rd32(rom + fat + fid * 8), end = rd32(rom + fat + fid * 8 + 4), dn = end - s;
        u8 *d;
        if (flags >> 24 & 1)
            d = blz(rom + s, flags & 0xffffff, &dn);
        else {
            d = malloc(dn);
            memcpy(d, rom + s, dn);
        }
        snprintf(path, sizeof path, "rom/arm9_overlays/ov%03u.bin", rd32(e));
        rc |= save(dir, path, d, dn);
        free(d);
    }
    for (i = 0; ndogs_host_tables[i][0]; i++) {
        snprintf(path, sizeof path, "host/%s", ndogs_host_tables[i][0]);
        rc |= save(dir, path, ndogs_host_tables[i][1], (u32)strlen(ndogs_host_tables[i][1]));
    }
    rc |= save(dir, "host/header.bin", rom, 0x200);
    snprintf(path, sizeof path, "%s\n", rom_path);
    rc |= save(dir, "host/rom_path.txt", path, (u32)strlen(path));
    return rc;
}

/* ---- the full-game image (port/tools/make_full_rom.py has the reasoning) ---- */

typedef struct {
    char *path;
    const u8 *data;
    u32 len;
    u32 start, end;   /* where it is in the demo ROM */
    int changed;      /* data is not the demo's copy */
} File;

typedef struct {
    char *path;       /* "" for the root, else "a/b/" */
    char **subs, **files;
    int nsubs, nfiles;
    u32 id;
} Dir;

typedef struct {
    File *f;
    int n;
    Dir *d;
    int nd;
    u32 first_named;
} Fs;

static void *grow(void *p, int n, size_t sz) { return (n & (n - 1)) == 0 ? realloc(p, (n ? n * 2 : 1) * sz) : p; }
static char *str_cat(const char *a, const char *b, u32 bn, const char *c)
{
    size_t an = strlen(a), cn = strlen(c);
    char *s = malloc(an + bn + cn + 1);
    memcpy(s, a, an);
    memcpy(s + an, b, bn);
    memcpy(s + an + bn, c, cn + 1);
    return s;
}

static File *fs_file(Fs *fs, const char *path)
{
    int i;
    for (i = 0; i < fs->n; i++)
        if (!strcmp(fs->f[i].path, path))
            return &fs->f[i];
    return 0;
}

static void fs_walk(Fs *fs, const u8 *rom, u32 fnt, u32 fat, u32 dir_id, const char *path)
{
    const u8 *e = rom + fnt + (dir_id & 0xfff) * 8;
    const u8 *p = rom + fnt + rd32(e);
    u32 fid = rd16(e + 4);
    int di = fs->nd;
    fs->d = grow(fs->d, fs->nd, sizeof *fs->d);
    memset(&fs->d[fs->nd], 0, sizeof *fs->d);
    fs->d[fs->nd].path = str_cat(path, "", 0, "");
    fs->d[fs->nd++].id = dir_id;
    if (fid < fs->first_named)
        fs->first_named = fid;
    while (*p) {
        u32 len = *p & 0x7f;
        int sub = *p & 0x80;
        const char *name = (const char *)p + 1;
        Dir *d = &fs->d[di];
        p += 1 + len;
        if (sub) {
            d->subs = grow(d->subs, d->nsubs, sizeof *d->subs);
            d->subs[d->nsubs++] = str_cat("", name, len, "");
            {
                char *child = str_cat(path, name, len, "/");
                u32 cid = rd16(p);
                p += 2;
                fs_walk(fs, rom, fnt, fat, cid, child);
                free(child);
            }
        } else {
            File *f;
            d->files = grow(d->files, d->nfiles, sizeof *d->files);
            d->files[d->nfiles++] = str_cat("", name, len, "");
            fs->f = grow(fs->f, fs->n, sizeof *fs->f);
            f = &fs->f[fs->n++];
            memset(f, 0, sizeof *f);
            f->path = str_cat(path, name, len, "");
            f->start = rd32(rom + fat + fid * 8);
            f->end = rd32(rom + fat + fid * 8 + 4);
            f->data = rom + f->start;
            f->len = f->end - f->start;
            fid++;
        }
    }
}

static void read_fs(Fs *fs, const u8 *rom)
{
    memset(fs, 0, sizeof *fs);
    fs->first_named = 0xffffffffu;
    fs_walk(fs, rom, rd32(rom + 0x40), rd32(rom + 0x48), 0xf000, "");
}

/* BMG messages */
typedef struct {
    u32 off[8], size[8];
    u32 magic[8];
    int n;
} Secs;

static u32 magic4(const char *m) { return rd32((const u8 *)m); }

static void bmg_sections(const u8 *d, u32 len, Secs *s)
{
    u32 o = 0x20, i, cnt = rd32(d + 12);
    s->n = 0;
    for (i = 0; i < cnt && s->n < 8; i++) {
        if (o + 8 > len)
            break;
        s->magic[s->n] = rd32(d + o);
        s->off[s->n] = o;
        s->size[s->n] = rd32(d + o + 4);
        o += s->size[s->n++];
    }
}

static int sec_find(const Secs *s, const char *m)
{
    int i, r = -1;
    for (i = 0; i < s->n; i++)
        if (s->magic[i] == magic4(m))
            r = i;
    return r;
}

typedef struct {
    u32 n, esz;
    const u32 *dummy;
    u32 *ids;
    const u8 **msg;
    u32 *mlen;
    const u8 **attr;   /* INF1 entry bytes after the offset (esz - 4 of them) */
} Bmg;

static int bmg_read(const u8 *d, u32 len, Bmg *b)
{
    Secs s;
    int ii, di, mi;
    u32 io, dat, datn, mo, i;
    bmg_sections(d, len, &s);
    ii = sec_find(&s, "INF1"), di = sec_find(&s, "DAT1"), mi = sec_find(&s, "MID1");
    if (ii < 0 || di < 0 || mi < 0)
        return -1;
    io = s.off[ii], dat = s.off[di] + 8, datn = s.size[di] - 8, mo = s.off[mi];
    b->n = rd16(d + io + 8), b->esz = rd16(d + io + 10);
    b->ids = malloc(b->n * 4 + 4);
    b->msg = malloc(b->n * sizeof *b->msg + 4);
    b->mlen = malloc(b->n * 4 + 4);
    b->attr = malloc(b->n * sizeof *b->attr + 4);
    for (i = 0; i < b->n; i++) {
        u32 off = rd32(d + io + 16 + i * b->esz), e = off;
        b->ids[i] = rd32(d + mo + 16 + i * 4);
        b->attr[i] = d + io + 16 + i * b->esz + 4;
        while (e + 2 <= datn) {
            u32 c = rd16(d + dat + e);
            if (!c)
                break;
            /* control code: 0x1a, then a byte with the code's whole size (it may contain zero halfwords) */
            e += c == 0x1a ? (d[dat + e + 2] > 4 ? d[dat + e + 2] : 4) : 2;
        }
        b->msg[i] = d + dat + off;
        b->mlen[i] = e > off ? e - off : 0;
    }
    return 0;
}

static void bmg_free(Bmg *b) { free(b->ids); free(b->msg); free(b->mlen); free(b->attr); }

static int bmg_index(const Bmg *b, u32 id)
{
    u32 i;
    for (i = 0; i < b->n; i++)
        if (b->ids[i] == id)
            return (int)i;
    return -1;
}

static int cmp_u32(const void *a, const void *b)
{
    u32 x = *(const u32 *)a, y = *(const u32 *)b;
    return x < y ? -1 : x > y;
}

static void section(Buf *out, const char *magic, const u8 *hdr, u32 hn, const u8 *body, u32 bn)
{
    u32 start = out->n;
    buf_add(out, magic, 4);
    buf_u32(out, 0);
    buf_add(out, hdr, hn);
    buf_add(out, body, bn);
    buf_pad(out, 32, 0);
    wr32(out->p + start + 4, out->n - start);
}

/* the demo file's layout with each message taken from the retail file by ID, retail-only messages inserted in ID
 * order; returns 0 (keep the demo's) if either file isn't a complete BMG */
static u8 *merge_bmg(const u8 *demo, u32 dn, const u8 *retail, u32 rn, u32 *out_n)
{
    Bmg d, r;
    Secs s;
    Buf inf = {0}, dat = {0}, body = {0}, out = {0};
    u32 *ids, nids, i, k, io, mo;
    int sorted = 1;
    u8 h[8];
    if (bmg_read(demo, dn, &d) || bmg_read(retail, rn, &r))
        return 0;
    bmg_sections(demo, dn, &s);
    io = s.off[sec_find(&s, "INF1")], mo = s.off[sec_find(&s, "MID1")];
    ids = malloc((d.n + r.n) * 4 + 4);
    memcpy(ids, d.ids, d.n * 4);
    nids = d.n;
    for (i = 1; i < d.n; i++)
        if (d.ids[i - 1] > d.ids[i])
            sorted = 0;
    if (sorted) {
        for (i = 0; i < r.n; i++)
            if (bmg_index(&d, r.ids[i]) < 0)
                ids[nids++] = r.ids[i];
        qsort(ids, nids, 4, cmp_u32);
        for (i = 1, k = nids ? 1 : 0; i < nids; i++)  /* set semantics */
            if (ids[i] != ids[k - 1])
                ids[k++] = ids[i];
        nids = k;
    }
    buf_add(&dat, "\0\0", 2);  /* offset 0: the empty message */
    for (i = 0; i < nids; i++) {
        int ri = bmg_index(&r, ids[i]), di = bmg_index(&d, ids[i]);
        const u8 *m = 0, *a = 0;
        u32 ml = 0, an = 0, off = 0;
        if (ri >= 0 && r.mlen[ri])
            m = r.msg[ri], ml = r.mlen[ri];
        else if (di >= 0 && d.mlen[di])
            m = d.msg[di], ml = d.mlen[di];
        if (ml) {
            off = dat.n;
            buf_add(&dat, m, ml);
            buf_add(&dat, "\0\0", 2);
        }
        if (di >= 0 && d.esz > 4)
            a = d.attr[di], an = d.esz - 4;
        else if (ri >= 0 && r.esz > 4)
            a = r.attr[ri], an = r.esz - 4;
        buf_u32(&inf, off);
        for (k = 0; k < d.esz - 4; k++)
            buf_byte(&inf, k < an ? a[k] : 0);
    }
    wr16(h, nids), wr16(h + 2, d.esz), memcpy(h + 4, demo + io + 12, 4);
    section(&body, "INF1", h, 8, inf.p, inf.n);
    section(&body, "DAT1", 0, 0, dat.p, dat.n);
    {
        /* the remaining sections in the demo's order (sorted by offset, as bmg_sections reads them) */
        int j;
        for (j = 0; j < s.n; j++) {
            if (s.magic[j] == magic4("MID1")) {
                Buf idb = {0};
                for (k = 0; k < nids; k++)
                    buf_u32(&idb, ids[k]);
                wr16(h, nids), memcpy(h + 2, demo + s.off[j] + 10, 6);
                section(&body, "MID1", h, 8, idb.p, idb.n);
                free(idb.p);
            } else if (s.magic[j] != magic4("INF1") && s.magic[j] != magic4("DAT1"))
                buf_add(&body, demo + s.off[j], s.off[j] + s.size[j] > dn ? dn - s.off[j] : s.size[j]);  /* may run past the end */
        }
    }
    (void)mo;
    buf_add(&out, demo, 0x20);
    wr32(out.p + 8, 0x20 + body.n);
    buf_add(&out, body.p, body.n);
    free(inf.p), free(dat.p), free(body.p), free(ids);
    bmg_free(&d), bmg_free(&r);
    *out_n = out.n;
    return out.p;
}

/* NARC entry names (the game looks entries up by name) */
typedef struct { char **s; int n; } Names;

static void narc_walk(const u8 *d, u32 fnt, u32 did, const char *path, Names *out)
{
    const u8 *p = d + fnt + rd32(d + fnt + 8 * did);
    while (*p) {
        u32 t = *p, len = t & 0x7f;
        char *name = str_cat(path, (const char *)p + 1, len, "");
        p += 1 + len;
        if (t & 0x80) {
            char *sub = str_cat(name, "", 0, "/");
            narc_walk(d, fnt, rd16(p) & 0xfff, sub, out);
            free(sub);
            free(name);
            p += 2;
        } else {
            out->s = grow(out->s, out->n, sizeof *out->s);
            out->s[out->n++] = name;
        }
    }
}

static void narc_names(const u8 *d, Names *out)
{
    u32 o = rd16(d + 12);
    o += rd32(d + o + 4);  /* past BTAF */
    out->s = 0, out->n = 0;
    narc_walk(d, o + 8, 0, "", out);
}

static void names_free(Names *n)
{
    int i;
    for (i = 0; i < n->n; i++)
        free(n->s[i]);
    free(n->s);
}

/* whether the retail copy can stand in for the demo's: English or language-free files only, not the sound archive
 * or the build stamps, archives only when they keep every entry name the demo's has */
static int retail_fits(const char *path, const File *demo, const u8 *r, u32 rn)
{
    const char *name = strrchr(path, '/') ? strrchr(path, '/') + 1 : path;
    Names a, b;
    int i, j, ok = 1;
    if (!strncmp(name, "Fra_", 4) || !strncmp(name, "Ger_", 4) || !strncmp(name, "Ita_", 4) ||
        !strncmp(name, "Spa_", 4) || !strncmp(path, "Audio/", 6) || !strncmp(path, "System/", 7))
        return 0;
    if (demo->len < 4 || memcmp(demo->data, "NARC", 4))
        return 1;
    if (rn < 4 || memcmp(r, "NARC", 4))
        return 0;
    narc_names(demo->data, &a);
    narc_names(r, &b);
    for (i = 0; i < a.n && ok; i++) {
        for (j = 0; j < b.n && strcmp(a.s[i], b.s[j]); j++)
            ;
        ok = j < b.n;
    }
    names_free(&a), names_free(&b);
    return ok;
}

static int path_cmp(const void *x, const void *y)
{
    /* like Python's sorted(key=[component.lower() ...]) */
    const char *a = *(char *const *)x, *b = *(char *const *)y;
    for (;;) {
        int ca = *a == '/' ? -1 : *a ? tolower((u8)*a) : -2;
        int cb = *b == '/' ? -1 : *b ? tolower((u8)*b) : -2;
        if (ca != cb) {
            /* end of a component sorts before any further character; a shorter list before a longer one */
            if (ca == -2 || cb == -2)
                return ca == -2 ? -1 : 1;
            if (ca == -1 || cb == -1)
                return ca == -1 ? -1 : 1;
            return ca - cb;
        }
        if (ca == -2)
            return 0;
        a++, b++;
    }
}

static int has(char **list, int n, const char *s)
{
    int i;
    for (i = 0; i < n; i++)
        if (!strcmp(list[i], s))
            return 1;
    return 0;
}

static int build_full(const u8 *demo, u32 demo_n, const u8 *retail, u32 retail_n, const char *dir)
{
    Fs dfs, rfs, fs;
    Buf fnt = {0}, blob = {0}, out = {0};
    Dir *dirs;
    int nd, i, j, nov, added = 0, merged = 0, replaced = 0;
    char **paths, **order = 0;
    int norder = 0;
    u32 fnt_off, fat_off, fid, used = rd32(demo + 0x80), *first_id, *blob_off;
    u8 *fatb;
    (void)retail_n;
    read_fs(&dfs, demo);
    read_fs(&rfs, retail);
    fs = dfs;  /* the file list starts as the demo's; entries are copied, so dfs keeps the originals */
    fs.f = malloc((dfs.n + rfs.n) * sizeof *fs.f);
    memcpy(fs.f, dfs.f, dfs.n * sizeof *fs.f);
    fs.n = dfs.n;
    for (i = 0; i < rfs.n; i++) {
        File *r = &rfs.f[i], *f = fs_file(&fs, r->path);
        if (!f) {
            fs.f[fs.n] = *r;
            fs.f[fs.n++].changed = 1;
            added++;
        } else if (strlen(r->path) > 4 && !strcmp(r->path + strlen(r->path) - 4, ".bmg") && r->len >= 8 &&
                   !memcmp(r->data, "MESGbmg1", 8) && (f->len != r->len || memcmp(f->data, r->data, r->len))) {
            u32 n;
            u8 *m = merge_bmg(f->data, f->len, r->data, r->len, &n);
            if (m) {
                f->data = m, f->len = n, f->changed = 1;
                merged++;
            }
        } else if ((f->len != r->len || memcmp(f->data, r->data, r->len)) && retail_fits(r->path, f, r->data, r->len)) {
            /* the demo's copies grey out what the kiosk locks (the kennel's Buy button, shop and menu entries) */
            f->data = r->data, f->len = r->len, f->changed = 1;
            replaced++;
        }
    }
    /* the demo keeps untranslated copies directly in Message/; give them the English text too */
    for (i = 0; i < fs.n; i++) {
        File *f = &fs.f[i], *r, *d;
        char *eng;
        if (strncmp(f->path, "Message/", 8) || strchr(f->path + 8, '/'))
            continue;
        eng = str_cat("Message/Eng/", "", 0, f->path + 8);
        r = fs_file(&rfs, eng);
        d = fs_file(&dfs, f->path);
        free(eng);
        if (r && d) {
            u32 n;
            u8 *m = merge_bmg(d->data, d->len, r->data, r->len, &n);
            if (m)
                f->data = m, f->len = n, f->changed = 1;
        }
    }
    /* the FNT: the demo's directories and file order (file IDs) kept, new files appended to their directories */
    nd = dfs.nd;
    dirs = malloc((dfs.nd + rfs.nd + 1) * sizeof *dirs);
    for (i = 0; i < nd; i++) {
        dirs[i] = dfs.d[i];
        dirs[i].subs = malloc((dfs.d[i].nsubs + rfs.nd + 1) * sizeof(char *));
        memcpy(dirs[i].subs, dfs.d[i].subs, dfs.d[i].nsubs * sizeof(char *));
        dirs[i].files = malloc((dfs.d[i].nfiles + fs.n + 1) * sizeof(char *));
        memcpy(dirs[i].files, dfs.d[i].files, dfs.d[i].nfiles * sizeof(char *));
    }
    for (i = 0; i < nd; i++)  /* sorted by directory ID */
        for (j = i + 1; j < nd; j++)
            if (dirs[j].id < dirs[i].id) {
                Dir t = dirs[i];
                dirs[i] = dirs[j];
                dirs[j] = t;
            }
    paths = malloc(fs.n * sizeof *paths);
    for (i = 0; i < fs.n; i++)
        paths[i] = fs.f[i].path;
    qsort(paths, fs.n, sizeof *paths, path_cmp);
    for (i = 0; i < fs.n; i++) {
        char *p = paths[i], *s;
        Dir *d = &dirs[0];
        int k;
        for (k = 0; k < nd && dirs[k].path[0]; k++)
            ;
        d = &dirs[k];  /* the root */
        while ((s = strchr(p, '/')) != 0) {
            char *sub = str_cat("", paths[i], (u32)(s - paths[i] + 1), "");
            int found = -1;
            for (k = 0; k < nd; k++)
                if (!strcmp(dirs[k].path, sub))
                    found = k;
            if (found < 0) {
                memset(&dirs[nd], 0, sizeof *dirs);
                dirs[nd].path = sub;
                dirs[nd].subs = malloc((rfs.nd + 1) * sizeof(char *));
                dirs[nd].files = malloc((fs.n + 1) * sizeof(char *));
                d->subs[d->nsubs++] = str_cat("", p, (u32)(s - p), "");
                found = nd++;
            } else
                free(sub);
            d = &dirs[found];
            p = s + 1;
        }
        if (!has(d->files, d->nfiles, p))
            d->files[d->nfiles++] = p;
    }
    nov = (int)dfs.first_named;
    order = malloc(fs.n * sizeof *order);
    first_id = malloc(nd * sizeof *first_id);
    blob_off = malloc(nd * sizeof *blob_off);
    fid = (u32)nov;
    for (i = 0; i < nd; i++) {
        Dir *d = &dirs[i];
        first_id[i] = fid;
        blob_off[i] = blob.n;
        for (j = 0; j < d->nfiles; j++) {  /* files first, then subdirectories */
            u32 l = (u32)strlen(d->files[j]);
            buf_byte(&blob, (int)l);
            buf_add(&blob, d->files[j], l);
            order[norder++] = str_cat(d->path, d->files[j], (u32)strlen(d->files[j]), "");
            fid++;
        }
        for (j = 0; j < d->nsubs; j++) {
            u32 l = (u32)strlen(d->subs[j]);
            char *sub = str_cat(d->path, d->subs[j], l, "/");
            int k;
            for (k = 0; k < nd && strcmp(dirs[k].path, sub); k++)
                ;
            free(sub);
            buf_byte(&blob, (int)(0x80 | l));
            buf_add(&blob, d->subs[j], l);
            buf_byte(&blob, 0);
            buf_byte(&blob, 0);
            wr16(blob.p + blob.n - 2, 0xf000 + k);
        }
        buf_byte(&blob, 0);
    }
    for (i = 0; i < nd; i++) {
        u32 third = 0;
        if (i == 0)
            third = (u32)nd;
        else {
            /* the parent: the directory whose path is this one's minus the last component */
            size_t l = strlen(dirs[i].path);
            char *par = str_cat("", dirs[i].path, (u32)l, "");
            int k;
            par[l - 1] = 0;
            if (strrchr(par, '/'))
                strrchr(par, '/')[1] = 0;
            else
                par[0] = 0;
            for (k = 0; k < nd && strcmp(dirs[k].path, par); k++)
                ;
            third = 0xf000 + (u32)k;
            free(par);
        }
        buf_u32(&fnt, (u32)nd * 8 + blob_off[i]);
        buf_byte(&fnt, (int)(first_id[i] & 0xff));
        buf_byte(&fnt, (int)(first_id[i] >> 8));
        buf_byte(&fnt, (int)(third & 0xff));
        buf_byte(&fnt, (int)(third >> 8));
    }
    buf_add(&fnt, blob.p, blob.n);
    /* the image: the demo up to its used size, then the new tables and the new or changed files */
    buf_add(&out, demo, used);
    buf_pad(&out, 0x200, 0xff);
    fnt_off = out.n;
    buf_add(&out, fnt.p, fnt.n);
    buf_pad(&out, 0x200, 0xff);
    fat_off = out.n;
    buf_add(&out, 0, 8 * (u32)(nov + norder));
    buf_pad(&out, 0x200, 0xff);
    fatb = malloc(8 * (nov + norder));
    memcpy(fatb, demo + rd32(demo + 0x48), 8 * nov);  /* overlays stay where they are */
    for (i = 0; i < norder; i++) {
        File *f = fs_file(&fs, order[i]);
        if (!f->changed) {
            wr32(fatb + 8 * (nov + i), f->start), wr32(fatb + 8 * (nov + i) + 4, f->end);
            continue;
        }
        wr32(fatb + 8 * (nov + i), out.n), wr32(fatb + 8 * (nov + i) + 4, out.n + f->len);
        buf_add(&out, f->data, f->len);
        buf_pad(&out, 0x200, 0xff);
    }
    memcpy(out.p + fat_off, fatb, 8 * (nov + norder));
    wr32(out.p + 0x40, fnt_off), wr32(out.p + 0x44, fnt.n), wr32(out.p + 0x48, fat_off),
        wr32(out.p + 0x4c, 8 * (u32)(nov + norder));
    wr32(out.p + 0x80, out.n);
    if (out.n > CARD_SIZE) {
        fprintf(stderr, "setup: the full image does not fit in a 64 MB card (%u bytes)\n", out.n);
        return -1;
    }
    {
        u32 n = out.n;
        buf_add(&out, 0, CARD_SIZE - n);
        memset(out.p + n, 0xff, CARD_SIZE - n);
    }
    printf("setup: full.nds: %d files (%d added from the retail cartridge, %d message files merged, %d files "
           "replaced)\n", norder, added, merged, replaced);
    (void)demo_n;
    i = save(dir, "host/full_header.bin", out.p, 0x170) | save(dir, "host/full.nds", out.p, out.n);
    free(out.p);  /* the rest is left to the process exit: setup runs once */
    return i;
}

/* ---- finding the ROMs ---- */

static char demo_path[1024], retail_path[1024];

static void consider(const char *path)
{
    FILE *f;
    u8 h[0x200];
    size_t l = strlen(path);
    if (l < 4 || (strcmp(path + l - 4, ".nds") && strcmp(path + l - 4, ".NDS")))
        return;
    f = fopen(path, "rb");
    if (!f)
        return;
    if (fread(h, 1, sizeof h, f) == sizeof h) {
        if (!memcmp(h + 12, "A36P", 4) && !demo_path[0])
            snprintf(demo_path, sizeof demo_path, "%s", path);
        else if (!memcmp(h + 12, "ADGP", 4) && !retail_path[0])
            snprintf(retail_path, sizeof retail_path, "%s", path);
    }
    fclose(f);
}

static void scan(const char *dir)
{
    char path[1024];
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    HANDLE h;
    snprintf(path, sizeof path, "%s\\*.nds", dir);
    h = FindFirstFileA(path, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do {
        snprintf(path, sizeof path, "%s\\%s", dir, fd.cFileName);
        consider(path);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR *d = opendir(dir);
    struct dirent *e;
    if (!d)
        return;
    while ((e = readdir(d)) != 0) {
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        consider(path);
    }
    closedir(d);
#endif
}

static void full_path(const char *in, char *out, size_t n)
{
#ifdef _WIN32
    if (!GetFullPathNameA(in, (DWORD)n, out, 0))
        snprintf(out, n, "%s", in);
#else
    char t[PATH_MAX];
    snprintf(out, n, "%s", realpath(in, t) ? t : in);
#endif
}

static void message(const char *text, int error)
{
    fprintf(stderr, "%s\n", text);
#ifdef _WIN32
    if (!getenv("NDOGS_HEADLESS"))
        MessageBoxA(0, text, "nintendogs_pc", MB_OK | (error ? MB_ICONERROR : MB_ICONINFORMATION));
#else
    (void)error;
#endif
}

/* Returns the data folder to use, or 0 when there is nothing to run (the user has been told why).
 * `found` is the folder main.c found (may lack data); `exe_dir` is where a new one goes (<exe_dir>/extracted). */
const char *ndogs_setup(const char *found, const char *exe_dir, int have_data, int argc, char **argv)
{
    static char dir[1024];
    u8 *demo = 0, *retail = 0;
    u32 dn = 0, rn = 0;
    int i;
    if (have_data && exists(found, "host/full.nds"))
        return found;
    for (i = 1; i < argc; i++)
        consider(argv[i]);
    scan(exe_dir);
    scan(".");
    if (have_data) {
        if (!retail_path[0])
            return found;  /* demo only */
        snprintf(dir, sizeof dir, "%s", found);
    } else
        snprintf(dir, sizeof dir, "%s/extracted", exe_dir);
    if (!have_data) {
        char abs[1024];
        if (!ndogs_host_tables[0][0]) {
            message("This build of nintendogs_pc cannot set up its game data (it was built without extracted/; "
                    "see port/tools/gen_hosttabs.py). Start it with --data <extracted folder>.", 1);
            return 0;
        }
        if (!demo_path[0]) {
            message("Nintendogs needs your own copy of the game to run.\n\n"
                    "Put the Nintendogs kiosk demo ROM (Europe, .nds) in the same folder as nintendogs_pc.exe, "
                    "then start it again. Add your Nintendogs: Dachshund & Friends (Europe) ROM too for the full "
                    "game.", 1);
            return 0;
        }
        demo = load(demo_path, &dn);
        if (!demo || crc32(demo, dn) != DEMO_CRC) {
            message("This kiosk demo ROM is not the expected dump (the port needs the Nintendogs European kiosk "
                    "demo with MD5 a190eca6a61b3c69ce48f076d2445eb8).", 1);
            return 0;
        }
        printf("setup: extracting %s into %s\n", demo_path, dir);
        full_path(demo_path, abs, sizeof abs);
        if (extract_demo(demo, abs, dir)) {
            message("Could not write the game data next to nintendogs_pc.exe. Is the folder writable?", 1);
            return 0;
        }
    } else {
        char rp[1024];
        FILE *f;
        snprintf(rp, sizeof rp, "%s/host/rom_path.txt", dir);
        f = fopen(rp, "r");
        rp[0] = 0;
        if (f) {
            if (fgets(rp, sizeof rp, f))
                rp[strcspn(rp, "\r\n")] = 0;
            fclose(f);
        }
        demo = load(rp, &dn);
        if (!demo && demo_path[0])
            demo = load(demo_path, &dn);
        if (!demo || crc32(demo, dn) != DEMO_CRC)
            return found;
    }
    if (retail_path[0] && (retail = load(retail_path, &rn)) != 0) {
        printf("setup: building the full game from %s\n", retail_path);
        if (build_full(demo, dn, retail, rn, dir))
            message("Could not build the full game from the Dachshund & Friends ROM; starting the kiosk demo.", 1);
    } else if (!have_data)
        message("Set up the kiosk demo. For the full game, put your Nintendogs: Dachshund & Friends (Europe) ROM "
                "next to nintendogs_pc.exe and start it again.", 0);
    free(demo);
    free(retail);
    return dir;
}
