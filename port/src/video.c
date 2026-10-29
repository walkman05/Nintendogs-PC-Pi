/* Per-frame video: at each VBlank, render the 3D frame if a new one was swapped, both 2D engines, and present
 * them (POWCNT1 bit 15 chooses which engine is on the top screen). Also feeds window input to the DS. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ds_video.h"

static u8 io[0x2000];
static u32 frame3d[DS_W * DS_H];
static u32 screen_a[DS_W * DS_H], screen_b[DS_W * DS_H];
static int opened;

/* High resolution output (scale S = gx_scale(), 1-4): the 2D engines render at 256x192 and record each pixel's
 * source (gpu2d_src); the screens are then composed at 256S x 192S, taking 3D pixels from the high resolution 3D
 * frame and pixels of a captured bitmap (the other screen's 3D, shown through display capture) from a high
 * resolution copy of that capture. Everything else is the native pixel, enlarged. */
#define MAXS 4
static u32 hi_a[DS_W * DS_H * MAXS * MAXS], hi_b[DS_W * DS_H * MAXS * MAXS];
typedef struct {
    int valid, s, w, h;
    u32 dofs;  /* halfword offset in the bank */
    u32 img[DS_W * DS_H * MAXS * MAXS];
} HiCapture;
static HiCapture hicap[4];  /* banks A-D */

/* NDOGS_SHOT=<frame>[,<frame>...]: save both screens (top above bottom) to shot_<frame>.bmp in the current folder */
static void save_bmp(int frame, const u32 *top, int tw, int th, const u32 *bottom, int bw, int bh)
{
    char name[64];
    FILE *f;
    u32 hdr[13], black[DS_W * 2 * 4] = { 0 };
    int y, w = tw > bw ? tw : bw, h = th + bh;  /* each screen centred (the 3D one may be wider) */
    snprintf(name, sizeof name, "shot_%d.bmp", frame);
    f = fopen(name, "wb");
    if (!f)
        return;
    memset(hdr, 0, sizeof hdr);
    fwrite("BM", 1, 2, f);
    hdr[0] = 54 + (u32)w * h * 4;
    hdr[2] = 54;
    hdr[3] = 40;
    hdr[4] = (u32)w;
    hdr[5] = (u32)h;
    hdr[6] = 1 | (32 << 16);
    fwrite(hdr, 4, 13, f);
    for (y = h - 1; y >= 0; y--) {
        int sw = y < th ? tw : bw, pad = (w - sw) / 2;
        fwrite(black, 4, (size_t)pad, f);
        fwrite(y < th ? top + y * tw : bottom + (y - th) * bw, 4, (size_t)sw, f);
        fwrite(black, 4, (size_t)(w - sw - pad), f);
    }
    fclose(f);
}

/* compose one engine's screen at scale S into out (see above); bright: apply the engine's master brightness to
 * the high resolution pixels (native pixels already have whatever the engine applied). Bands of rows run on the
 * worker threads (idle while the 3D is drawn by the GPU). */
typedef struct {
    int engine, S, bright, bands;
    const u32 *native;
    u32 *out;
} ComposeJob;

static void compose_band(int band, void *arg)
{
    const ComposeJob *j = arg;
    int engine = j->engine, S = j->S, bright = j->bright;
    const u32 *src = gpu2d_src[engine], *h3d = gx_hires(), *native = j->native;
    u32 *out = j->out;
    int W = DS_W * S, H3 = gx_stride(), x, y, sx, sy;
    int hofs = engine ? 0 : *(u16 *)(io + 0x10) & 0x1ff;
    for (y = DS_H * band / j->bands; y < DS_H * (band + 1) / j->bands; y++)
        for (x = 0; x < DS_W; x++) {
            u32 sv = src[y * DS_W + x], c = native[y * DS_W + x] & 0xffffff;
            u32 *o = out + y * S * W + x * S;
            const u32 *hp = 0;
            int hw = 0;
            if (sv == GPU2D_SRC_3D) {
                int tx = (x + hofs) & 511;
                if (tx < DS_W) {
                    hp = h3d + y * S * H3 + tx * S;
                    hw = H3;
                }
            } else if (sv & GPU2D_SRC_VRAM) {
                u32 boff, rel;
                int b = vram_bank_at((int)((sv >> 24) & 0x7f), sv & 0xffffff, &boff);
                if (b >= 0 && b < 4 && hicap[b].valid && hicap[b].s == S) {
                    HiCapture *k = &hicap[b];
                    rel = ((boff >> 1) - k->dofs) & 0xffff;
                    if (rel < (u32)(k->w * k->h)) {
                        hw = k->w * S;
                        hp = k->img + (rel / k->w) * S * hw + (rel % k->w) * S;
                    }
                }
            }
            for (sy = 0; sy < S; sy++)
                for (sx = 0; sx < S; sx++) {
                    u32 v = c;
                    if (hp) {
                        u32 h = hp[sy * hw + sx];
                        if (h >> 31)
                            v = bright ? gpu2d_bright_pixel(engine, io, h) : h & 0xffffff;
                    }
                    o[sy * W + sx] = v;
                }
        }
}

static int row_bands(int rows)
{
    int n = workers_count() * 2;
    return n > rows ? rows : n < 1 ? 1 : n;
}

static void compose(int engine, const u32 *native, u32 *out, int S, int bright)
{
    ComposeJob j;
    j.engine = engine;
    j.S = S;
    j.bright = bright;
    j.native = native;
    j.out = out;
    j.bands = row_bands(DS_H);
    workers_run(j.bands, compose_band, &j);
}

static u16 rgb15(u32 c)
{
    return (u16)(((c >> 19) & 31) | ((c >> 11) & 31) << 5 | ((c >> 3) & 31) << 10);
}

/* Display capture (DISPCAPCNT 0x04000064): engine A's picture (before master brightness) or the 3D layer,
 * optionally blended with a VRAM bitmap, written into VRAM bank A-D as 15-bit pixels with bit 15 set. Games
 * with 3D on both screens swap the screens every frame and show the captured previous frame on the other one. */
static int cap_bank = -1;  /* this frame's display capture: the bank it filled, if a whole plain screen (see wcap) */
static void display_capture(const u32 *graphics)
{
    u32 cap = *(u32 *)(io + 0x64), dc = *(u32 *)(io + 0);
    static const int widths[4] = { 128, 256, 256, 256 }, heights[4] = { 128, 64, 128, 192 };
    int w = widths[(cap >> 20) & 3], h = heights[(cap >> 20) & 3], src = (cap >> 29) & 3;
    cap_bank = w == DS_W && h == DS_H && src == 0 ? (int)((cap >> 16) & 3) : -1;
    int eva = cap & 31, evb = (cap >> 8) & 31, x, y;
    u16 *dst = vram_bank_ptr((cap >> 16) & 3), *vsrc = vram_bank_ptr((dc >> 18) & 3);
    u32 dofs = ((cap >> 18) & 3) * 0x4000u, sofs = ((cap >> 26) & 3) * 0x4000u;  /* in halfwords */
    if (eva > 16) eva = 16;
    if (evb > 16) evb = 16;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            u32 a = cap & (1u << 24) ? frame3d[y * DS_W + x] : graphics[y * DS_W + x] | 0x80000000u;
            u16 pa = (a & 0x80000000u) ? (u16)(rgb15(a) | 0x8000) : 0, pb, out;
            u32 i = (u32)(y * w + x);
            pb = cap & (1u << 25) ? 0 : vsrc[(sofs + i) & 0xffff];
            if (src == 0)
                out = pa;
            else if (src == 1)
                out = pb;
            else {
                int fa = pa & 0x8000 ? eva : 0, fb = pb & 0x8000 ? evb : 0, ch, sh;
                out = (fa || fb) ? 0x8000 : 0;
                for (sh = 0; sh < 15; sh += 5) {
                    ch = (((pa >> sh) & 31) * fa + ((pb >> sh) & 31) * fb + 8) >> 4;
                    out |= (u16)((ch > 31 ? 31 : ch) << sh);
                }
            }
            dst[(dofs + i) & 0xffff] = out;
        }
    {
        HiCapture *k = &hicap[(cap >> 16) & 3];
        int S = gx_scale();
        k->valid = 0;
        if (S > 1 && src == 0) {  /* only plain captures of engine A or the 3D layer have a high resolution form */
            const u32 *from = cap & (1u << 24) ? gx_hires() : hi_a;
            int stride = cap & (1u << 24) ? gx_stride() : DS_W * S;
            for (y = 0; y < h * S; y++)
                memcpy(k->img + y * w * S, from + y * stride, (size_t)w * S * 4);
            k->valid = 1;
            k->s = S;
            k->w = w;
            k->h = h;
            k->dofs = dofs;
        }
    }
    hw_capture_done();
}

/* engine A (the one with the 3D layer) from a 3D frame, at native size and composed at scale S; the display
 * capture is taken from real frames only */
static void render_a(const u32 *f3d, u32 *scr, u32 *hi, int S, int capture)
{
    if (*(u32 *)(io + 0x64) & 0x80000000u) {
        gpu2d_no_brightness = 1;
        gpu2d_render(0, io, f3d, scr);
        gpu2d_no_brightness = 0;
        if (S > 1)
            compose(0, scr, hi, S, 0);
        if (capture)
            display_capture(scr);
        gpu2d_brightness(0, io, scr);
        if (S > 1 && (*(u16 *)(io + 0x6c) >> 14) % 3 && (*(u16 *)(io + 0x6c) & 31)) {
            int i;
            for (i = 0; i < DS_W * DS_H * S * S; i++)
                hi[i] = gpu2d_bright_pixel(0, io, hi[i]);
        }
    } else {
        gpu2d_render(0, io, f3d, scr);
        if (S > 1)
            compose(0, scr, hi, S, 1);
    }
}

/* ---- high frame rate and widescreen -----------------------------------------------------------------------------
 * The game runs at 60 frames a second (its logic counts frames). Above that, frames in between are presented while
 * the game waits for the next VBlank (hw_halt), interpolated between the game's frames: the 3D with its geometry
 * and skeletons interpolated (gpu3d.c); the 2D from the registers and OAM of both frames, with sprite positions and
 * rotation/scale, background scrolling and rotation, and fades interpolated (anything that changed identity or
 * jumped is taken from the newer frame). One screen with 3D: between the last two frames, shown one frame late.
 * 3D on both screens (the game swaps the screens every frame, drawing each one's 3D every other frame and showing
 * it on the other screen through display capture): each screen between the two last frames it was drawn in, both
 * rendered here from their own 3D, shown two frames late. Rate: NDOGS_FPS, else 120 or the monitor's refresh rate
 * if lower (Windows); off at 60 or below and with NDOGS_FPS=0. Frames are presented on a steady grid; if the
 * machine can't keep up, the rate drops to what it can do.
 * Widescreen (gx_set_view): the screen with the 3D (or both) is shown wider, or taller, to fill the window
 * (window_view), the extra coming from the 3D renderer's wider or taller view; the 2D stays in the middle. */
extern double now_ms(void);
#define WMAX 2
/* What engine A's 2D does to the 3D at one pixel of the screen's edge: out = b + a * 3D / 256 per channel (blends,
 * fades, tints are of this form), tr where the 3D has nothing; cov: an opaque 2D pixel covers the 3D there. */
typedef struct { u16 a[3]; u8 b[3], tr[3], cov; } Edge;
typedef struct {
    u8 io[0x2000], io3d[0x2000];  /* registers at the frame, and at the VBlank of the 3D list it showed */
    Edge el[DS_H], er[DS_H], et[DS_W], eb[DS_W];  /* left, right, top, bottom edges (view extended: see widen) */
    int edges;                    /* they were measured */
    u16 oam[0x400];               /* OAM of both engines */
    double time;
    unsigned seq;                 /* that 3D list (gx_list_seq) */
} Frame;
static Frame fh[4];               /* the last four frames (fr(0) the newest) */
static int fh_head, fh_count;
static int dual_on, dual_swaps, dual_still;  /* 3D on both screens (see dual) */
static u8 io3d[0x2000];           /* registers of the last 3D list's VBlank */
static u8 io_i[0x2000], io_save[0x2000];
static u16 oam_i[0x400];
static u32 i3d[DS_W * DS_H], scr_i[2][DS_W * DS_H], hi_i[2][DS_W * DS_H * MAXS * MAXS];
static u32 i_b[DS_W * DS_H], hi_ib[DS_W * DS_H * MAXS * MAXS];
static u32 wide_buf[2][DS_W * DS_H * WMAX * MAXS * MAXS];  /* a screen up to twice the area */
static double i_period, i_cost, i_last;  /* target ms per frame (0 off), measured cost, last present */
static int raster_skipped;               /* the shown 3D list wasn't rasterised at its VBlank (gx_take_swapped) */
static unsigned i_presents;              /* frames presented (NDOGS_JANK) */
static const u32 *i_top, *i_bottom;      /* the last real frame's screens */
static int i_S;
extern int window_refresh_hz(void);

static Frame *fr(int k) { return &fh[(fh_head - k) & 3]; }
static u32 fpow(int k) { return *(u32 *)(fr(k)->io + 0x304); }
static int has3d(int k) { return (*(u32 *)fr(k)->io & 0x108) == 0x108; }  /* BG0 on, showing 3D */

static void interp_init(void)
{
    static int done;
    int hz;
    if (done)
        return;
    done = 1;
    if (getenv("NDOGS_FPS"))
        hz = atoi(getenv("NDOGS_FPS"));
    else {  /* default: 120, or the monitor's rate if lower */
        hz = window_refresh_hz();
        hz = hz > 120 ? 120 : hz;
    }
    i_period = hz > 60 ? 1000.0 / hz : 0;
    if (i_period)
        fprintf(stderr, "video: up to %d fps (game logic 60, frames in between interpolated)\n", hz);
}

/* Measure this frame's edge transforms (Edge): engine A rendered over a black, a white and an empty 3D layer. */
static void edges_keep(Frame *f)
{
    static u32 flat[3][DS_W * DS_H], out[3][DS_W * DS_H];
    static int init;
    int i, k, c;
    if (!init) {
        for (i = 0; i < DS_W * DS_H; i++)
            flat[0][i] = 0x80000000u, flat[1][i] = 0x80ffffffu, flat[2][i] = 0;
        init = 1;
    }
    for (k = 0; k < 3; k++)
        gpu2d_render(0, io, flat[k], out[k]);
    for (i = 0; i < DS_H + DS_H + DS_W + DS_W; i++) {
        int idx = i < DS_H ? i * DS_W : i < 2 * DS_H ? (i - DS_H) * DS_W + DS_W - 1 :
                  i < 2 * DS_H + DS_W ? i - 2 * DS_H : (DS_H - 1) * DS_W + (i - 2 * DS_H - DS_W);
        Edge *e = i < DS_H ? &f->el[i] : i < 2 * DS_H ? &f->er[i - DS_H] :
                  i < 2 * DS_H + DS_W ? &f->et[i - 2 * DS_H] : &f->eb[i - 2 * DS_H - DS_W];
        int sum = 0;
        for (c = 0; c < 3; c++) {
            int lo = (out[0][idx] >> (c * 8)) & 255, hi = (out[1][idx] >> (c * 8)) & 255;
            int a = hi > lo ? (hi - lo) * 256 / 255 : 0;
            e->a[c] = (u16)a;
            e->b[c] = (u8)lo;
            e->tr[c] = (u8)(out[2][idx] >> (c * 8));
            sum += a;
        }
        e->cov = sum < 3 * 26;  /* the 3D shows through less than a tenth */
    }
    f->edges = 1;
}

/* video_frame: keep this frame */
static void interp_keep(void)
{
    Frame *f;
    fh_head++;
    if (fh_count < 4)
        fh_count++;
    f = fr(0);
    memcpy(f->io, io, sizeof f->io);
    memcpy(f->io3d, io3d, sizeof f->io3d);
    memcpy(f->oam, (const void *)0x07000000u, sizeof f->oam);
    f->time = now_ms();
    f->seq = gx_list_seq();
    f->edges = 0;
    if (fh_count >= 2) {  /* the game falling behind (60 frames taking over 1.05 s: under ~57 fps; single slow
                           * frames don't count, the schedule catches up after them): first no interpolation for a
                           * few seconds, then a lower resolution; ten seconds at full speed: one higher */
        static int frames, slow, good;
        static unsigned presents0;
        static double t0;
        extern void window_slow(void), window_fast(void);
        if (!frames)
            t0 = f->time;
        if (++frames == 61) {
            frames = 0;
            slow = f->time - t0 > 60 * 1000.0 / 60 * 1.05 ? 13 : 0;
            int on = i_period && i_cost * 1.25 <= 1000.0 / 60 * 0.6;
            if (on && i_presents - presents0 < 60)
                i_cost = 40.0;  /* interpolating, but showing no more frames than the game makes: off for now */
            presents0 = i_presents;
            if (slow > 12) {
                if (i_period && i_cost * 1.25 <= 1000.0 / 60 * 0.6)
                    i_cost = 40.0;  /* interpolation off (decays: tried again in ~13 s) */
                else
                    window_slow();
                good = 0;
            } else if (++good >= 10) {
                window_fast();
                good = 0;
            }
        }
    }
    if ((gx_wide_px() > DS_W || gx_tall_px() > DS_H) && has3d(0))
        edges_keep(f);
    if (fh_count >= 2) {  /* 3D on both screens: on after three swaps in a row, off after two frames without */
        if ((fpow(0) ^ fpow(1)) & 0x8000)
            dual_swaps++, dual_still = 0;
        else
            dual_still++, dual_swaps = 0;
        if (dual_swaps >= 3)
            dual_on = 1;
        if (dual_still >= 2)
            dual_on = 0;
    }
}

/* 3D on both screens: the game swaps the screens every frame (sticky, so a frame's hiccup doesn't switch the
 * screens' size back and forth) */
static int dual(void)
{
    return dual_on && fh_count >= 4;
}

/* the screens stayed put over the last two frames */
static int steady(void)
{
    return fh_count >= 2 && !((fpow(0) ^ fpow(1)) & 0x8000);
}

/* a value of <bits> bits that wraps (scroll offsets, sprite coordinates): the short way round, if under <max> */
static u32 lerp_wrap(u32 a, u32 b, int bits, int max, float t)
{
    int m = 1 << bits, d = (int)((b - a) & (m - 1));
    if (d >= m / 2)
        d -= m;
    if (d > max || d < -max)
        return b;
    return (a + (u32)(int)(d * t + (d < 0 ? -0.5f : 0.5f))) & (m - 1);
}

static s32 lerp_s(s32 a, s32 b, s32 max, float t)
{
    s32 d = b - a;
    return d > max || d < -max ? b : a + (s32)(d * t + (d < 0 ? -0.5f : 0.5f));
}

static u16 r16(const u8 *r, int o) { return (u16)(r[o] | r[o + 1] << 8); }
static void w16(u8 *r, int o, u16 v) { r[o] = (u8)v; r[o + 1] = (u8)(v >> 8); }
static s32 r28(const u8 *r, int o) { return (s32)((u32)(r16(r, o) | r16(r, o + 2) << 16) << 4) >> 4; }
static void w28(u8 *r, int o, s32 v) { w16(r, o, (u16)v); w16(r, o + 2, (u16)(((u32)v >> 16) & 0x0fff)); }

/* registers and OAM at t between frames fa (older) and fb (into io_i, oam_i) */
static void interp_2d(float t, const Frame *fa, const Frame *fb)
{
    int e, i, k;
    memcpy(io_i, fb->io, sizeof io_i);
    memcpy(oam_i, fb->oam, sizeof oam_i);
    for (e = 0; e < 2; e++) {
        const u8 *a = fa->io + e * 0x1000, *b = fb->io + e * 0x1000;
        u8 *o = io_i + e * 0x1000;
        u16 *oa = oam_i + e * 0x200;
        const u16 *pa = fa->oam + e * 0x200, *pb = fb->oam + e * 0x200;
        for (i = 0; i < 4; i++) {  /* BG scrolling (9 bit) */
            int hv;
            for (hv = 0; hv < 2; hv++) {
                int off = 0x10 + i * 4 + hv * 2;
                w16(o, off, (u16)lerp_wrap(r16(a, off) & 0x1ff, r16(b, off) & 0x1ff, 9, 64, t));
            }
        }
        for (i = 0; i < 2; i++) {  /* BG2/BG3 rotation and scaling: matrix (8.8) and reference point (20.8) */
            int base = 0x20 + i * 0x10;
            for (k = 0; k < 4; k++)
                w16(o, base + k * 2, (u16)lerp_s((s16)r16(a, base + k * 2), (s16)r16(b, base + k * 2), 0x100, t));
            for (k = 0; k < 2; k++)
                w28(o, base + 8 + k * 4, lerp_s(r28(a, base + 8 + k * 4), r28(b, base + 8 + k * 4), 64 << 8, t));
        }
        /* fades: blend weights, fade level, master brightness (same mode) */
        w16(o, 0x52, (u16)(lerp_s(r16(a, 0x52) & 31, r16(b, 0x52) & 31, 31, t) |
                           lerp_s((r16(a, 0x52) >> 8) & 31, (r16(b, 0x52) >> 8) & 31, 31, t) << 8));
        w16(o, 0x54, (u16)lerp_s(r16(a, 0x54) & 31, r16(b, 0x54) & 31, 31, t));
        if ((r16(a, 0x6c) ^ r16(b, 0x6c)) >> 14 == 0)
            w16(o, 0x6c, (u16)((r16(b, 0x6c) & 0xffe0) | lerp_s(r16(a, 0x6c) & 31, r16(b, 0x6c) & 31, 31, t)));
        /* sprites: the game reuses OAM slots for different sprites from frame to frame, so a sprite is matched by
         * what it is (tile, palette, shape, size, mode, rotation/flip settings), with the one nearest its new
         * place in the older frame, in any slot; one with no match within the limit is shown where it is now.
         * Each older sprite is used once. Affine matrices turn. */
        {
            u8 used[128];
            memset(used, 0, sizeof used);
            for (i = 0; i < 128; i++) {
                const u16 *y = pb + i * 4;
                int j, best = -1, bd = 64 * 2 + 1;
                if ((y[0] & 0x300) == 0x200)  /* hidden */
                    continue;
                for (j = 0; j < 128; j++) {
                    const u16 *x = pa + j * 4;
                    int dx, dy;
                    if (used[j] || (x[0] & 0xff00) != (y[0] & 0xff00) || (x[1] & 0xfe00) != (y[1] & 0xfe00) ||
                        x[2] != y[2])
                        continue;
                    dx = (int)(((y[1] & 0x1ff) - (x[1] & 0x1ff)) & 0x1ff);
                    dx = dx >= 256 ? 512 - dx : dx;
                    dy = (int)(((y[0] & 0xff) - (x[0] & 0xff)) & 0xff);
                    dy = dy >= 128 ? 256 - dy : dy;
                    if (dx <= 64 && dy <= 64 && dx + dy < bd) {
                        bd = dx + dy;
                        best = j;
                        if (!bd)
                            break;
                    }
                }
                if (best < 0)
                    continue;
                used[best] = 1;
                oa[i * 4] = (u16)((y[0] & 0xff00) | lerp_wrap(pa[best * 4] & 0xff, y[0] & 0xff, 8, 64, t));
                oa[i * 4 + 1] = (u16)((y[1] & 0xfe00) | lerp_wrap(pa[best * 4 + 1] & 0x1ff, y[1] & 0x1ff, 9, 64, t));
            }
        }
        for (i = 0; i < 32; i++)
            for (k = 0; k < 4; k++) {
                int idx = i * 16 + k * 4 + 3;
                oa[idx] = (u16)lerp_s((s16)pa[idx], (s16)pb[idx], 0x80, t);
            }
    }
}

/* Widescreen: engine A's screen (scr, a DS screen at scale S) with the 3D renderer's extra width or height around
 * it (from the frame just rendered; where it has nothing, the backdrop), master brightness applied as on the
 * screen. Into out; its size in *w x *h. Uses the registers in io. */
typedef struct {
    const u32 *scr, *h3;
    u32 *out;
    const Frame *f;
    int S, W, H, sw, sh, ox, oy, st, bright, bands;
    u32 back;
} WidenJob;

static void widen_band(int band, void *arg)
{
    const WidenJob *j = arg;
    const Frame *f = j->f;
    int S = j->S, W = j->W, sw = j->sw, sh = j->sh, ox = j->ox, oy = j->oy, bright = j->bright, x, y;
    for (y = j->H * band / j->bands; y < j->H * (band + 1) / j->bands; y++) {
        const u32 *hr = j->h3 + y * j->st;
        u32 *o = j->out + y * W;
        int inside = y >= oy && y < oy + sh;
        if (inside)
            memcpy(o + ox, j->scr + (y - oy) * sw, (size_t)sw * 4);
        for (x = 0; x < W; x++) {
            u32 c;
            if (inside && x == ox)
                x += sw;
            if (x >= W)
                break;
            c = hr[x];
            if (f && f->edges) {  /* continue what the 2D does at the nearest edge pixel */
                const Edge *e;
                if (inside)
                    e = x < ox ? &f->el[(y - oy) / S] : &f->er[(y - oy) / S];
                else {
                    int col = x < ox ? 0 : x >= ox + sw ? DS_W - 1 : (x - ox) / S;
                    e = y < oy ? &f->et[col] : &f->eb[col];
                }
                if (!(c >> 31))
                    o[x] = (u32)e->tr[0] | (u32)e->tr[1] << 8 | (u32)e->tr[2] << 16;
                else if (!e->cov) {
                    u32 v = 0;
                    int ch;
                    for (ch = 0; ch < 3; ch++) {
                        int r = e->b[ch] + (int)(((c >> (ch * 8)) & 255) * e->a[ch] >> 8);
                        v |= (u32)(r > 255 ? 255 : r) << (ch * 8);
                    }
                    o[x] = v;
                } else
                    o[x] = bright ? gpu2d_bright_pixel(0, io, c) : c & 0xffffff;
                continue;
            }
            o[x] = c >> 31 ? (bright ? gpu2d_bright_pixel(0, io, c) : c & 0xffffff) : j->back;
        }
    }
}

static const u32 *widen(const u32 *scr, int S, u32 *out, int *w, int *h, const Frame *f)
{
    int wpx = gx_wide_px(), hpx = gx_tall_px();
    u16 bd = *(volatile u16 *)0x05000000u;  /* engine A's backdrop colour */
    WidenJob j;
    if ((wpx == DS_W && hpx == DS_H) || gx_scale() != S) {
        *w = DS_W * S;
        *h = DS_H * S;
        return scr;
    }
    j.scr = scr;
    j.h3 = gx_hires_wide();
    j.out = out;
    j.f = f;
    j.S = S;
    j.W = wpx * S;
    j.H = hpx * S;
    j.sw = DS_W * S;
    j.sh = DS_H * S;
    j.ox = (wpx - DS_W) / 2 * S;
    j.oy = (hpx - DS_H) / 2 * S;
    j.st = gx_stride();
    j.bright = (*(u16 *)(io + 0x6c) >> 14) % 3 && (*(u16 *)(io + 0x6c) & 31);
    j.back = (u32)((bd & 31) * 255 / 31) << 16 | (u32)(((bd >> 5) & 31) * 255 / 31) << 8 | ((bd >> 10) & 31) * 255 / 31;
    if (j.bright)
        j.back = gpu2d_bright_pixel(0, io, j.back);
    j.bands = row_bands(j.H);
    workers_run(j.bands, widen_band, &j);
    *w = j.W;
    *h = j.H;
    return out;
}

/* Widened captures: when the game captures a whole screen while the view is widened, the widened screen is kept
 * per VRAM bank, so that a screen later showing that capture (the top of the door after the intro: a still of the
 * intro's last frame) is shown widened too. */
typedef struct { int valid, S, W, H; u32 img[DS_W * DS_H * WMAX * MAXS * MAXS]; } WideCap;
static WideCap *wcap[4];

static void wcap_keep(int bank, const u32 *wide, int S, int W, int H)
{
    if (!wcap[bank] && !(wcap[bank] = malloc(sizeof *wcap[bank])))
        return;
    memcpy(wcap[bank]->img, wide, (size_t)W * H * 4);
    wcap[bank]->S = S, wcap[bank]->W = W, wcap[bank]->H = H;
    wcap[bank]->valid = 1;
}

/* engine B's screen scr, if it is showing a whole widened capture: the capture's widened sides around it (into out,
 * size in *w x *h); else scr itself */
static const u32 *widen_cap(const u32 *scr, int S, u32 *out, int *w, int *h)
{
    static const int pts[5][2] = { { 128, 96 }, { 16, 16 }, { 239, 16 }, { 16, 175 }, { 239, 175 } };
    int k, bank = -1, W, H, sw = DS_W * S, ox, oy, y;
    const WideCap *c;
    *w = sw;
    *h = DS_H * S;
    for (k = 0; k < 5; k++) {  /* all over the screen, pixels of one bank's bitmap */
        u32 sv = gpu2d_src[1][pts[k][1] * DS_W + pts[k][0]], boff;
        int b;
        if (!(sv & GPU2D_SRC_VRAM))
            return scr;
        b = vram_bank_at((int)((sv >> 24) & 0x7f), sv & 0xffffff, &boff);
        if (b < 0 || b > 3 || (bank >= 0 && b != bank))
            return scr;
        bank = b;
    }
    c = wcap[bank];
    if (!c || !c->valid || c->S != S)
        return scr;
    W = c->W, H = c->H;
    ox = (W - sw) / 2, oy = (H - DS_H * S) / 2;
    memcpy(out, c->img, (size_t)W * H * 4);
    for (y = 0; y < DS_H * S; y++)  /* the screen as it is now in the middle */
        memcpy(out + (oy + y) * W + ox, scr + y * sw, (size_t)sw * 4);
    *w = W;
    *h = H;
    return out;
}

/* Engine A's screen at t between frames fa and fb (its 2D, and the 3D lists they showed: ages relative to the newest
 * list), widened, into buffers k; *w x *h its size. 0 if the 3D can't be interpolated. */
static const u32 *screen_a_at(const Frame *fa, const Frame *fb, float t, int S, int k, int *w, int *h)
{
    unsigned seq = gx_list_seq();
    const u32 *s;
    if (seq - fb->seq > 3 || seq - fa->seq > 3 || !gx_render_pair(fb->io3d, (int)(seq - fb->seq), (int)(seq - fa->seq),
                                                                   t, i3d))
        return 0;  /* (its list is gone: the game hasn't drawn 3D for a while) */
    render_a(i3d, scr_i[k], hi_i[k], S, 0);
    s = S > 1 ? hi_i[k] : scr_i[k];
    return widen(s, S, wide_buf[k], w, h, fb);
}

/* What to show at <now> (latest: the newest frames, not interpolated): both screens into *top/*bottom, their sizes
 * into *tw x *th, *bw x *bh. 1 if rendered here (interpolated or widened), 0 if the last real frame's screens are
 * to be shown as they are. */
static int compose_now(double now, int latest, const u32 **top, int *tw, int *th, const u32 **bottom, int *bw,
                       int *bh, int S)
{
    int wide = (gx_wide_px() > DS_W || gx_tall_px() > DS_H) && gx_scale() == S, k;
    *tw = *bw = DS_W * S;
    *th = *bh = DS_H * S;
    if (dual() && (i_period || wide)) {
        /* each screen between the last two frames it was drawn in: frames 0 and 2, frames 1 and 3 */
        const u32 *out[2];
        int w[2], h[2], ok = 1;
        memcpy(io_save, io, sizeof io);
        for (k = 0; k < 2 && ok; k++) {
            const Frame *fa = fr(k + 2), *fb = fr(k);
            double span = fb->time - fa->time;
            float t;
            if (((fpow(k) ^ fpow(k + 2)) & 0x8000) || span <= 0 || span > 100.0 || gx_list_seq() - fa->seq > 3)
                fa = fb, span = 1;  /* no earlier frame of this screen to come from: as it is now */
            t = latest || fa == fb ? 1.0f : (float)((now - fb->time) / span);
            t = t < 0 ? 0 : t > 1 ? 1 : t;
            interp_2d(t, fa, fb);
            memcpy(io, io_i, sizeof io);
            gpu2d_oam = oam_i;
            out[k] = screen_a_at(fa, fb, t, S, k, &w[k], &h[k]);
            gpu2d_oam = 0;
            ok = out[k] != 0;
        }
        memcpy(io, io_save, sizeof io);
        if (ok) {
            int a_top = fpow(0) & 0x8000;  /* engine A, now drawing frame 0's screen, is on top */
            *top = out[a_top ? 0 : 1], *tw = w[a_top ? 0 : 1], *th = h[a_top ? 0 : 1];
            *bottom = out[a_top ? 1 : 0], *bw = w[a_top ? 1 : 0], *bh = h[a_top ? 1 : 0];
            return 1;
        }
        return 0;  /* (not widened: the screens would change size every frame) */
    }
    if (i_period && !latest && steady()) {  /* one frame late, between the last two */
        const Frame *fa = fr(1), *fb = fr(0);
        double span = fb->time - fa->time;
        float t = (float)((now - fb->time) / span);
        const u32 *f3d = frame3d, *a, *b;
        int w, h, wb, hb, b_same;
        if (span > 0 && span <= 50.0) {
            t = t < 0 ? 0 : t > 1 ? 1 : t;
            if (gx_interp_pos(now) >= 0 && gx_render_pair(fb->io3d, 0, 1, t, i3d))  /* 3D at the 2D's position */
                f3d = i3d;
            interp_2d(t, fa, fb);
            memcpy(io_save, io, sizeof io);
            memcpy(io, io_i, sizeof io);
            gpu2d_oam = oam_i;
            /* engine B: its registers and sprites the same in both frames, the real frame's picture is it */
            b_same = !memcmp(fa->io + 0x1000, fb->io + 0x1000, 0x1000) && !memcmp(fa->oam + 0x200, fb->oam + 0x200, 0x400);
            if (!b_same) {
                gpu2d_render(1, io, 0, i_b);
                if (S > 1)
                    compose(1, i_b, hi_ib, S, 1);
            }
            render_a(f3d, scr_i[0], hi_i[0], S, 0);
            a = S > 1 ? hi_i[0] : scr_i[0];
            b = b_same ? (S > 1 ? hi_b : screen_b) : S > 1 ? hi_ib : i_b;
            b = widen_cap(b, S, wide_buf[1], &wb, &hb);
            if (has3d(0))
                a = widen(a, S, wide_buf[0], &w, &h, fb);
            else
                w = DS_W * S, h = DS_H * S;
            gpu2d_oam = 0;
            memcpy(io, io_save, sizeof io);
            if (fpow(0) & 0x8000)
                *top = a, *tw = w, *th = h, *bottom = b, *bw = wb, *bh = hb;
            else
                *top = b, *tw = wb, *th = hb, *bottom = a, *bw = w, *bh = h;
            return 1;
        }
    }
    if (raster_skipped && !dual()) {  /* the real frame is shown after all: draw the 3D its VBlank skipped */
        int sc = gx_scale();
        if (sc == S && gx_render_pair(fr(0)->io3d, 0, 0, 1.0f, frame3d)) {
            render_a(frame3d, screen_a, hi_a, S, 0);
            raster_skipped = 0;
        }
    }
    if (wide && has3d(0) && !dual()) {  /* the real frame, the 3D screen widened (and a widened capture shown) */
        const u32 *a = S > 1 ? hi_a : screen_a;
        if (*top == a) {
            *top = widen(a, S, wide_buf[0], tw, th, fr(0));
            *bottom = widen_cap(*bottom, S, wide_buf[1], bw, bh);
        } else {
            *bottom = widen(a, S, wide_buf[0], bw, bh, fr(0));
            *top = widen_cap(*top, S, wide_buf[1], tw, th);
        }
        return 1;
    }
    return 0;
}

static int interp_active(void);

/* present: interpolated to now if possible, else the last real frame */
static int present(const u32 *top, const u32 *bottom, int S)
{
    double t0 = now_ms();
    int act = interp_active(), tw, th, bw, bh, r, in = compose_now(t0, !act, &top, &tw, &th, &bottom, &bw, &bh, S);
    r = window_present(top, tw, th, bottom, bw, bh, DS_W * S, DS_H * S);
    i_presents++;
    if (in && act) {  /* adapt the rate to the machine: an interpolated frame must leave time for the game */
        double c = now_ms() - t0;
        i_cost = i_cost ? i_cost * 0.9 + c * 0.1 : c;
    }
    return r;
}

/* While interpolating, frames are presented on a steady grid of the target period (or what the machine manages),
 * independent of the game's frames, which only bring new data; the game's own present is skipped (video_frame). */
static int interp_active(void)
{
    /* only while a frame in between costs clearly less than a game frame: else the game's own frames are shown
     * (60 fps beats interpolated frames that can't keep up). The cost estimate decays while off (video_frame), so
     * this is tried again now and then (a smaller window, a calmer scene). */
    if (!i_period || !i_top || i_cost * 1.25 > 1000.0 / 60 * 0.6)
        return 0;
    if (dual())
        return fr(0)->time - fr(2)->time <= 100.0 && fr(1)->time - fr(3)->time <= 100.0;
    return steady() && fr(0)->time - fr(1)->time > 0 && fr(0)->time - fr(1)->time <= 50.0;
}

/* hw_halt: when the next frame is due; 0 if none, or if it would start after <deadline> (the next VBlank: one frame
 * may overrun it, the game's schedule catches up) */
double video_next_present(double deadline)
{
    double p = i_period, c = i_cost * 1.25, step = c > p ? c : p, now;
    if (!interp_active())
        return 0;
    now = now_ms();
    if (i_last < now - 2 * step)  /* fell well behind (the game was busy): start the grid again from now (a slot
                                   * a little late is just shown late, keeping the grid's rate) */
        i_last = now - step;
    return i_last + step < deadline ? i_last + step : 0;
}

void video_present_interp(void)
{
    double p = i_period, c = i_cost * 1.25;
    i_last += c > p ? c : p;
    if (!present(i_top, i_bottom, i_S))
        exit(0);
}

/* VBlank: the 3D engine swaps its lists here (geometry stalls after SWAP_BUFFERS until VBlank), so the 3D image
 * shown in the coming frame is rasterised now, before the game can submit and swap another list. */
static int swapped;
static double t3d;
/* The internal resolution changes (auto resolution, a resize): the kept high resolution captures go to the new
 * scale, nearest pixel, instead of being dropped. A screen can show the last capture for a long time (after the
 * intro the top screen is a still of its last frame): without them it fell back to native pixels, not widened. */
static void img_rescale(u32 *img, int w, int h, int from, int to)  /* w x h DS pixels */
{
    static u32 tmp[DS_W * DS_H * WMAX * MAXS * MAXS];
    int x, y, W = w * to, H = h * to, sw = w * from;
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++)
            tmp[y * W + x] = img[(y * from / to) * sw + x * from / to];
    memcpy(img, tmp, (size_t)W * H * 4);
}

static void caps_rescale(int from, int to)
{
    int b;
    for (b = 0; b < 4; b++) {
        HiCapture *k = &hicap[b];
        if (k->valid && k->s == from && to > 1) {
            img_rescale(k->img, k->w, k->h, from, to);
            k->s = to;
        } else
            k->valid = 0;
        if (wcap[b] && wcap[b]->valid && wcap[b]->S == from) {
            WideCap *c = wcap[b];
            img_rescale(c->img, c->W / from, c->H / from, from, to);
            c->S = to, c->W = c->W / from * to, c->H = c->H / from * to;
        }
    }
}

void video_vblank(void)
{
    if (gx_swap_pending()) {
        double t0 = now_ms();
        int want = window_scale();
        if (want != gx_scale()) {
            caps_rescale(gx_scale(), want);
            gx_set_scale(want);
        }
        hw_io_snapshot(io);
        memcpy(io3d, io, sizeof io3d);
        /* widescreen for the coming frame: both screens if the game is drawing 3D on both, else the one with it */
        {
            float wf = 1.0f, hf = 1.0f;
            if (fh_count && (dual() || has3d(0)))
                window_view(dual(), &wf, &hf);
            gx_set_view(wf, fh_count && dual() ? 1.0f : hf);  /* both screens: only wider (see window_view) */
        }
        /* skip the raster when every frame shown is drawn from the recorded lists anyway: interpolating one screen,
         * unless a display capture is on (what it captures may be shown for a long time after: at the end of the
         * intro the top screen keeps its last capture) */
        if (interp_active() && steady() && !dual() && !(*(u32 *)(io + 0x64) & 0x80000000u))
            raster_skipped = gx_take_swapped(io, frame3d);
        else {
            gx_render_swapped(io, frame3d);
            raster_skipped = 0;
        }
        t3d = now_ms() - t0;
        swapped = 1;
    }
}

/* Scripted input for test runs (frame numbers as in NDOGS_SHOT):
 * NDOGS_TOUCH="<frame>,<x>,<y>,<frames>;..." holds the stylus at (x, y) on the bottom screen,
 * NDOGS_KEYS="<frame>,<mask>,<frames>;..." holds DS buttons (KEYINPUT bits: 1 A, 2 B, 4 Select, 8 Start, 0x10 Right,
 * 0x20 Left, 0x40 Up, 0x80 Down, 0x100 R, 0x200 L, 0x400 X, 0x800 Y). */
static int frame;

static int script_find(const char *env, int nvals, int *vals)
{
    const char *p = getenv(env);
    while (p && *p) {
        int v[4] = { 0, 0, 0, 0 }, i;
        for (i = 0; i < nvals + 2 && *p && *p != ';'; i++) {
            v[i] = (int)strtol(p, (char **)&p, 0);
            if (*p == ',')
                p++;
        }
        if (frame >= v[0] && frame < v[0] + v[nvals + 1]) {
            for (i = 0; i < nvals; i++)
                vals[i] = v[i + 1];
            return 1;
        }
        while (*p && *p != ';')
            p++;
        if (*p == ';')
            p++;
    }
    return 0;
}

/* buttons: window keyboard plus NDOGS_KEYS (0 = pressed) */
u32 input_keys(void)
{
    int m;
    u32 k = window_keys();
    if (script_find("NDOGS_KEYS", 1, &m))
        k &= ~(u32)m;
    return k;
}

void video_frame(void)
{
    u32 powcnt;
    int x, y, xy[2], scripted = 0, S;
    const u32 *top, *bottom;
    if (!opened) {
        opened = 1;
        if (!getenv("NDOGS_HEADLESS"))
            window_open(2);
    }
    double t0 = now_ms();
    hw_io_snapshot(io);
    interp_keep();
    S = gx_scale();
    /* engine B first: it may be showing the previous frame's capture, which this frame's capture overwrites */
    gpu2d_render(1, io, 0, screen_b);
    if (S > 1)
        compose(1, screen_b, hi_b, S, 1);
    cap_bank = -1;
    render_a(frame3d, screen_a, hi_a, S, 1);
    if (cap_bank >= 0) {  /* a whole screen captured: keep it widened, if the view is (see wcap) */
        if ((gx_wide_px() > DS_W || gx_tall_px() > DS_H) && gx_scale() == S && !raster_skipped) {
            int W, H;
            const u32 *wd = widen(S > 1 ? hi_a : screen_a, S, wide_buf[1], &W, &H, fr(0));
            if (wd != (S > 1 ? hi_a : screen_a))
                wcap_keep(cap_bank, wd, S, W, H);
        } else if (wcap[cap_bank])
            wcap[cap_bank]->valid = 0;
    }
    powcnt = *(u32 *)(io + 0x304);
    frame++;
    if (getenv("NDOGS_VLOG"))
        fprintf(stderr, "vid %d: 3d %.1fms 2d %.1fms swap %d POWCNT1 %04x DISPCAPCNT %08x VRAMCNT %08x\n", frame, t3d, now_ms() - t0, swapped, powcnt & 0xffff,
                *(u32 *)(io + 0x64), *(u32 *)(io + 0x240));
    {
        /* NDOGS_JANK=1: per presented frame, the time since the previous one, whether a new 3D frame and a
         * screen swap came with it, and how long the frame took (debugging uneven animation) */
        static int jank = -1;
        static double last;
        static u32 last_pow;
        double now = now_ms();
        if (jank < 0)
            jank = getenv("NDOGS_JANK") != 0;
        if (jank)
            fprintf(stderr, "jank %d: dt %.1f 3d %d swapscreens %d render %.1f interp %.1f presents %u\n", frame,
                    now - last, swapped, ((powcnt ^ last_pow) >> 15) & 1, now - t0, i_cost, i_presents);
        last = now;
        last_pow = powcnt;
    }
    swapped = 0;
    if (getenv("NDOGS_SHOT")) {
        const char *p = getenv("NDOGS_SHOT");
        while (*p) {
            if (atoi(p) == frame) {
                if (S > 1)
                    save_bmp(frame, powcnt & 0x8000 ? hi_a : hi_b, DS_W * S, DS_H * S, powcnt & 0x8000 ? hi_b : hi_a,
                             DS_W * S, DS_H * S);
                else
                    save_bmp(frame, powcnt & 0x8000 ? screen_a : screen_b, DS_W, DS_H,
                             powcnt & 0x8000 ? screen_b : screen_a, DS_W, DS_H);
                fprintf(stderr, "shot %d: POWCNT1=%08x DISPCNT A=%08x B=%08x BRIGHT A=%04x B=%04x VRAMCNT=%08x %08x %04x\n",
                        frame, powcnt, *(u32 *)(io + 0), *(u32 *)(io + 0x1000), *(u16 *)(io + 0x6c),
                        *(u16 *)(io + 0x106c), *(u32 *)(io + 0x240), *(u32 *)(io + 0x244), *(u16 *)(io + 0x248));
            }
            while (*p && *p != ',')
                p++;
            if (*p == ',')
                p++;
        }
    }
    /* NDOGS_PTRWATCH=<addr>: log each change of the u16 that the game pointer at <addr> points to (e.g. 0215942c,
     * the game mode) */
    if (getenv("NDOGS_PTRWATCH")) {
        static u32 last_v = 0xffffffffu;
        u32 p = *(u32 *)strtoul(getenv("NDOGS_PTRWATCH"), 0, 16), v = p ? *(u16 *)p : 0xffff0000u;
        if (v != last_v)
            fprintf(stderr, "ptrwatch %d: %08x -> %x\n", frame, p, v);
        last_v = v;
    }
    if (S > 1) {
        top = powcnt & 0x8000 ? hi_a : hi_b;
        bottom = powcnt & 0x8000 ? hi_b : hi_a;
    } else {
        top = powcnt & 0x8000 ? screen_a : screen_b;
        bottom = powcnt & 0x8000 ? screen_b : screen_a;
    }
    interp_init();
    i_top = top;
    i_bottom = bottom;
    i_S = S;
    if (getenv("NDOGS_SHOT")) {  /* also what is shown (shot_2<frame>: widened) and, interpolating, halfway between
                                  * the frames (shot_1<frame>) */
        const char *p = getenv("NDOGS_SHOT");
        for (; *p; p++)
            if (atoi(p) == frame && (p == getenv("NDOGS_SHOT") || p[-1] == ',')) {
                const u32 *it = top, *ib = bottom;
                int tw, th, bw, bh;
                if (compose_now(now_ms(), 1, &it, &tw, &th, &ib, &bw, &bh, S))
                    save_bmp(200000 + frame, it, tw, th, ib, bw, bh);
                it = top, ib = bottom;
                if (i_period && compose_now(fr(0)->time + (fr(0)->time - fr(dual() ? 2 : 1)->time) * 0.5, 0, &it, &tw,
                                            &th, &ib, &bw, &bh, S))
                    save_bmp(100000 + frame, it, tw, th, ib, bw, bh);
            }
    }
    {   /* interpolating, the frames on the grid show it (video_present_interp); unless none went out for a
         * period and a half (a slow machine): then this one, so something is always shown and the window kept
         * alive. (Presenting every frame here would pin the grid to the game's frames: at 72 Hz no slot would
         * ever fit before the next VBlank.) */
        double step = i_cost * 1.25 > i_period ? i_cost * 1.25 : i_period;
        if (i_period && i_cost * 1.25 > 1000.0 / 60 * 0.6)
            i_cost *= 0.998;  /* interpolation off for its cost: try again in ~13 s */
        if (!interp_active() || now_ms() - i_last > step * 1.5) {
            if (!present(top, bottom, S))
                exit(0);
            i_last = now_ms();
        }
    }

    /* stylus: the ARM7 leaves the latest touch sample at 0x027fffaa (x, y as 12-bit ADC values, touch bit 24,
     * validity bits 25-26 = 0). The ADC values are screen coordinates scaled by 16; the calibration written at
     * startup (see video_init) maps them back. */
    if (!window_touch(&x, &y) && script_find("NDOGS_TOUCH", 2, xy))
        x = xy[0], y = xy[1], scripted = 1;
    if (window_touch(&x, &y) || scripted) {
        u32 v = (u32)(x * 16 + 8) | (u32)(y * 16 + 8) << 12 | 1u << 24;
        *(volatile u16 *)0x027fffaau = (u16)v;
        *(volatile u16 *)0x027fffacu = (u16)(v >> 16);
        *(volatile u16 *)0x027fffa8u &= (u16)~0x40;  /* pen down (EXTKEYIN bit 6 clear) */
    } else {
        *(volatile u16 *)0x027fffacu = 0;
        *(volatile u16 *)0x027fffa8u |= 0x40;
    }
    {
        u32 k = input_keys();
        /* X and Y come from the ARM7's EXTKEYIN copy at 0x027fffa8 (bits 0-1) */
        *(volatile u16 *)0x027fffa8u = (u16)((*(volatile u16 *)0x027fffa8u & ~3u) | ((k >> 10) & 3));
    }
}

/* Firmware user settings the ARM7 copies to 0x027ffc80 at boot include the touch-screen calibration: two points
 * (ADC x/y, screen x/y) at +0x58. Matching our ADC = screen * 16 + 8 scale. */
void video_init(void)
{
    volatile u8 *c = (volatile u8 *)(0x027ffc80u + 0x58u);
    volatile u16 *w = (volatile u16 *)c;
    w[0] = 32 * 16 + 8;   /* adc x1 */
    w[1] = 32 * 16 + 8;   /* adc y1 */
    c[4] = 32;            /* screen x1 */
    c[5] = 32;            /* screen y1 */
    w[3] = 224 * 16 + 8;  /* adc x2 */
    w[4] = 160 * 16 + 8;  /* adc y2 */
    c[10] = 224;
    c[11] = 160;
    *(volatile u16 *)0x027fffa8u = 0x007f;  /* EXTKEYIN: nothing pressed, pen up */
    vram_init();
    gx_init();
}
