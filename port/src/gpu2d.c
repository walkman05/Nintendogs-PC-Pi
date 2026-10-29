/* Nintendo DS 2D graphics engines (engine A = main, engine B = sub), rendered a whole frame at a time from a
 * snapshot of the I/O registers, VRAM (through port/src/vram.c), palette RAM and OAM.
 *
 * Written from the hardware description in GBATEK. Implemented:
 *  - display modes (off / graphics / VRAM display / main-memory display as black), forced blank;
 *  - BG modes 0-6: text BGs (4/8 bpp, flips, 4 screen sizes), affine BGs, extended affine BGs (16-bit maps,
 *    256-colour bitmaps, direct-colour bitmaps), the mode-6 large bitmap; BG0 as the 3D layer;
 *  - extended BG/OBJ palettes, DISPCNT char/screen base offsets (engine A);
 *  - 128 OBJs: normal and affine (double size), all shapes/sizes, 4/8 bpp, 1D/2D tile mapping with boundary,
 *    bitmap OBJs (1D/2D) with their alpha, semi-transparent OBJs, OBJ window;
 *  - WIN0/WIN1/OBJ window/outside, colour special effects (alpha blend, brighten, darken), BG mosaic,
 *    master brightness.
 * Not implemented: OBJ mosaic, 3D-layer alpha (3D pixels are opaque), mid-frame register changes (the frame is
 * rendered from one register snapshot), main-memory display FIFO. Display capture is in video.c.
 *
 * Layer pixels are kept as 0x80000000 | 0x00RRGGBB (bit 31 = opaque); effects are computed on 8-bit channels. */
#include <string.h>
#include "ds_video.h"

#define OPAQUE 0x80000000u

/* Where each final pixel came from, for the high-resolution compositor (video.c): GPU2D_SRC_3D for the 3D layer
 * shown unchanged, GPU2D_SRC_VRAM | role << 24 | offset for a direct-colour bitmap pixel (bitmap BG or bitmap OBJ)
 * shown 1:1 and unchanged, 0 for anything else. */
u32 gpu2d_src[2][DS_W * DS_H];
static u32 bgsrc[4][DS_W];

/* palette RAM and OAM are plain memory at their DS addresses (mapped by platform_mem.c) */
static const u16 *pal_bg(int engine) { return (const u16 *)(0x05000000u + (u32)engine * 0x400u); }
static const u16 *pal_obj(int engine) { return (const u16 *)(0x05000200u + (u32)engine * 0x400u); }
const u16 *gpu2d_oam;  /* if set, OAM of both engines read from here (interpolated frames, video.c) */
static const u16 *oam_base(int engine)
{
    return gpu2d_oam ? gpu2d_oam + engine * 0x200 : (const u16 *)(0x07000000u + (u32)engine * 0x400u);
}

static u32 s_c15to24[32768];
static int s_lut_ready;

static void init_lut(void)
{
    u32 i;
    for (i = 0; i < 32768; i++) {
        u32 r = i & 31, g = (i >> 5) & 31, b = (i >> 10) & 31;
        r = (r << 3) | (r >> 2);
        g = (g << 3) | (g >> 2);
        b = (b << 3) | (b >> 2);
        s_c15to24[i] = (r << 16) | (g << 8) | b;
    }
    s_lut_ready = 1;
}

static inline u32 col(u16 c) { return OPAQUE | s_c15to24[c & 0x7fff]; }

static inline u16 rd16(const u8 *r, u32 off) { return (u16)(r[off] | (r[off + 1] << 8)); }
static inline u32 rd32(const u8 *r, u32 off) { return (u32)rd16(r, off) | ((u32)rd16(r, off + 2) << 16); }

/* ---- per-frame state ---------------------------------------------------------------------------------------- */
typedef struct {
    int engine;
    const u8 *r;          /* this engine's registers */
    u32 dispcnt;
    int bgvram, objvram, bgext, objext;
    const u16 *bgpal, *objpal;
    const u8 *bgmem, *objmem;
    u32 bgsize, objsize;
    u32 charofs, scrofs;  /* DISPCNT char/screen base (engine A) */
    int mosh, mosv;       /* BG mosaic sizes */
} Gpu;

/* BG/OBJ VRAM is CPU-visible (vram.c): read it in place, bounds-checked like vram_read*, rather than a call per
 * pixel. Extended palettes are hidden roles and still go through vram_read*. */
static inline u8 bg_read8(const Gpu *g, u32 offset)
{
    return offset < g->bgsize ? g->bgmem[offset] : 0;
}
static inline u16 bg_read16(const Gpu *g, u32 offset)
{
    offset &= ~1u;
    return offset < g->bgsize ? *(const u16 *)(g->bgmem + offset) : 0;
}
static inline u8 obj_read8(const Gpu *g, u32 offset)
{
    return offset < g->objsize ? g->objmem[offset] : 0;
}
static inline u16 obj_read16(const Gpu *g, u32 offset)
{
    offset &= ~1u;
    return offset < g->objsize ? *(const u16 *)(g->objmem + offset) : 0;
}

/* ---- text BGs ------------------------------------------------------------------------------------------------ */
static u32 bg_ext_color(const Gpu *g, int slot, int pal, int c)
{
    return col(vram_read16(g->bgext, (u32)slot * 0x2000u + (u32)pal * 0x200u + (u32)c * 2u));
}

static void render_text(const Gpu *g, int bg, int y, u32 *line)
{
    u16 cnt = rd16(g->r, 0x08 + bg * 2);
    int hofs = rd16(g->r, 0x10 + bg * 4) & 0x1ff;
    int vofs = rd16(g->r, 0x12 + bg * 4) & 0x1ff;
    int w = (cnt & 0x4000) ? 512 : 256;
    int h = (cnt & 0x8000) ? 512 : 256;
    u32 charbase = g->charofs + ((cnt >> 2) & 15) * 0x4000u;
    u32 scrbase = g->scrofs + ((cnt >> 8) & 31) * 0x800u;
    int bpp8 = (cnt & 0x80) != 0;
    int slot = bg;
    int yy = (y + vofs) & (h - 1);
    u32 rowbase;
    u32 lastaddr = 0xffffffffu;
    u16 entry = 0;
    int x;

    if (bg < 2 && (cnt & 0x2000)) slot = bg + 2;
    rowbase = scrbase + (u32)(yy >> 8) * (u32)(w >> 8) * 0x800u + (u32)((yy >> 3) & 31) * 64u;

    for (x = 0; x < DS_W; x++) {
        int xx = (x + hofs) & (w - 1);
        u32 maddr = rowbase + (u32)(xx >> 8) * 0x800u + (u32)((xx >> 3) & 31) * 2u;
        int px, py, c;
        if (maddr != lastaddr) { entry = bg_read16(g, maddr); lastaddr = maddr; }
        px = xx & 7;
        py = yy & 7;
        if (entry & 0x400) px = 7 - px;
        if (entry & 0x800) py = 7 - py;
        if (bpp8) {
            c = bg_read8(g, charbase + (u32)(entry & 0x3ff) * 64u + (u32)(py * 8 + px));
            if (!c) { line[x] = 0; continue; }
            line[x] = (g->dispcnt & 0x40000000u) ? bg_ext_color(g, slot, entry >> 12, c) : col(g->bgpal[c]);
        } else {
            c = bg_read8(g, charbase + (u32)(entry & 0x3ff) * 32u + (u32)(py * 4 + (px >> 1)));
            c = (px & 1) ? (c >> 4) : (c & 15);
            if (!c) { line[x] = 0; continue; }
            line[x] = col(g->bgpal[(entry >> 12) * 16 + c]);
        }
    }
}

/* ---- affine / extended / bitmap BGs --------------------------------------------------------------------------- */
enum { AFF_TILE8, AFF_MAP16, AFF_BMP8, AFF_BMP16, AFF_LARGE };

static s32 sext28(u32 v)
{
    v &= 0x0fffffffu;
    if (v & 0x08000000u) v |= 0xf0000000u;
    return (s32)v;
}

static void render_affine(const Gpu *g, int bg, int kind, int y, u32 *line)
{
    u16 cnt = rd16(g->r, 0x08 + bg * 2);
    u32 pb = 0x20 + (u32)(bg - 2) * 0x10;
    s32 pa = (s16)rd16(g->r, pb + 0), pbv = (s16)rd16(g->r, pb + 2);
    s32 pc = (s16)rd16(g->r, pb + 4), pd = (s16)rd16(g->r, pb + 6);
    s32 cx = sext28(rd32(g->r, pb + 8)) + pbv * y;
    s32 cy = sext28(rd32(g->r, pb + 12)) + pd * y;
    int wrap = (cnt & 0x2000) != 0;
    int sz = (cnt >> 14) & 3;
    int w, h;
    u32 charbase = g->charofs + ((cnt >> 2) & 15) * 0x4000u;
    u32 scrbase = g->scrofs + ((cnt >> 8) & 31) * 0x800u;
    u32 bmpbase = ((cnt >> 8) & 31) * 0x4000u;
    int useext = (g->dispcnt & 0x40000000u) != 0;
    int x;

    switch (kind) {
    case AFF_BMP8: case AFF_BMP16: {
        static const short bw[4] = {128, 256, 512, 512}, bh[4] = {128, 256, 256, 512};
        w = bw[sz]; h = bh[sz];
        break;
    }
    case AFF_LARGE:
        w = (sz & 1) ? 1024 : 512; h = (sz & 1) ? 512 : 1024;
        bmpbase = 0;
        break;
    default:
        w = h = 128 << sz;
        break;
    }

    for (x = 0; x < DS_W; x++, cx += pa, cy += pc) {
        int tx = cx >> 8, ty = cy >> 8;
        u32 out = 0;
        if (wrap) {
            tx &= w - 1;
            ty &= h - 1;
        } else if (tx < 0 || ty < 0 || tx >= w || ty >= h) {
            line[x] = 0;
            continue;
        }
        switch (kind) {
        case AFF_TILE8: {
            int t = bg_read8(g, scrbase + (u32)(ty >> 3) * (u32)(w >> 3) + (u32)(tx >> 3));
            int c = bg_read8(g, charbase + (u32)t * 64u + (u32)((ty & 7) * 8 + (tx & 7)));
            if (c) out = col(g->bgpal[c]);
            break;
        }
        case AFF_MAP16: {
            u16 e = bg_read16(g, scrbase + ((u32)(ty >> 3) * (u32)(w >> 3) + (u32)(tx >> 3)) * 2u);
            int px = tx & 7, py = ty & 7, c;
            if (e & 0x400) px = 7 - px;
            if (e & 0x800) py = 7 - py;
            c = bg_read8(g, charbase + (u32)(e & 0x3ff) * 64u + (u32)(py * 8 + px));
            if (c) out = useext ? bg_ext_color(g, bg, e >> 12, c) : col(g->bgpal[c]);
            break;
        }
        case AFF_BMP8: case AFF_LARGE: {
            int c = bg_read8(g, bmpbase + (u32)ty * (u32)w + (u32)tx);
            if (c) out = col(g->bgpal[c]);
            break;
        }
        case AFF_BMP16: {
            u32 off = bmpbase + ((u32)ty * (u32)w + (u32)tx) * 2u;
            u16 c = bg_read16(g, off);
            if (c & 0x8000) {
                out = col(c);
                if (pa == 0x100 && pc == 0 && pd == 0x100 && pbv == 0)
                    bgsrc[bg][x] = GPU2D_SRC_VRAM | (u32)g->bgvram << 24 | off;
            }
            break;
        }
        }
        line[x] = out;
    }
}

/* ---- 3D layer -------------------------------------------------------------------------------------------------- */
static void render_3d(const Gpu *g, const u32 *frame3d, int y, u32 *line)
{
    int hofs = rd16(g->r, 0x10) & 0x1ff;
    int x;
    if (!frame3d) {
        for (x = 0; x < DS_W; x++) line[x] = 0;
        return;
    }
    for (x = 0; x < DS_W; x++) {
        int sx = (x + hofs) & 511;
        u32 p;
        if (sx >= DS_W) { line[x] = 0; continue; }
        p = frame3d[y * DS_W + sx];
        line[x] = (p & 0x80000000u) ? (OPAQUE | (p & 0xffffffu)) : 0;
    }
}

/* ---- OBJs ------------------------------------------------------------------------------------------------------ */
#define OBJF_SEMI   1
#define OBJF_BITMAP 2

typedef struct {
    u32 color[DS_W];
    u8 prio[DS_W];
    u8 flags[DS_W];
    u8 alpha[DS_W];   /* bitmap OBJ alpha 0-15 */
    u8 win[DS_W];     /* OBJ window mask */
    u32 src[DS_W];    /* bitmap OBJ source (see gpu2d_src) */
} ObjLine;

static void render_objs(const Gpu *g, int y, ObjLine *ol)
{
    static const u8 objw[3][4] = {{8, 16, 32, 64}, {16, 32, 32, 64}, {8, 8, 16, 32}};
    static const u8 objh[3][4] = {{8, 16, 32, 64}, {8, 8, 16, 32}, {16, 32, 32, 64}};
    const u16 *oam = oam_base(g->engine);
    u32 dc = g->dispcnt;
    int map1d = (dc & 0x10) != 0;
    int bnd = (dc >> 20) & 3;
    int bmp1d = (dc & 0x40) != 0;
    int bmpbnd = (dc >> 22) & 1;
    int i, x;

    if (g->engine && bnd > 2) bnd = 2;
    if (g->engine) bmpbnd = 0;

    for (x = 0; x < DS_W; x++) { ol->color[x] = 0; ol->win[x] = 0; ol->flags[x] = 0; ol->prio[x] = 4; ol->src[x] = 0; }

    for (i = 0; i < 128; i++) {
        u16 a0 = oam[i * 4], a1 = oam[i * 4 + 1], a2 = oam[i * 4 + 2];
        int affine = (a0 & 0x100) != 0;
        int mode = (a0 >> 10) & 3, shape = (a0 >> 14) & 3, size = (a1 >> 14) & 3;
        int w, h, bw, bh, line, ox, x0, x1, prio, bpp8, alpha = 0;
        s32 pa = 0x100, pbv = 0, pc = 0, pd = 0x100;
        u32 tilebase;

        if (!affine && (a0 & 0x200)) continue;
        if (shape == 3) continue;
        w = objw[shape][size];
        h = objh[shape][size];
        bw = w; bh = h;
        if (affine && (a0 & 0x200)) { bw *= 2; bh *= 2; }
        line = (y - (a0 & 0xff)) & 0xff;
        if (line >= bh) continue;
        ox = a1 & 0x1ff;
        if (ox >= 256) ox -= 512;
        x0 = ox < 0 ? 0 : ox;
        x1 = ox + bw > DS_W ? DS_W : ox + bw;
        if (x0 >= x1) continue;
        prio = (a2 >> 10) & 3;
        bpp8 = (a0 & 0x2000) != 0;

        if (mode == 3) {
            alpha = a2 >> 12;
            if (!alpha) continue;
            if (bmp1d) {
                tilebase = (u32)(a2 & 0x3ff) * (128u << bmpbnd);
            } else if (dc & 0x20) {
                tilebase = (u32)(a2 & 0x1f) * 0x10u + (u32)(a2 & 0x3e0) * 0x80u;
            } else {
                tilebase = (u32)(a2 & 0x0f) * 0x10u + (u32)(a2 & 0x3f0) * 0x80u;
            }
        } else {
            tilebase = map1d ? (u32)(a2 & 0x3ff) * (32u << bnd) : (u32)(a2 & 0x3ff) * 32u;
        }

        if (affine) {
            int grp = (a1 >> 9) & 31;
            pa = (s16)oam[grp * 16 + 3];
            pbv = (s16)oam[grp * 16 + 7];
            pc = (s16)oam[grp * 16 + 11];
            pd = (s16)oam[grp * 16 + 15];
        }

        for (x = x0; x < x1; x++) {
            int tx, ty;
            u32 c, srcv = 0;
            if (affine) {
                s32 dx = x - ox - bw / 2, dy = line - bh / 2;
                tx = ((pa * dx + pbv * dy) >> 8) + w / 2;
                ty = ((pc * dx + pd * dy) >> 8) + h / 2;
                if (tx < 0 || ty < 0 || tx >= w || ty >= h) continue;
            } else {
                tx = x - ox;
                ty = line;
                if (a1 & 0x1000) tx = w - 1 - tx;
                if (a1 & 0x2000) ty = h - 1 - ty;
            }

            if (mode == 3) {
                u32 stride = bmp1d ? (u32)w * 2u : ((dc & 0x20) ? 512u : 256u);
                u32 off = tilebase + (u32)ty * stride + (u32)tx * 2u;
                u16 p = obj_read16(g, off);
                if (!(p & 0x8000)) continue;
                c = col(p);
                srcv = affine || (a1 & 0x3000) ? 0 : GPU2D_SRC_VRAM | (u32)g->objvram << 24 | off;
            } else if (bpp8) {
                u32 addr = tilebase + (u32)(ty & 7) * 8u + (u32)(tx & 7);
                int ci;
                if (map1d) addr += ((u32)(ty >> 3) * (u32)(w >> 3) + (u32)(tx >> 3)) * 64u;
                else addr += (u32)(ty >> 3) * 1024u + (u32)(tx >> 3) * 64u;
                ci = obj_read8(g, addr);
                if (!ci) continue;
                if (mode == 2) { ol->win[x] = 1; continue; }
                c = (dc & 0x80000000u) ? col(vram_read16(g->objext, (u32)(a2 >> 12) * 512u + (u32)ci * 2u))
                                       : col(g->objpal[ci]);
            } else {
                u32 addr = tilebase + (u32)(ty & 7) * 4u + (u32)((tx & 7) >> 1);
                int ci;
                if (map1d) addr += ((u32)(ty >> 3) * (u32)(w >> 3) + (u32)(tx >> 3)) * 32u;
                else addr += (u32)(ty >> 3) * 1024u + (u32)(tx >> 3) * 32u;
                ci = obj_read8(g, addr);
                ci = (tx & 1) ? (ci >> 4) : (ci & 15);
                if (!ci) continue;
                if (mode == 2) { ol->win[x] = 1; continue; }
                c = col(g->objpal[(a2 >> 12) * 16 + ci]);
            }

            /* lower OAM index wins at equal priority; a better priority from a later OBJ still wins */
            if ((ol->color[x] & OPAQUE) && ol->prio[x] <= prio) continue;
            ol->color[x] = c;
            ol->prio[x] = (u8)prio;
            ol->flags[x] = (u8)(mode == 1 ? OBJF_SEMI : mode == 3 ? OBJF_BITMAP : 0);
            ol->alpha[x] = (u8)alpha;
            ol->src[x] = mode == 3 ? srcv : 0;
        }
    }
}

/* ---- effects ----------------------------------------------------------------------------------------------------- */
static inline u32 blend(u32 a, u32 b, int eva, int evb)
{
    u32 r = ((((a >> 16) & 255) * (u32)eva + ((b >> 16) & 255) * (u32)evb) >> 4);
    u32 gg = ((((a >> 8) & 255) * (u32)eva + ((b >> 8) & 255) * (u32)evb) >> 4);
    u32 bb = (((a & 255) * (u32)eva + (b & 255) * (u32)evb) >> 4);
    if (r > 255) r = 255;
    if (gg > 255) gg = 255;
    if (bb > 255) bb = 255;
    return (r << 16) | (gg << 8) | bb;
}

static inline u32 brighten(u32 a, int evy)
{
    u32 r = (a >> 16) & 255, gg = (a >> 8) & 255, b = a & 255;
    r += ((255 - r) * (u32)evy) >> 4;
    gg += ((255 - gg) * (u32)evy) >> 4;
    b += ((255 - b) * (u32)evy) >> 4;
    return (r << 16) | (gg << 8) | b;
}

static inline u32 darken(u32 a, int evy)
{
    u32 r = (a >> 16) & 255, gg = (a >> 8) & 255, b = a & 255;
    r -= (r * (u32)evy) >> 4;
    gg -= (gg * (u32)evy) >> 4;
    b -= (b * (u32)evy) >> 4;
    return (r << 16) | (gg << 8) | b;
}

static int win_inside(int v, u16 reg)
{
    int a = reg >> 8, b = reg & 0xff;
    if (a <= b) return v >= a && v < b;
    return v >= a || v < b;
}

/* set by video.c while rendering engine A for display capture, which takes the image before master brightness */
int gpu2d_no_brightness;

u32 gpu2d_bright_pixel(int engine, const u8 *io, u32 c)
{
    u16 mb = rd16(io + (engine ? 0x1000 : 0), 0x6c);
    int mode = mb >> 14, f = mb & 31;
    if (f > 16) f = 16;
    if (!f || mode == 0 || mode == 3) return c & 0xffffff;
    return mode == 1 ? brighten(c, f) & 0xffffff : darken(c, f) & 0xffffff;
}

static void master_brightness(const u8 *r, u32 *row)
{
    u16 mb = rd16(r, 0x6c);
    int mode = mb >> 14, f = mb & 31, x;
    if (f > 16) f = 16;
    if (gpu2d_no_brightness || !f || mode == 0 || mode == 3) return;
    for (x = 0; x < DS_W; x++) row[x] = mode == 1 ? brighten(row[x], f) : darken(row[x], f);
}

void gpu2d_brightness(int engine, const u8 *io, u32 *out)
{
    int y, save = gpu2d_no_brightness;
    gpu2d_no_brightness = 0;
    for (y = 0; y < DS_H; y++)
        master_brightness(io + (engine ? 0x1000 : 0), out + y * DS_W);
    gpu2d_no_brightness = save;
}

/* ---- frame ------------------------------------------------------------------------------------------------------- */
void gpu2d_render(int engine, const u8 *io, const u32 *frame3d, u32 *out)
{
    static u32 bgline[4][DS_W];
    static ObjLine ol;
    Gpu g;
    const u8 *r;
    u32 dc;
    int dispmode, bgmode, y, x, i;
    int bgkind[4];          /* -1 none, 0 text, 1 3D, 2+ affine kind */
    int bgprio[4];
    int bgmos[4];
    int bgorder[4], nbg = 0;
    u16 bldcnt, bldalpha, bldy;
    int eva, evb, evy, effect;
    u32 backdrop;
    u16 win0h, win1h, win0v, win1v, winin, winout;

    if (!s_lut_ready) init_lut();
    engine = engine ? 1 : 0;
    memset(gpu2d_src[engine], 0, sizeof gpu2d_src[engine]);
    r = io + engine * 0x1000;
    dc = rd32(r, 0);
    dispmode = (dc >> 16) & 3;
    if (engine) dispmode &= 1;

    if (dispmode == 0) {
        for (i = 0; i < DS_W * DS_H; i++) out[i] = 0xffffff;
        return;
    }
    if (dispmode == 2) {
        u32 bank = ((dc >> 18) & 3) * 0x20000u;
        const u16 *pixels = (const u16 *)(vram_cpu_view(VRAM_LCDC, 0) + bank);
        for (y = 0; y < DS_H; y++) {
            for (x = 0; x < DS_W; x++)
                out[y * DS_W + x] = s_c15to24[pixels[y * DS_W + x] & 0x7fff];
            master_brightness(r, out + y * DS_W);
        }
        return;
    }
    if (dispmode == 3) {
        for (y = 0; y < DS_H; y++) {
            for (x = 0; x < DS_W; x++) out[y * DS_W + x] = 0;
            master_brightness(r, out + y * DS_W);
        }
        return;
    }
    if (dc & 0x80) {  /* forced blank */
        for (i = 0; i < DS_W * DS_H; i++) out[i] = 0xffffff;
        return;
    }

    g.engine = engine;
    g.r = r;
    g.dispcnt = dc;
    g.bgvram = engine ? VRAM_BG_B : VRAM_BG_A;
    g.objvram = engine ? VRAM_OBJ_B : VRAM_OBJ_A;
    g.bgmem = vram_cpu_view(g.bgvram, &g.bgsize);
    g.objmem = vram_cpu_view(g.objvram, &g.objsize);
    g.bgext = engine ? VRAM_BGEXTPAL_B : VRAM_BGEXTPAL_A;
    g.objext = engine ? VRAM_OBJEXTPAL_B : VRAM_OBJEXTPAL_A;
    g.bgpal = pal_bg(engine);
    g.objpal = pal_obj(engine);
    g.charofs = engine ? 0 : ((dc >> 24) & 7) * 0x10000u;
    g.scrofs = engine ? 0 : ((dc >> 27) & 7) * 0x10000u;
    {
        u16 mos = rd16(r, 0x4c);
        g.mosh = (mos & 15) + 1;
        g.mosv = ((mos >> 4) & 15) + 1;
    }

    /* which kind of layer each BG is in this mode */
    bgmode = dc & 7;
    for (i = 0; i < 4; i++) {
        u16 cnt = rd16(r, 0x08 + i * 2);
        int k = -1;
        if (dc & (0x100u << i)) {
            if (i < 2) k = 0;
            else {
                int m = bgmode;
                if (i == 2) {
                    if (m == 0 || m == 1 || m == 3) k = 0;
                    else if (m == 2 || m == 4) k = 2 + AFF_TILE8;
                    else if (m == 5) k = 2 + ((cnt & 0x80) ? ((cnt & 4) ? AFF_BMP16 : AFF_BMP8) : AFF_MAP16);
                    else if (m == 6 && !engine) k = 2 + AFF_LARGE;
                } else {
                    if (m == 0) k = 0;
                    else if (m == 1 || m == 2) k = 2 + AFF_TILE8;
                    else if (m >= 3 && m <= 5) k = 2 + ((cnt & 0x80) ? ((cnt & 4) ? AFF_BMP16 : AFF_BMP8) : AFF_MAP16);
                }
            }
            if (i == 1 && bgmode == 6) k = -1;
            if (i == 0 && !engine && (dc & 8)) k = 1;
        }
        bgkind[i] = k;
        bgprio[i] = cnt & 3;
        bgmos[i] = (cnt & 0x40) != 0 && k != 1;
    }
    /* Priorities are constant for the whole frame. Keep BG index as the stable
     * tie-breaker, rather than searching all four priorities for every pixel. */
    for (i = 0; i < 4; i++) {
        int at;
        if (bgkind[i] < 0) continue;
        at = nbg;
        while (at > 0 && bgprio[bgorder[at - 1]] > bgprio[i]) {
            bgorder[at] = bgorder[at - 1];
            at--;
        }
        bgorder[at] = i;
        nbg++;
    }

    bldcnt = rd16(r, 0x50);
    bldalpha = rd16(r, 0x52);
    bldy = rd16(r, 0x54);
    eva = bldalpha & 31; if (eva > 16) eva = 16;
    evb = (bldalpha >> 8) & 31; if (evb > 16) evb = 16;
    evy = bldy & 31; if (evy > 16) evy = 16;
    effect = (bldcnt >> 6) & 3;
    backdrop = col(g.bgpal[0]);
    win0h = rd16(r, 0x40); win1h = rd16(r, 0x42);
    win0v = rd16(r, 0x44); win1v = rd16(r, 0x46);
    winin = rd16(r, 0x48); winout = rd16(r, 0x4a);

    for (y = 0; y < DS_H; y++) {
        u32 *row = out + y * DS_W;
        int objon = (dc & 0x1000) != 0;
        int anywin = (dc & 0xe000) != 0;
        int w0 = (dc & 0x2000) && win_inside(y, win0v);
        int w1 = (dc & 0x4000) && win_inside(y, win1v);
        int ow = (dc & 0x8000) != 0;

        for (i = 0; i < 4; i++) {
            int ly = bgmos[i] ? y - y % g.mosv : y;
            memset(bgsrc[i], 0, sizeof bgsrc[i]);
            switch (bgkind[i]) {
            case -1: break;
            case 0: render_text(&g, i, ly, bgline[i]); break;
            case 1: render_3d(&g, frame3d, y, bgline[i]); break;
            default: render_affine(&g, i, bgkind[i] - 2, ly, bgline[i]); break;
            }
            if (bgkind[i] >= 0 && bgmos[i] && g.mosh > 1)
                for (x = 0; x < DS_W; x++) bgline[i][x] = bgline[i][x - x % g.mosh];
        }
        if (objon || ow) render_objs(&g, y, &ol);

        for (x = 0; x < DS_W; x++) {
            int mask, k, n = 0, l[2], obj_pending;
            u32 c[2], res;
            if (!anywin) mask = 0x3f;
            else if (w0 && win_inside(x, win0h)) mask = winin & 0x3f;
            else if (w1 && win_inside(x, win1h)) mask = (winin >> 8) & 0x3f;
            else if (ow && ol.win[x]) mask = (winout >> 8) & 0x3f;
            else mask = winout & 0x3f;

            obj_pending = objon && (mask & 0x10) && (ol.color[x] & OPAQUE);
            for (k = 0; k < nbg && n < 2; k++) {
                i = bgorder[k];
                if (!(mask & (1 << i)) || !(bgline[i][x] & OPAQUE)) continue;
                /* OBJ precedes a BG of equal priority, matching the DS rule. */
                if (obj_pending && ol.prio[x] <= bgprio[i]) {
                    l[n] = 4; c[n] = ol.color[x]; n++;
                    obj_pending = 0;
                    if (n == 2) break;
                }
                l[n] = i; c[n] = bgline[i][x]; n++;
            }
            if (obj_pending && n < 2) {
                l[n] = 4; c[n] = ol.color[x]; n++;
            }
            while (n < 2) { l[n] = 5; c[n] = backdrop; n++; }

            res = c[0] & 0xffffff;
            if (mask & 0x20) {
                int done = 0;
                if (l[0] == 4 && (ol.flags[x] & (OBJF_SEMI | OBJF_BITMAP)) && (bldcnt & (0x100 << l[1]))) {
                    if (ol.flags[x] & OBJF_BITMAP) {
                        int a = ol.alpha[x] + 1;
                        res = blend(c[0], c[1], a, 16 - a);
                    } else {
                        res = blend(c[0], c[1], eva, evb);
                    }
                    done = 1;
                }
                if (!done && (bldcnt & (1 << l[0]))) {
                    if (effect == 1) {
                        if (bldcnt & (0x100 << l[1])) res = blend(c[0], c[1], eva, evb);
                    } else if (effect == 2) {
                        res = brighten(res, evy);
                    } else if (effect == 3) {
                        res = darken(res, evy);
                    }
                }
            }
            row[x] = res;
            /* source of the pixel, if it's shown unchanged (no effect or mosaic) */
            {
                u32 sv = 0;
                if (res == (c[0] & 0xffffff)) {
                    if (l[0] == 4)
                        sv = ol.src[x];
                    else if (l[0] < 4 && bgkind[l[0]] == 1)
                        sv = GPU2D_SRC_3D;
                    else if (l[0] < 4 && !bgmos[l[0]])
                        sv = bgsrc[l[0]][x];
                }
                gpu2d_src[engine][y * DS_W + x] = sv;
            }
        }
        master_brightness(r, row);
    }
}

