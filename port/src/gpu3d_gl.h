/* Between the 3D engine (gpu3d.c) and its OpenGL rendering engine (gpu3d_gl.c): the polygon list and one frame's
 * rendering job. */
#ifndef GPU3D_GL_H
#define GPU3D_GL_H
#include "ds_video.h"

#define MAX_POLY 2048
#define MAX_PV 10 /* a quad clipped against 6 planes */

enum { VX, VY, VZ, VW, VR, VG, VB, VS, VT, VNF };
typedef struct { float f[VNF]; } vtx_t;

typedef struct {
    int nv;
    u32 attr, tex, pal;
    u8 translucent;
    float ymin, ymax;
    vtx_t v[MAX_PV]; /* screen: x,y pixels; z = Z-buffer value (0..0xffffff); w = clip w (4096 = 1.0) */
} poly_t;

typedef struct {
    int npoly;
    u32 swapflags;
    poly_t poly[MAX_POLY];
} plist_t;

typedef struct {
    const plist_t *L;
    const int *order;          /* drawing order: n polygons, opaque first */
    int n;
    const u8 *io;              /* register snapshot */
    int scale, rw, rh;         /* internal resolution: rw x rh pixels, scale per DS pixel */
    int xoff, yoff;            /* the DS screen's 256x192 inside the view (DS pixels) */
    const u8 *tex_mem, *texpal_mem;
    unsigned texgen;           /* changes whenever tex_mem / texpal_mem are refreshed */
    u32 *hires;                /* out: rw x rh, 0x80RRGGBB */
    u32 *out;                  /* out: the native 256x192 frame */
} gl3d_job_t;

int gl3d_mode(void);   /* 0 CPU rasteriser, 1 OpenGL, 2 both, compared (NDOGS_GPU3D) */
int gl3d_render(const gl3d_job_t *j);                 /* 0: no OpenGL (use the CPU rasteriser) */
int gl3d_prefetch(const gl3d_job_t *j);  /* submit without waiting (hires/out unused); 0 if not possible */
int gl3d_collect(int rw, int rh, u32 *hires, u32 *out);  /* the frame submitted ahead, if it is that size; else 0 */
void gl3d_drop(void);                    /* forget the frame submitted ahead */
void gl3d_compare(const gl3d_job_t *j);  /* mode 2: j, then 0 once the CPU render has finished: compare the two */
void gx_tex_decode(u32 tex, u32 pal, int w, int h, u32 *dst);  /* texels as col | alpha << 16 (gpu3d.c) */

#endif
