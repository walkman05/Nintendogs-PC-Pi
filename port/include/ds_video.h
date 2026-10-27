/* DS video: interfaces between the hardware layer, the 2D engines, the 3D engine and the window.
 * All functions run on the game thread at VBlank (or when a 3D command arrives). Pixels are 0x00BBGGRR-free
 * "RGB555 + flags" internally; finished screens are 32-bit 0x00RRGGBB, 256x192, row-major. */
#ifndef DS_VIDEO_H
#define DS_VIDEO_H
#include "ctx.h"

#define DS_W 256
#define DS_H 192

/* ---- VRAM (port/src/vram.c) ------------------------------------------------------------------------------------
 * VRAM banks A-I are separate memories mapped into roles by VRAMCNT (0x04000240-0x04000249). The CPU sees a bank
 * at its mapped address in the DS memory map; the engines read through these accessors, which follow the current
 * mapping. Reads of unmapped space return 0. Offsets are byte offsets within the role's address space. */
enum {
    VRAM_BG_A,       /* engine A backgrounds, CPU 0x06000000 (512 KiB space) */
    VRAM_OBJ_A,      /* engine A sprites, CPU 0x06400000 (256 KiB) */
    VRAM_BG_B,       /* engine B backgrounds, CPU 0x06200000 (128 KiB) */
    VRAM_OBJ_B,      /* engine B sprites, CPU 0x06600000 (128 KiB) */
    VRAM_TEX,        /* 3D texture image slots 0-3 (512 KiB) */
    VRAM_TEXPAL,     /* 3D texture palette slots (96 KiB) */
    VRAM_BGEXTPAL_A, /* engine A BG extended palettes, slots 0-3 (32 KiB) */
    VRAM_OBJEXTPAL_A,/* engine A OBJ extended palette (8 KiB) */
    VRAM_BGEXTPAL_B,
    VRAM_OBJEXTPAL_B,
    VRAM_LCDC,       /* plain bank view, CPU 0x06800000 (banks in order A..I) */
    VRAM_ROLE_COUNT
};
u8 vram_read8(int role, u32 offset);
u16 vram_read16(int role, u32 offset);
u32 vram_read32(int role, u32 offset);
void vram_cnt_written(int bank, u8 value);  /* VRAMCNT_A..I changed (bank 0..8) */
void vram_init(void);
u16 *vram_bank_ptr(int bank);
void vram_flatten(int role, u8 *dst, u32 size); /* copy of a hidden role (textures, texture palettes) */               /* bank A-D (0-3) storage, for display capture */

/* ---- I/O snapshot (port/src/hw.c) -------------------------------------------------------------------------------
 * Register values as the game last wrote them: io[0x0000..0x1fff] is 0x04000000..0x04001fff (engine A registers
 * at 0x000, engine B at 0x1000). Palettes (0x05000000, 2 KiB: A BG, A OBJ, B BG, B OBJ) and OAM (0x07000000,
 * 2 KiB: A then B) are plain memory in the DS map and can be read directly. */
void hw_io_snapshot(u8 *io /* 0x2000 bytes */);

/* ---- 2D engines (port/src/gpu2d.c) -------------------------------------------------------------------------------
 * Renders one engine's full 256x192 frame from its registers, VRAM, palette and OAM (engine 0 = A/main, 1 = B/sub).
 * For engine A, BG0 can be the 3D layer: pass the 3D frame (3d_color, ARGB-like: bit 31 set = opaque pixel,
 * low 24 bits 0xRRGGBB) or NULL. Applies windows, blending, mosaic where implemented, and master brightness.
 * Output: 0x00RRGGBB. */
void gpu2d_render(int engine, const u8 *io, const u32 *frame3d, u32 *out);
extern int gpu2d_no_brightness;             /* render without master brightness (capture source) */
void gpu2d_brightness(int engine, const u8 *io, u32 *out);
void hw_capture_done(void);                 /* clears DISPCAPCNT's enable bit once a capture has been taken */

/* ---- 3D engine (port/src/gpu3d.c) --------------------------------------------------------------------------------
 * gx_command receives every geometry command word, from the FIFO (port 0x04000400, packed command bytes followed
 * by their parameters) or a direct command port (0x04000440-0x040005ff: one parameter word per write). It keeps the
 * matrix stacks, lighting, polygon attributes and the vertex/polygon lists. SWAP_BUFFERS (0x50) marks the end of a
 * frame: gx_render_swapped() then rasterises the swapped lists into 256x192 (bit 31 = opaque, 0xRRGGBB) using
 * DISP3DCNT and the rear-plane/edge/fog/toon registers from io. gxstat() supplies GXSTAT bits the game polls
 * (matrix stack levels, busy = 0). */
void gx_command(u32 port, u32 value);
void gx_render_swapped(const u8 *io, u32 *out);
int gx_swap_pending(void);   /* nonzero after SWAP_BUFFERS until the next gx_render_swapped */
u32 gx_status(void);
void gx_init(void);

/* ---- window (port/src/window.c) ---------------------------------------------------------------------------------- */
int window_open(int scale);
/* top and bottom screen, 256x192 each, 0x00RRGGBB; returns 0 when the user closed the window */
int window_present(const u32 *top, const u32 *bottom);
/* mouse on the bottom screen: returns 1 while the left button is down inside it, with DS coordinates */
int window_touch(int *x, int *y);
u32 window_keys(void);  /* DS KEYINPUT bits (0 = pressed), plus X/Y in bits 10/11 */

#endif
