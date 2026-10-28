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

/* NDOGS_SHOT=<frame>[,<frame>...]: save both screens (top above bottom) to shot_<frame>.bmp in the current folder */
static void save_bmp(int frame, const u32 *top, const u32 *bottom)
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
    hdr[0] = 54 + DS_W * DS_H * 2 * 4;
    hdr[2] = 54;
    hdr[3] = 40;
    hdr[4] = DS_W;
    hdr[5] = DS_H * 2;
    hdr[6] = 1 | (32 << 16);
    fwrite(hdr, 4, 13, f);
    for (y = DS_H * 2 - 1; y >= 0; y--)
        fwrite(y < DS_H ? top + y * DS_W : bottom + (y - DS_H) * DS_W, 4, DS_W, f);
    fclose(f);
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
    hw_capture_done();
}

/* VBlank: the 3D engine swaps its lists here (geometry stalls after SWAP_BUFFERS until VBlank), so the 3D image
 * shown in the coming frame is rasterised now, before the game can submit and swap another list. */
static int swapped;
static double t3d;
extern double now_ms(void);
void video_vblank(void)
{
    if (gx_swap_pending()) {
        double t0 = now_ms();
        hw_io_snapshot(io);
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
    int x, y, xy[2], scripted = 0;
    if (!opened) {
        opened = 1;
        if (!getenv("NDOGS_HEADLESS"))
            window_open(2);
    }
    double t0 = now_ms();
    hw_io_snapshot(io);
    /* engine B first: it may be showing the previous frame's capture, which this frame's capture overwrites */
    gpu2d_render(1, io, 0, screen_b);
    if (*(u32 *)(io + 0x64) & 0x80000000u) {
        gpu2d_no_brightness = 1;
        gpu2d_render(0, io, frame3d, screen_a);
        gpu2d_no_brightness = 0;
        display_capture(screen_a);
        gpu2d_brightness(0, io, screen_a);
    } else
        gpu2d_render(0, io, frame3d, screen_a);
    powcnt = *(u32 *)(io + 0x304);
    frame++;
    if (getenv("NDOGS_VLOG"))
        fprintf(stderr, "vid %d: 3d %.1fms 2d %.1fms swap %d POWCNT1 %04x DISPCAPCNT %08x VRAMCNT %08x\n", frame, t3d, now_ms() - t0, swapped, powcnt & 0xffff,
                *(u32 *)(io + 0x64), *(u32 *)(io + 0x240));
    swapped = 0;
    if (getenv("NDOGS_SHOT")) {
        const char *p = getenv("NDOGS_SHOT");
        while (*p) {
            if (atoi(p) == frame) {
                save_bmp(frame, powcnt & 0x8000 ? screen_a : screen_b, powcnt & 0x8000 ? screen_b : screen_a);
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
    if (!window_present(powcnt & 0x8000 ? screen_a : screen_b, powcnt & 0x8000 ? screen_b : screen_a))
        exit(0);

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
