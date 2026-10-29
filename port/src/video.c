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
static void save_bmp(int frame, const u32 *top, const u32 *bottom, int w, int h)
{
    char name[64];
    FILE *f;
    u32 hdr[13];
    int y;
    snprintf(name, sizeof name, "shot_%d.bmp", frame);
    f = fopen(name, "wb");
    if (!f)
        return;
    memset(hdr, 0, sizeof hdr);
    fwrite("BM", 1, 2, f);
    hdr[0] = 54 + (u32)w * h * 2 * 4;
    hdr[2] = 54;
    hdr[3] = 40;
    hdr[4] = (u32)w;
    hdr[5] = (u32)h * 2;
    hdr[6] = 1 | (32 << 16);
    fwrite(hdr, 4, 13, f);
    for (y = h * 2 - 1; y >= 0; y--)
        fwrite(y < h ? top + y * w : bottom + (y - h) * w, 4, (size_t)w, f);
    fclose(f);
}

/* compose one engine's screen at scale S into out (see above); bright: apply the engine's master brightness to
 * the high resolution pixels (native pixels already have whatever the engine applied) */
static void compose(int engine, const u32 *native, u32 *out, int S, int bright)
{
    const u32 *src = gpu2d_src[engine], *h3d = gx_hires();
    int W = DS_W * S, x, y, sx, sy;
    int hofs = engine ? 0 : *(u16 *)(io + 0x10) & 0x1ff;
    for (y = 0; y < DS_H; y++)
        for (x = 0; x < DS_W; x++) {
            u32 sv = src[y * DS_W + x], c = native[y * DS_W + x] & 0xffffff;
            u32 *o = out + y * S * W + x * S;
            const u32 *hp = 0;
            int hw = 0;
            if (sv == GPU2D_SRC_3D) {
                int tx = (x + hofs) & 511;
                if (tx < DS_W) {
                    hp = h3d + y * S * W + tx * S;
                    hw = W;
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

static u16 rgb15(u32 c)
{
    return (u16)(((c >> 19) & 31) | ((c >> 11) & 31) << 5 | ((c >> 3) & 31) << 10);
}

/* Display capture (DISPCAPCNT 0x04000064): engine A's picture (before master brightness) or the 3D layer,
 * optionally blended with a VRAM bitmap, written into VRAM bank A-D as 15-bit pixels with bit 15 set. Games
 * with 3D on both screens swap the screens every frame and show the captured previous frame on the other one. */
static void display_capture(const u32 *graphics)
{
    u32 cap = *(u32 *)(io + 0x64), dc = *(u32 *)(io + 0);
    static const int widths[4] = { 128, 256, 256, 256 }, heights[4] = { 128, 64, 128, 192 };
    int w = widths[(cap >> 20) & 3], h = heights[(cap >> 20) & 3], src = (cap >> 29) & 3;
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
            for (y = 0; y < h * S; y++)
                memcpy(k->img + y * w * S, from + y * DS_W * S, (size_t)w * S * 4);
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

/* ---- high frame rate: interpolated frames ----------------------------------------------------------------------
 * The game runs at 60 frames a second (its logic counts frames). Above that, frames in between are presented while
 * the game waits for the next VBlank (hw_halt), interpolated between the game's last two frames: the 3D with its
 * geometry and skeletons interpolated (gpu3d.c); the 2D from the registers and OAM of both frames, with sprite
 * positions and rotation/scale, background scrolling and rotation, and fades interpolated (anything that changed
 * identity or jumped is taken from the newer frame). So everything is shown one game frame late, in step.
 * Rate: NDOGS_FPS, else 120 or the monitor's refresh rate if lower (Windows); off at 60 or below, while the game
 * swaps its screens every frame (3D on both screens: consecutive 3D frames belong to different screens), and with
 * NDOGS_FPS=0. Frames are presented on a steady grid; if the machine can't keep up, the rate drops to what it can
 * do. */
extern double now_ms(void);
static u8 io3d[0x2000];        /* registers of the 3D frame's VBlank (clear colour, fog, toon, edges) */
static u8 io_prev[0x2000], io_cur[0x2000], io_i[0x2000], io_save[0x2000];  /* the last two frames' registers */
static u16 oam_prev[0x400], oam_cur[0x400], oam_i[0x400];                  /* and OAM (both engines) */
static double f_prev, f_cur;   /* when they were drawn */
static u32 i3d[DS_W * DS_H], i_a[DS_W * DS_H], i_b[DS_W * DS_H];
static u32 hi_ia[DS_W * DS_H * MAXS * MAXS], hi_ib[DS_W * DS_H * MAXS * MAXS];
static double i_period, i_cost, i_last;  /* target ms per frame (0 off), measured cost, last present */
static int i_swaps;                      /* frames since the screens last swapped */
static unsigned i_presents;              /* frames presented (NDOGS_JANK) */
static u32 i_pow;
static const u32 *i_top, *i_bottom;      /* the last real frame's screens */
static int i_S;
extern int window_refresh_hz(void);

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

/* video_frame: keep this frame's registers and OAM as the newer of the pair */
static void interp_keep(void)
{
    memcpy(io_prev, io_cur, sizeof io_cur);
    memcpy(io_cur, io, sizeof io_cur);
    memcpy(oam_prev, oam_cur, sizeof oam_cur);
    memcpy(oam_cur, (const void *)0x07000000u, sizeof oam_cur);
    f_prev = f_cur;
    f_cur = now_ms();
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

/* registers and OAM at t between the last two frames (into io_i, oam_i) */
static void interp_2d(float t)
{
    int e, i, k;
    memcpy(io_i, io_cur, sizeof io_i);
    memcpy(oam_i, oam_cur, sizeof oam_i);
    for (e = 0; e < 2; e++) {
        const u8 *a = io_prev + e * 0x1000, *b = io_cur + e * 0x1000;
        u8 *o = io_i + e * 0x1000;
        u16 *oa = oam_i + e * 0x200;
        const u16 *pa = oam_prev + e * 0x200, *pb = oam_cur + e * 0x200;
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
         * what it is (tile, palette, shape, size, mode, rotation/flip settings), with the one nearest its new place
         * in the previous frame, in any slot; one with no match within the limit is shown where it is now. Each
         * previous sprite is used once. Affine matrices turn. */
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

/* Both screens interpolated to now (screens in *top, *bottom); 0 if there is no pair of frames to interpolate. */
static int interp_frame(double now, const u32 **top, const u32 **bottom)
{
    double span = f_cur - f_prev;
    float t, t3;
    const u32 *f3d = frame3d;
    int S = i_S;
    double ta, tb;
    if (!i_period || i_swaps < 2 || !i_top || span <= 0 || span > 50.0)
        return 0;
    t = (float)((now - f_cur) / span);
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    ta = now_ms();
    t3 = gx_interp_pos(now);
    if (t3 >= 0 && gx_render_interp(io3d, t, i3d))  /* the 3D at the 2D's position: in step */
        f3d = i3d;
    tb = now_ms();
    interp_2d(t);
    memcpy(io_save, io, sizeof io);
    memcpy(io, io_i, sizeof io);
    gpu2d_oam = oam_i;
    gpu2d_render(1, io, 0, i_b);
    if (S > 1)
        compose(1, i_b, hi_ib, S, 1);
    render_a(f3d, i_a, hi_ia, S, 0);
    gpu2d_oam = 0;
    memcpy(io, io_save, sizeof io);
    if (getenv("NDOGS_ILOG"))
        fprintf(stderr, "interp: t %.2f 3d %.2fms 2d %.2fms\n", t, tb - ta, now_ms() - tb);
    if (i_pow & 0x8000) {
        *top = S > 1 ? hi_ia : i_a;
        *bottom = S > 1 ? hi_ib : i_b;
    } else {
        *top = S > 1 ? hi_ib : i_b;
        *bottom = S > 1 ? hi_ia : i_a;
    }
    return 1;
}

/* present: interpolated to now if possible, else the last real frame */
static int present(const u32 *top, const u32 *bottom, int S)
{
    double t0 = now_ms();
    int r, in = interp_frame(t0, &top, &bottom);
    r = window_present(top, bottom, DS_W * S, DS_H * S);
    i_presents++;
    if (in) {  /* adapt the rate to the machine: an interpolated frame must leave time for the game */
        double c = now_ms() - t0;
        i_cost = i_cost ? i_cost * 0.9 + c * 0.1 : c;
    }
    return r;
}

/* While interpolating, frames are presented on a steady grid of the target period (or what the machine manages),
 * independent of the game's frames, which only bring new data; the game's own present is skipped (video_frame). */
static int interp_active(void)
{
    double span = f_cur - f_prev;
    return i_period && i_swaps >= 2 && i_top && span > 0 && span <= 50.0;
}

/* hw_halt: when the next frame is due; 0 if none, or if it wouldn't be done by <deadline> (a little after the next
 * VBlank: the game's time comes first) */
double video_next_present(double deadline)
{
    double p = i_period, c = i_cost * 1.25, step = c > p ? c : p, now;
    if (!interp_active())
        return 0;
    now = now_ms();
    if (i_last < now - step)  /* fell behind (the game was busy): start the grid again from now */
        i_last = now - step;
    return i_last + step + c < deadline ? i_last + step : 0;
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
void video_vblank(void)
{
    if (gx_swap_pending()) {
        double t0 = now_ms();
        int want = window_scale();
        if (want != gx_scale()) {
            gx_set_scale(want);
            memset(hicap, 0, sizeof hicap);
        }
        hw_io_snapshot(io);
        memcpy(io3d, io, sizeof io3d);
        if (i_period && i_swaps >= 2 && !(*(u32 *)(io + 0x64) & 0x80000000u))  /* no capture needs the frame */
            gx_take_swapped(io, frame3d);
        else
            gx_render_swapped(io, frame3d);
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
    render_a(frame3d, screen_a, hi_a, S, 1);
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
                    save_bmp(frame, powcnt & 0x8000 ? hi_a : hi_b, powcnt & 0x8000 ? hi_b : hi_a, DS_W * S, DS_H * S);
                else
                    save_bmp(frame, powcnt & 0x8000 ? screen_a : screen_b, powcnt & 0x8000 ? screen_b : screen_a,
                             DS_W, DS_H);
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
    i_swaps = (powcnt ^ i_pow) & 0x8000 ? 0 : i_swaps + 1;
    i_pow = powcnt;
    i_top = top;
    i_bottom = bottom;
    i_S = S;
    if (getenv("NDOGS_SHOT") && i_period) {  /* with NDOGS_FPS: also the frame halfway to the next (shot_1<frame>) */
        const char *p = getenv("NDOGS_SHOT");
        for (; *p; p++)
            if (atoi(p) == frame && (p == getenv("NDOGS_SHOT") || p[-1] == ',')) {
                const u32 *it, *ib;
                if (interp_frame(f_cur + (f_cur - f_prev) * 0.5, &it, &ib))  /* halfway from the previous frame */
                    save_bmp(100000 + frame, it, ib, DS_W * S, DS_H * S);
            }
    }
    {   /* interpolating, the frames on the grid show it (video_present_interp); unless none fitted in since the
         * last frame (a slow machine): then this one, so something is always shown and the window kept alive */
        static unsigned presents_seen;
        if (!interp_active() || i_presents == presents_seen) {
            if (!present(top, bottom, S))
                exit(0);
            i_last = now_ms();
        }
        presents_seen = i_presents;
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
