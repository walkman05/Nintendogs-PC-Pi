/* DS 3D engine: geometry engine (command decoding, matrix stacks, lighting, texture coordinate generation,
 * primitive assembly, clipping, viewport transform) and rendering engine (scanline rasteriser with perspective
 * correct texturing, Gouraud shading, Z/W buffer, translucency, alpha test, toon/highlight, shadow stencil, fog
 * and edge marking). Written from the documented hardware behaviour (GBATEK); interface in ds_video.h.
 *
 * Geometry runs in the hardware's fixed point (20.12 matrices, 4.12 vertices, 12.4 texture coordinates); clipping
 * and rasterisation use float. Not modelled: anti-aliasing, the rear-plane bitmap, 1-dot polygon culling, line
 * (zero-area) polygons, exact hardware edge/interpolation precision, polygon/vertex RAM overflow beyond 2048
 * polygons. */
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "ds_video.h"
extern double now_ms(void);

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

static plist_t g_list[2];
static int g_cur;          /* list being filled */
static int g_swap_pending;
static u32 g_vtx_count;    /* approximate vertex count of the current list (RAM_COUNT) */

/* ---- geometry state ------------------------------------------------------------------------------------------ */
static s32 m_proj[16], m_pos[16], m_vec[16], m_tex[16], m_clip[16];
static s32 st_proj[16], st_tex[16], st_pos[32][16], st_vec[32][16];
static int sp_proj, sp_tex, sp_pos;
static int g_stack_err;
static int g_clip_dirty;
static int g_mtx_mode;

static s32 g_vx, g_vy, g_vz;        /* last vertex (4.12) */
static int g_cr, g_cg, g_cb;        /* current vertex colour, 6 bit */
static s32 g_s0, g_t0, g_s, g_t;    /* raw / transformed texture coordinates (12.4) */
static u32 g_attr_pending, g_attr;  /* POLYGON_ATTR written / latched at BEGIN_VTXS */
static u32 g_teximage, g_pltt;
static u32 g_dif, g_amb, g_spe, g_emi;
static int g_set_vtx_col, g_shine_en;
static u8 g_shine[128];
static s32 g_light[4][3];          /* transformed light vectors, .12 */
static int g_light_col[4][3];      /* 5 bit */
static int g_vp_x1, g_vp_y1, g_vp_w, g_vp_h;
static int g_prim, g_in_begin;
static int g_wpx = DS_W, g_xoff, g_hpx = DS_H, g_yoff;  /* a wider or taller view (see gx_set_view) */
static float g_wide = 1.0f, g_tall = 1.0f;
static vtx_t g_vbuf[4];
static int g_nv, g_strip_odd;

/* ---- frame interpolation: recording ------------------------------------------------------------------------------
 * For high frame rate output (see gx_render_interp) each list is also recorded before clipping: per vertex its
 * object-space position, the matrices it was transformed with (projection and position, i.e. the camera and the
 * model or skeleton bone), its lit colour and texture coordinates; per BEGIN_VTXS batch the polygon state. Two
 * consecutive lists are matched batch by batch and replayed with every matrix interpolated (rotation by quaternion,
 * scale and translation linearly), the way decomp PC ports interpolate their final matrices; the dogs are skinned
 * by restoring a bone matrix per vertex group, so this is skeletal interpolation. */
#define REC_V 16384
#define REC_B 4096
#define REC_M 4096
typedef struct { s32 x, y, z, s, t; u32 tex, pal; u16 m; u8 r, g, b; } rvtx_t;
typedef struct { u32 attr; int prim, first, n, vp[4]; } rbat_t;
typedef struct { float proj[16], pos[16]; } rmtx_t;
typedef struct {
    int nv, nb, nm, bad;
    u32 swapflags;
    double time;
    rvtx_t v[REC_V];
    rbat_t b[REC_B];
    rmtx_t m[REC_M];
} rec_t;
#define NREC 6
static rec_t g_rec[NREC];
/* being filled, swapped (not shown yet), and the last four shown, [0] newest (scenes with 3D on both screens
 * alternate: a screen's two lists are [0] and [2], or [1] and [3]) */
static int r_fill, r_swapped = -1, r_hist[4] = { -1, -1, -1, -1 };
static unsigned r_seq;  /* lists shown so far (gx_list_seq) */
#define r_cur r_hist[0]
#define r_prev r_hist[1]
typedef struct { int c, p; short m[REC_B]; } match_t;  /* a newer list's batches -> matching older ones, or -1 */
static match_t g_match[4];
static int g_match_next;
static int g_rec_mnew = 1;                                   /* the clip matrix changed since the last recorded one */
static plist_t *g_emit;                                      /* replay target (0: the list being filled) */

static int g_box_result;
static s32 g_pos_result[4];
static s16 g_vec_result[3];

/* command decoder */
static const u8 g_nparams[256] = {
    [0x10] = 1, [0x12] = 1, [0x13] = 1, [0x14] = 1, [0x16] = 16, [0x17] = 12, [0x18] = 16, [0x19] = 12,
    [0x1A] = 9, [0x1B] = 3, [0x1C] = 3, [0x20] = 1, [0x21] = 1, [0x22] = 1, [0x23] = 2, [0x24] = 1, [0x25] = 1,
    [0x26] = 1, [0x27] = 1, [0x28] = 1, [0x29] = 1, [0x2A] = 1, [0x2B] = 1, [0x30] = 1, [0x31] = 1, [0x32] = 1,
    [0x33] = 1, [0x34] = 32, [0x40] = 1, [0x50] = 1, [0x60] = 1, [0x70] = 3, [0x71] = 2, [0x72] = 1,
};
static u32 f_pack;        /* remaining packed command bytes */
static int f_active, f_cmd, f_need, f_have;
static u32 f_par[32];
static int d_cmd = -1, d_have;
static u32 d_par[32];

/* ---- fixed point helpers ------------------------------------------------------------------------------------- */
static inline s32 sext(u32 v, int bits) { return (s32)(v << (32 - bits)) >> (32 - bits); }
static inline int c5to6(int c) { return c ? c * 2 + 1 : 0; }

static const s32 k_ident[16] = { 4096, 0, 0, 0, 0, 4096, 0, 0, 0, 0, 4096, 0, 0, 0, 0, 4096 };

static void mtx_mul(s32 *m, const s32 *n) /* m = n * m */
{
    s32 r[16];
    int i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++) {
            s64 a = (s64)n[i * 4 + 0] * m[0 * 4 + j] + (s64)n[i * 4 + 1] * m[1 * 4 + j] +
                    (s64)n[i * 4 + 2] * m[2 * 4 + j] + (s64)n[i * 4 + 3] * m[3 * 4 + j];
            r[i * 4 + j] = (s32)(a >> 12);
        }
    memcpy(m, r, sizeof(r));
}

static void update_clip(void)
{
    if (!g_clip_dirty)
        return;
    memcpy(m_clip, m_proj, sizeof(m_clip));
    mtx_mul(m_clip, m_pos);
    g_clip_dirty = 0;
    g_rec_mnew = 1;
}

static void xform4(const s32 *m, s32 x, s32 y, s32 z, s32 *out)
{
    int j;
    for (j = 0; j < 4; j++)
        out[j] = (s32)(((s64)x * m[j] + (s64)y * m[4 + j] + (s64)z * m[8 + j] + (s64)4096 * m[12 + j]) >> 12);
}

static void xform3(const s32 *m, s32 x, s32 y, s32 z, s32 *out)
{
    int j;
    for (j = 0; j < 3; j++)
        out[j] = (s32)(((s64)x * m[j] + (s64)y * m[4 + j] + (s64)z * m[8 + j]) >> 12);
}

/* ---- matrix commands ----------------------------------------------------------------------------------------- */
/* apply an operation "cur = n * cur" to the matrices selected by the mode; scale skips the vector matrix */
static void mtx_apply(const s32 *n, int is_scale)
{
    switch (g_mtx_mode) {
    case 0: mtx_mul(m_proj, n); g_clip_dirty = 1; break;
    case 1: mtx_mul(m_pos, n); g_clip_dirty = 1; break;
    case 2:
        mtx_mul(m_pos, n);
        if (!is_scale)
            mtx_mul(m_vec, n);
        g_clip_dirty = 1;
        break;
    default: mtx_mul(m_tex, n); break;
    }
}

static void mtx_load(const s32 *n)
{
    switch (g_mtx_mode) {
    case 0: memcpy(m_proj, n, 64); g_clip_dirty = 1; break;
    case 1: memcpy(m_pos, n, 64); g_clip_dirty = 1; break;
    case 2: memcpy(m_pos, n, 64); memcpy(m_vec, n, 64); g_clip_dirty = 1; break;
    default: memcpy(m_tex, n, 64); break;
    }
}

static void mtx_push(void)
{
    switch (g_mtx_mode) {
    case 0:
        if (sp_proj > 0) g_stack_err = 1;
        memcpy(st_proj, m_proj, 64);
        sp_proj = 1;
        break;
    case 3:
        if (sp_tex > 0) g_stack_err = 1;
        memcpy(st_tex, m_tex, 64);
        sp_tex = 1;
        break;
    default:
        if (sp_pos >= 31) g_stack_err = 1;
        memcpy(st_pos[sp_pos & 31], m_pos, 64);
        memcpy(st_vec[sp_pos & 31], m_vec, 64);
        sp_pos = (sp_pos + 1) & 63;
        break;
    }
}

static void mtx_pop(u32 p)
{
    switch (g_mtx_mode) {
    case 0:
        if (sp_proj == 0) g_stack_err = 1;
        sp_proj = 0;
        memcpy(m_proj, st_proj, 64);
        g_clip_dirty = 1;
        break;
    case 3:
        if (sp_tex == 0) g_stack_err = 1;
        sp_tex = 0;
        memcpy(m_tex, st_tex, 64);
        break;
    default:
        sp_pos = (sp_pos - sext(p, 6)) & 63;
        if (sp_pos >= 31) g_stack_err = 1;
        memcpy(m_pos, st_pos[sp_pos & 31], 64);
        memcpy(m_vec, st_vec[sp_pos & 31], 64);
        g_clip_dirty = 1;
        break;
    }
}

static void mtx_store(u32 p)
{
    int i = p & 31;
    switch (g_mtx_mode) {
    case 0: memcpy(st_proj, m_proj, 64); break;
    case 3: memcpy(st_tex, m_tex, 64); break;
    default:
        if (i == 31) g_stack_err = 1;
        memcpy(st_pos[i], m_pos, 64);
        memcpy(st_vec[i], m_vec, 64);
        break;
    }
}

static void mtx_restore(u32 p)
{
    int i = p & 31;
    switch (g_mtx_mode) {
    case 0: memcpy(m_proj, st_proj, 64); g_clip_dirty = 1; break;
    case 3: memcpy(m_tex, st_tex, 64); break;
    default:
        if (i == 31) g_stack_err = 1;
        memcpy(m_pos, st_pos[i], 64);
        memcpy(m_vec, st_vec[i], 64);
        g_clip_dirty = 1;
        break;
    }
}

/* ---- lighting ------------------------------------------------------------------------------------------------ */
static void do_normal(u32 p)
{
    s32 n[3];
    s32 nx = sext(p, 10) << 3, ny = sext(p >> 10, 10) << 3, nz = sext(p >> 20, 10) << 3; /* .12 */
    int tmode = g_teximage >> 30, i, c;
    int col[3];

    if (tmode == 2) {
        g_s = g_s0 + (s32)(((s64)nx * m_tex[0] + (s64)ny * m_tex[4] + (s64)nz * m_tex[8]) >> 21);
        g_t = g_t0 + (s32)(((s64)nx * m_tex[1] + (s64)ny * m_tex[5] + (s64)nz * m_tex[9]) >> 21);
    }
    xform3(m_vec, nx, ny, nz, n);

    for (c = 0; c < 3; c++)
        col[c] = ((g_emi >> (c * 5)) & 31) << 12;
    for (i = 0; i < 4; i++) {
        s32 dl, sl;
        s64 d;
        if (!(g_attr & (1u << i)))
            continue;
        d = -((s64)g_light[i][0] * n[0] + (s64)g_light[i][1] * n[1] + (s64)g_light[i][2] * n[2]);
        dl = (s32)(d >> 12);
        if (dl < 0) dl = 0;
        if (dl > 4096) dl = 4096;
        d = -(((s64)(g_light[i][0] >> 1)) * n[0] + ((s64)(g_light[i][1] >> 1)) * n[1] +
              ((s64)((g_light[i][2] - 4096) >> 1)) * n[2]);
        sl = (s32)(d >> 12);
        if (sl < 0) sl = 0;
        if (sl > 4096) sl = 4096;
        sl = (s32)(((s64)sl * sl) >> 12);
        if (g_shine_en) {
            int idx = sl >> 5;
            if (idx > 127) idx = 127;
            sl = g_shine[idx] << 4;
        }
        for (c = 0; c < 3; c++) {
            int lc = g_light_col[i][c];
            int spe = (g_spe >> (c * 5)) & 31, dif = (g_dif >> (c * 5)) & 31, amb = (g_amb >> (c * 5)) & 31;
            col[c] += (spe * lc * sl) >> 5;
            col[c] += (dif * lc * dl) >> 5;
            col[c] += (amb * lc) << 7; /* (amb*lc/32) .12 */
        }
    }
    for (c = 0; c < 3; c++) {
        int v = col[c] >> 12;
        if (v > 31) v = 31;
        if (v < 0) v = 0;
        col[c] = c5to6(v);
    }
    g_cr = col[0];
    g_cg = col[1];
    g_cb = col[2];
}

/* ---- primitive assembly, clipping, projection ---------------------------------------------------------------- */
static int clip_against(const vtx_t *in, int n, vtx_t *out, int axis, float sign)
{
    int i, k, m = 0;
    for (i = 0; i < n; i++) {
        const vtx_t *a = &in[i], *b = &in[(i + 1) % n];
        float da = a->f[VW] - sign * a->f[axis];
        float db = b->f[VW] - sign * b->f[axis];
        if (da >= 0)
            out[m++] = *a;
        if ((da >= 0) != (db >= 0)) {
            float t = da / (da - db);
            for (k = 0; k < VNF; k++)
                out[m].f[k] = a->f[k] + (b->f[k] - a->f[k]) * t;
            m++;
        }
        if (m >= MAX_PV + 2)
            break;
    }
    return m;
}

/* NDOGS_GXSTAT=1 debugging: what happened to each polygon this frame */
static int g_dump;
int ndogs_dbg;  /* NDOGS_GXDUMP=<frame>: trace that frame's matrix commands and clipped vertices */
static u32 st_far, st_clip, st_w, st_back, st_full, st_ok, st_vtx, st_cmd[256];

static void emit_poly(const vtx_t *src, int n)
{
    vtx_t b0[MAX_PV + 4], b1[MAX_PV + 4];
    vtx_t *in = b0, *out = b1, *t;
    plist_t *L = g_emit ? g_emit : &g_list[g_cur];
    poly_t *p;
    int i, need_clip = 0, far = 0, fmt;
    float area = 0, ymin = 1e9f, ymax = -1e9f;
    u32 attr = g_attr;
    /* a wider (taller) view: x / g_wide (y / g_tall) is clipped against the normal planes, i.e. x against
     * +-g_wide*w; full-screen viewports only */
    int full = g_vp_x1 == 0 && g_vp_y1 == 0 && g_vp_w == 256 && g_vp_h == 192;
    float kx = full ? 1.0f / g_wide : 1.0f, ky = full ? 1.0f / g_tall : 1.0f;

    for (i = 0; i < n; i++) {
        float x = src[i].f[VX] * kx, y = src[i].f[VY] * ky, z = src[i].f[VZ], w = src[i].f[VW];
        if (z > w) far = 1;
        if (x > w || x < -w || y > w || y < -w || z > w || z < -w)
            need_clip = 1;
    }
    if (far && !(attr & (1u << 12))) {
        st_far++;
        return;
    }
    memcpy(in, src, sizeof(vtx_t) * n);
    if (kx != 1.0f || ky != 1.0f)
        for (i = 0; i < n; i++) {
            in[i].f[VX] *= kx;
            in[i].f[VY] *= ky;
        }
    if (need_clip) {
        static const int axes[3] = { VX, VY, VZ };
        int a;
        for (a = 0; a < 3 && n > 0; a++) {
            n = clip_against(in, n, out, axes[a], 1.0f);
            t = in; in = out; out = t;
            if (n > 0) {
                n = clip_against(in, n, out, axes[a], -1.0f);
                t = in; in = out; out = t;
            }
        }
        if (n < 3) {
            if (g_dump && st_clip < 0)
                fprintf(stderr, "  clipped: v0 %.0f %.0f %.0f %.0f  v1 %.0f %.0f %.0f %.0f\n", src[0].f[VX], src[0].f[VY],
                        src[0].f[VZ], src[0].f[VW], src[1].f[VX], src[1].f[VY], src[1].f[VZ], src[1].f[VW]);
            st_clip++;
            return;
        }
        if (n > MAX_PV)
            n = MAX_PV;
    }
    if (L->npoly >= MAX_POLY) {
        st_full++;
        return;
    }
    p = &L->poly[L->npoly];
    for (i = 0; i < n; i++) {
        vtx_t *v = &p->v[i];
        float w = in[i].f[VW], iw, z;
        if (w <= 0.0f) {
            st_w++;
            return;
        }
        iw = 1.0f / w;
        *v = in[i];
        v->f[VX] = (float)(g_xoff + g_vp_x1) + (in[i].f[VX] * iw / kx + 1.0f) * 0.5f * (float)g_vp_w;
        v->f[VY] = (float)(g_yoff + 192) - ((float)g_vp_y1 + (in[i].f[VY] * iw / ky + 1.0f) * 0.5f * (float)g_vp_h);
        z = (in[i].f[VZ] * iw * 16384.0f + 16383.0f) * 512.0f;
        if (z < 0) z = 0;
        if (z > 16777215.0f) z = 16777215.0f;
        v->f[VZ] = z;
        if (v->f[VY] < ymin) ymin = v->f[VY];
        if (v->f[VY] > ymax) ymax = v->f[VY];
    }
    for (i = 0; i < n; i++) {
        const vtx_t *a = &p->v[i], *b = &p->v[(i + 1) % n];
        area += a->f[VX] * b->f[VY] - b->f[VX] * a->f[VY];
    }
    /* front face = counter-clockwise in view space (y up) = negative shoelace in y-down screen space */
    if (area < 0) {
        if (!(attr & 0x80)) { st_back++; return; }
    } else if (area > 0) {
        if (!(attr & 0x40)) { st_back++; return; }
    } else {
        st_back++;
        return;
    }
    st_ok++;
    p->nv = n;
    p->attr = attr;
    p->tex = g_teximage;
    p->pal = g_pltt;
    p->ymin = ymin;
    p->ymax = ymax;
    fmt = (g_teximage >> 26) & 7;
    {
        int alpha = (attr >> 16) & 31;
        p->translucent = (alpha > 0 && alpha < 31) || fmt == 1 || fmt == 6 || ((attr >> 4) & 3) == 3;
    }
    L->npoly++;
    if (!g_emit)
        g_vtx_count += n;  /* RAM_COUNT: not for replayed lists */
}

/* primitive assembly: one vertex into the current BEGIN_VTXS primitive */
static void assemble(const vtx_t *nv)
{
    g_vbuf[g_nv++] = *nv;
    switch (g_prim) {
    case 0: /* triangles */
        if (g_nv == 3) { emit_poly(g_vbuf, 3); g_nv = 0; }
        break;
    case 1: /* quads */
        if (g_nv == 4) { emit_poly(g_vbuf, 4); g_nv = 0; }
        break;
    case 2: /* triangle strip */
        if (g_nv == 3) {
            if (g_strip_odd) {
                vtx_t t[3];
                t[0] = g_vbuf[1]; t[1] = g_vbuf[0]; t[2] = g_vbuf[2];
                emit_poly(t, 3);
            } else
                emit_poly(g_vbuf, 3);
            g_strip_odd ^= 1;
            g_vbuf[0] = g_vbuf[1];
            g_vbuf[1] = g_vbuf[2];
            g_nv = 2;
        }
        break;
    default: /* quad strip: v0 v1 v3 v2 */
        if (g_nv == 4) {
            vtx_t t[4];
            t[0] = g_vbuf[0]; t[1] = g_vbuf[1]; t[2] = g_vbuf[3]; t[3] = g_vbuf[2];
            emit_poly(t, 4);
            g_vbuf[0] = g_vbuf[2];
            g_vbuf[1] = g_vbuf[3];
            g_nv = 2;
        }
        break;
    }
}

/* record a vertex of the list being filled (see g_rec) */
static void rec_vertex(s32 x, s32 y, s32 z)
{
    rec_t *R = &g_rec[r_fill];
    rvtx_t *v;
    int i;
    if (R->bad)
        return;
    if (g_rec_mnew) {
        rmtx_t *m;
        if (R->nm >= REC_M) { R->bad = 1; return; }
        m = &R->m[R->nm++];
        for (i = 0; i < 16; i++) {
            m->proj[i] = (float)m_proj[i];
            m->pos[i] = (float)m_pos[i];
        }
        g_rec_mnew = 0;
    }
    if (R->nv >= REC_V || R->nb == 0) { R->bad = 1; return; }
    v = &R->v[R->nv++];
    v->x = x; v->y = y; v->z = z;
    v->s = g_s; v->t = g_t;
    v->tex = g_teximage; v->pal = g_pltt;
    v->m = (u16)(R->nm - 1);
    v->r = (u8)g_cr; v->g = (u8)g_cg; v->b = (u8)g_cb;
    R->b[R->nb - 1].n++;
}

static void rec_begin(void)
{
    rec_t *R = &g_rec[r_fill];
    rbat_t *b;
    if (R->bad)
        return;
    if (R->nb >= REC_B) { R->bad = 1; return; }
    b = &R->b[R->nb++];
    b->attr = g_attr;
    b->prim = g_prim;
    b->first = R->nv;
    b->n = 0;
    b->vp[0] = g_vp_x1; b->vp[1] = g_vp_y1; b->vp[2] = g_vp_w; b->vp[3] = g_vp_h;
}

static void add_vertex(s32 x, s32 y, s32 z)
{
    s32 c[4];
    vtx_t *v, nv;
    g_vx = x;
    g_vy = y;
    g_vz = z;
    update_clip();
    xform4(m_clip, x, y, z, c);
    if ((g_teximage >> 30) == 3) {
        g_s = g_s0 + (s32)(((s64)x * m_tex[0] + (s64)y * m_tex[4] + (s64)z * m_tex[8]) >> 24);
        g_t = g_t0 + (s32)(((s64)x * m_tex[1] + (s64)y * m_tex[5] + (s64)z * m_tex[9]) >> 24);
    }
    if (!g_in_begin)
        return;
    st_vtx++;
    rec_vertex(x, y, z);
    v = &nv;
    v->f[VX] = (float)c[0];
    v->f[VY] = (float)c[1];
    v->f[VZ] = (float)c[2];
    v->f[VW] = (float)c[3];
    v->f[VR] = (float)g_cr;
    v->f[VG] = (float)g_cg;
    v->f[VB] = (float)g_cb;
    v->f[VS] = (float)g_s;
    v->f[VT] = (float)g_t;
    assemble(v);
}

static void tex_mode1(void)
{
    g_s = (s32)(((s64)g_s0 * m_tex[0] + (s64)g_t0 * m_tex[4] + m_tex[8] + m_tex[12]) >> 12);
    g_t = (s32)(((s64)g_s0 * m_tex[1] + (s64)g_t0 * m_tex[5] + m_tex[9] + m_tex[13]) >> 12);
}

static void box_test(const u32 *p)
{
    s32 x = sext(p[0], 16), y = sext(p[0] >> 16, 16), z = sext(p[1], 16);
    s32 w = sext(p[1] >> 16, 16), h = sext(p[2], 16), d = sext(p[2] >> 16, 16);
    int out_all[6] = { 1, 1, 1, 1, 1, 1 }, i, k;
    update_clip();
    for (i = 0; i < 8; i++) {
        s32 c[4];
        xform4(m_clip, x + ((i & 1) ? w : 0), y + ((i & 2) ? h : 0), z + ((i & 4) ? d : 0), c);
        if ((g_wide != 1.0f || g_tall != 1.0f) && g_vp_x1 == 0 && g_vp_y1 == 0 && g_vp_w == 256 && g_vp_h == 192) {
            c[0] = (s32)((float)c[0] / g_wide);  /* a wider or taller view */
            c[1] = (s32)((float)c[1] / g_tall);
        }
        for (k = 0; k < 3; k++) {
            if (c[k] <= c[3]) out_all[k * 2] = 0;
            if (c[k] >= -c[3]) out_all[k * 2 + 1] = 0;
        }
    }
    g_box_result = 1;
    for (k = 0; k < 6; k++)
        if (out_all[k])
            g_box_result = 0;
}

static void dump_polys(const plist_t *L)
{
    int i;
    for (i = 0; i < L->npoly; i++) {
        const poly_t *q = &L->poly[i];
        fprintf(stderr, "  poly %d: nv %d attr %08x tex %08x pal %04x tr %d y %.0f-%.0f x0 %.0f z0 %.0f w0 %.0f\n", i,
                q->nv, q->attr, q->tex, q->pal, q->translucent, q->ymin, q->ymax, q->v[0].f[VX], q->v[0].f[VZ],
                q->v[0].f[VW]);
    }
}

static void do_swap(u32 p)
{
    if (g_dump)
        dump_polys(&g_list[g_cur]);
    static int gxstat = -1, frames;
    if (gxstat < 0)
        gxstat = getenv("NDOGS_GXSTAT") != 0;
    ++frames;
    if (gxstat && frames % 60 == 0) {
        int c;
        fprintf(stderr, "gx: %u vtx, polys ok %u far %u clipped %u w<=0 %u culled %u full %u | cmds", st_vtx, st_ok,
                st_far, st_clip, st_w, st_back, st_full);
        for (c = 0; c < 256; c++)
            if (st_cmd[c])
                fprintf(stderr, " %02x:%u", c, st_cmd[c]);
        fprintf(stderr, "\n");
    }
    {
        static int want = -2;
        if (want == -2)
            want = getenv("NDOGS_GXDUMP") ? atoi(getenv("NDOGS_GXDUMP")) : -1;
        g_dump = frames == want;
        {
            extern int ndogs_dbg;
            ndogs_dbg = g_dump;
        }
        if (g_dump)
            fprintf(stderr, "gxdump frame %d:\n", frames);
    }
    st_far = st_clip = st_w = st_back = st_full = st_ok = st_vtx = 0;
    memset(st_cmd, 0, sizeof st_cmd);
    g_list[g_cur].swapflags = p & 3;
    g_cur ^= 1;
    g_list[g_cur].npoly = 0;
    g_vtx_count = 0;
    g_swap_pending = 1;
    /* the recording goes with the list: swapped now, shown at VBlank (gx_render_swapped); start a free one */
    g_rec[r_fill].swapflags = p & 3;
    r_swapped = r_fill;
    for (r_fill = 0; r_fill == r_swapped || r_fill == r_hist[0] || r_fill == r_hist[1] || r_fill == r_hist[2] ||
                     r_fill == r_hist[3]; r_fill++)
        ;
    g_rec[r_fill].nv = g_rec[r_fill].nb = g_rec[r_fill].nm = g_rec[r_fill].bad = 0;
    {   /* forget matches involving the reused recording */
        int k;
        for (k = 0; k < 4; k++)
            if (g_match[k].c == r_fill || g_match[k].p == r_fill)
                g_match[k].c = g_match[k].p = -1;
    }
    g_rec_mnew = 1;
}

static void exec_cmd(int cmd, const u32 *p)
{
    st_cmd[cmd & 255]++;
    if (g_dump && (cmd == 0x29 || cmd == 0x2a)) {
        u32 *fp = (u32 *)__builtin_frame_address(0);
        int k;
        fprintf(stderr, "  %s %08x <", cmd == 0x29 ? "ATTR" : "TEX", p[0]);
        for (k = 0; k < 14 && fp; k++, fp = (u32 *)fp[0])
            fprintf(stderr, " %08x", fp[1]);
        fputc(10, stderr);
    }
    if (g_dump && cmd >= 0x10 && cmd <= 0x1c) {
        int k, np = cmd == 0x16 || cmd == 0x18 ? 16 : cmd == 0x17 || cmd == 0x19 ? 12 : cmd == 0x1a ? 9 :
                    cmd >= 0x1b ? 3 : 1;
        fprintf(stderr, "  [ok %u clip %u back %u] cmd %02x mode %d:", st_ok, st_clip, st_back, cmd, g_mtx_mode);
        for (k = 0; k < np; k++)
            fprintf(stderr, " %d", (s32)p[k]);
        fprintf(stderr, "\n");
    }
    s32 m[16];
    int i;
    switch (cmd) {
    case 0x10: g_mtx_mode = p[0] & 3; break;
    case 0x11: mtx_push(); break;
    case 0x12: mtx_pop(p[0]); break;
    case 0x13: mtx_store(p[0]); break;
    case 0x14: mtx_restore(p[0]); break;
    case 0x15: mtx_load(k_ident); break;
    case 0x16:
        for (i = 0; i < 16; i++) m[i] = (s32)p[i];
        mtx_load(m);
        break;
    case 0x17:
    case 0x19:
        for (i = 0; i < 4; i++) {
            m[i * 4 + 0] = (s32)p[i * 3 + 0];
            m[i * 4 + 1] = (s32)p[i * 3 + 1];
            m[i * 4 + 2] = (s32)p[i * 3 + 2];
            m[i * 4 + 3] = i == 3 ? 4096 : 0;
        }
        if (cmd == 0x17) mtx_load(m); else mtx_apply(m, 0);
        break;
    case 0x18:
        for (i = 0; i < 16; i++) m[i] = (s32)p[i];
        mtx_apply(m, 0);
        break;
    case 0x1A:
        memcpy(m, k_ident, 64);
        for (i = 0; i < 3; i++) {
            m[i * 4 + 0] = (s32)p[i * 3 + 0];
            m[i * 4 + 1] = (s32)p[i * 3 + 1];
            m[i * 4 + 2] = (s32)p[i * 3 + 2];
        }
        mtx_apply(m, 0);
        break;
    case 0x1B:
        memcpy(m, k_ident, 64);
        m[0] = (s32)p[0]; m[5] = (s32)p[1]; m[10] = (s32)p[2];
        mtx_apply(m, 1);
        break;
    case 0x1C:
        memcpy(m, k_ident, 64);
        m[12] = (s32)p[0]; m[13] = (s32)p[1]; m[14] = (s32)p[2];
        mtx_apply(m, 0);
        break;
    case 0x20:
        g_cr = c5to6(p[0] & 31); g_cg = c5to6((p[0] >> 5) & 31); g_cb = c5to6((p[0] >> 10) & 31);
        break;
    case 0x21: do_normal(p[0]); break;
    case 0x22:
        g_s0 = g_s = sext(p[0], 16);
        g_t0 = g_t = sext(p[0] >> 16, 16);
        if ((g_teximage >> 30) == 1) tex_mode1();
        break;
    case 0x23: add_vertex(sext(p[0], 16), sext(p[0] >> 16, 16), sext(p[1], 16)); break;
    case 0x24: add_vertex(sext(p[0], 10) << 6, sext(p[0] >> 10, 10) << 6, sext(p[0] >> 20, 10) << 6); break;
    case 0x25: add_vertex(sext(p[0], 16), sext(p[0] >> 16, 16), g_vz); break;
    case 0x26: add_vertex(sext(p[0], 16), g_vy, sext(p[0] >> 16, 16)); break;
    case 0x27: add_vertex(g_vx, sext(p[0], 16), sext(p[0] >> 16, 16)); break;
    case 0x28:
        add_vertex(g_vx + sext(p[0], 10), g_vy + sext(p[0] >> 10, 10), g_vz + sext(p[0] >> 20, 10));
        break;
    case 0x29: g_attr_pending = p[0]; break;
    case 0x2A: g_teximage = p[0]; break;
    case 0x2B: g_pltt = p[0] & 0x1fff; break;
    case 0x30:
        g_dif = p[0] & 0x7fff;
        g_amb = (p[0] >> 16) & 0x7fff;
        g_set_vtx_col = (p[0] >> 15) & 1;
        if (g_set_vtx_col) {
            g_cr = c5to6(g_dif & 31); g_cg = c5to6((g_dif >> 5) & 31); g_cb = c5to6((g_dif >> 10) & 31);
        }
        break;
    case 0x31:
        g_spe = p[0] & 0x7fff;
        g_emi = (p[0] >> 16) & 0x7fff;
        g_shine_en = (p[0] >> 15) & 1;
        break;
    case 0x32: {
        int l = p[0] >> 30;
        xform3(m_vec, sext(p[0], 10) << 3, sext(p[0] >> 10, 10) << 3, sext(p[0] >> 20, 10) << 3, g_light[l]);
        break;
    }
    case 0x33: {
        int l = p[0] >> 30;
        g_light_col[l][0] = p[0] & 31;
        g_light_col[l][1] = (p[0] >> 5) & 31;
        g_light_col[l][2] = (p[0] >> 10) & 31;
        break;
    }
    case 0x34:
        for (i = 0; i < 32; i++) {
            g_shine[i * 4 + 0] = p[i] & 0xff;
            g_shine[i * 4 + 1] = (p[i] >> 8) & 0xff;
            g_shine[i * 4 + 2] = (p[i] >> 16) & 0xff;
            g_shine[i * 4 + 3] = (p[i] >> 24) & 0xff;
        }
        break;
    case 0x40:
        g_prim = p[0] & 3;
        g_attr = g_attr_pending;
        g_in_begin = 1;
        g_nv = 0;
        g_strip_odd = 0;
        rec_begin();
        break;
    case 0x41: break; /* END_VTXS: no effect on hardware */
    case 0x50: do_swap(p[0]); break;
    case 0x60:
        g_vp_x1 = p[0] & 0xff;
        g_vp_y1 = (p[0] >> 8) & 0xff;
        g_vp_w = (int)((p[0] >> 16) & 0xff) - g_vp_x1 + 1;
        g_vp_h = (int)((p[0] >> 24) & 0xff) - g_vp_y1 + 1;
        break;
    case 0x70: box_test(p); break;
    case 0x71:
        g_vx = sext(p[0], 16);
        g_vy = sext(p[0] >> 16, 16);
        g_vz = sext(p[1], 16);
        update_clip();
        xform4(m_clip, g_vx, g_vy, g_vz, g_pos_result);
        break;
    case 0x72: {
        s32 r[3];
        xform3(m_vec, sext(p[0], 10) << 3, sext(p[0] >> 10, 10) << 3, sext(p[0] >> 20, 10) << 3, r);
        for (i = 0; i < 3; i++)
            g_vec_result[i] = (s16)sext((u32)r[i], 13);
        break;
    }
    default: break;
    }
}

static void fifo_next(void)
{
    while ((f_pack & 0xff) != 0) {
        int cmd = f_pack & 0xff;
        f_pack >>= 8;
        if (g_nparams[cmd] == 0) {
            exec_cmd(cmd, f_par);
            continue;
        }
        f_cmd = cmd;
        f_need = g_nparams[cmd];
        f_have = 0;
        f_active = 1;
        return;
    }
    f_active = 0;
}

void gx_command(u32 port, u32 value)
{
    u32 off = port - 0x04000400u;
    if (off < 0x40) { /* GXFIFO */
        if (f_active) {
            f_par[f_have++] = value;
            if (f_have >= f_need) {
                f_active = 0;
                exec_cmd(f_cmd, f_par);
                fifo_next();
            }
            return;
        }
        if (value == 0)
            return; /* NOP */
        f_pack = value;
        fifo_next();
        return;
    }
    if (off < 0x200) { /* direct command ports */
        int cmd = off >> 2;
        int need = g_nparams[cmd];
        if (need == 0) {
            d_cmd = -1;
            exec_cmd(cmd, d_par);
            return;
        }
        if (cmd != d_cmd) {
            d_cmd = cmd;
            d_have = 0;
        }
        d_par[d_have++] = value;
        if (d_have >= need) {
            d_cmd = -1;
            d_have = 0;
            exec_cmd(cmd, d_par);
        }
    }
}

int gx_swap_pending(void) { return g_swap_pending; }

u32 gx_status(void)
{
    u32 s = 0;
    if (g_box_result) s |= 1u << 1;
    s |= (u32)(sp_pos & 31) << 8;
    s |= (u32)(sp_proj & 1) << 13;
    if (g_stack_err) s |= 1u << 15;
    s |= 3u << 25; /* FIFO less than half full, empty; never busy */
    return s;
}

/* Extra (not in ds_video.h): read the geometry engine's result registers 0x04000604 (RAM_COUNT),
 * 0x04000620-0x0400062f (POS_RESULT), 0x04000630-0x04000635 (VEC_RESULT), 0x04000640-0x0400067f (CLIPMTX_RESULT),
 * 0x04000680-0x040006a3 (VECMTX_RESULT). addr is the full address of the 32-bit word (16-bit aligned for VEC). */
u32 gx_read_result(u32 addr);
u32 gx_read_result(u32 addr)
{
    u32 o = addr - 0x04000600u;
    if (o == 0x04)
        return (u32)g_list[g_cur].npoly | ((g_vtx_count > 6144 ? 6144 : g_vtx_count) << 16);
    if (o >= 0x20 && o < 0x30)
        return (u32)g_pos_result[(o - 0x20) >> 2];
    if (o >= 0x30 && o < 0x36) {
        int i = (o - 0x30) >> 1;
        u32 lo = (u16)g_vec_result[i], hi = i < 2 ? (u16)g_vec_result[i + 1] : 0;
        return lo | (hi << 16);
    }
    if (o >= 0x40 && o < 0x80) {
        update_clip();
        return (u32)m_clip[(o - 0x40) >> 2];
    }
    if (o >= 0x80 && o < 0xa4) {
        int i = (o - 0x80) >> 2;
        return (u32)m_vec[(i / 3) * 4 + i % 3];
    }
    return 0;
}

void gx_init(void)
{
    int i;
    memcpy(m_proj, k_ident, 64);
    memcpy(m_pos, k_ident, 64);
    memcpy(m_vec, k_ident, 64);
    memcpy(m_tex, k_ident, 64);
    g_clip_dirty = 1;
    sp_proj = sp_tex = sp_pos = 0;
    g_stack_err = 0;
    g_mtx_mode = 0;
    g_vx = g_vy = g_vz = 0;
    g_cr = g_cg = g_cb = 63;
    g_s0 = g_t0 = g_s = g_t = 0;
    g_attr_pending = g_attr = 0x001f00c0; /* opaque, render front+back until set */
    g_teximage = g_pltt = 0;
    g_dif = g_amb = g_spe = g_emi = 0;
    g_set_vtx_col = g_shine_en = 0;
    memset(g_shine, 0, sizeof(g_shine));
    memset(g_light, 0, sizeof(g_light));
    memset(g_light_col, 0, sizeof(g_light_col));
    g_vp_x1 = 0; g_vp_y1 = 0; g_vp_w = 256; g_vp_h = 192;
    g_prim = 0; g_in_begin = 0; g_nv = 0; g_strip_odd = 0;
    g_box_result = 0;
    f_pack = 0; f_active = 0; d_cmd = -1; d_have = 0;
    g_list[0].npoly = g_list[1].npoly = 0;
    g_list[0].swapflags = g_list[1].swapflags = 0;
    g_cur = 0;
    g_swap_pending = 0;
    g_vtx_count = 0;
    r_fill = 0;
    r_swapped = r_hist[0] = r_hist[1] = r_hist[2] = r_hist[3] = -1;
    for (i = 0; i < 4; i++)
        g_match[i].c = g_match[i].p = -1;
    g_rec[0].nv = g_rec[0].nb = g_rec[0].nm = g_rec[0].bad = 0;
    g_rec_mnew = 1;
}

/* ---- rendering engine ---------------------------------------------------------------------------------------- */
/* Internal resolution: the scene is rasterised at g_scale times the DS's 256x192 (1-4, gx_set_scale); RW x RH pixels.
 * Rasterising is split into horizontal bands run in parallel (port/src/workers.c). */
#define MAXS 4
#define WMAX 2 /* widest widescreen view: twice the DS screen's width */
#define NPIX (DS_W * WMAX * DS_H * MAXS * MAXS)
static int g_scale = 1, RW = DS_W, RH = DS_H;
/* Widescreen (gx_set_wide): full-screen 3D views are rendered wider than the DS screen, g_wpx DS pixels with the
 * screen's 256 in the middle (g_xoff): the horizontal field of view grows (clipping and the box test use the wider
 * range), so the top screen fills a wider window instead of leaving bars. Smaller viewports (the game renders e.g.
 * portraits into part of the screen) are unchanged. The 2D engines and display capture see the middle 256. */

static u32 rb_color[NPIX];   /* r | g<<8 | b<<16 (6 bit each) | alpha(5 bit)<<24 */
static u32 rb_depth[NPIX];
static u8 rb_opid[NPIX];     /* opaque polygon ID */
static u8 rb_trid[NPIX];     /* translucent polygon ID last written, 0xff none */
static u8 rb_flags[NPIX];    /* bit0 fog, bit1 opaque polygon pixel (edge-markable), bit2 stencil */

typedef struct {
    u32 addr, palbase;
    int fmt, w, h, rs, rt, fs, ft, transp0;
} texinfo_t;

/* this frame's texture and texture-palette memory (vram_flatten) */
static u8 tex_mem[0x80000], texpal_mem[0x20000];
static inline u8 tex8(u32 off) { return tex_mem[off & 0x7ffff]; }
static inline u16 tex16(u32 off) { off &= 0x7fffe; return (u16)(tex_mem[off] | tex_mem[off + 1] << 8); }
static inline u32 tex32(u32 off) { off &= 0x7fffc; return *(const u32 *)(tex_mem + off); }
static inline u16 pal16(u32 off) { off &= 0x1fffe; return (u16)(texpal_mem[off] | texpal_mem[off + 1] << 8); }

static inline int wrapc(int c, int size, int rep, int flip)
{
    if (rep) {
        if (flip && (c & size))
            return size - 1 - (c & (size - 1));
        return c & (size - 1);
    }
    if (c < 0) return 0;
    if (c >= size) return size - 1;
    return c;
}

static inline u16 blend555(u16 a, u16 b, int wa, int wb, int sh)
{
    int r = (((a & 31) * wa + (b & 31) * wb) >> sh);
    int g = ((((a >> 5) & 31) * wa + ((b >> 5) & 31) * wb) >> sh);
    int bl = ((((a >> 10) & 31) * wa + ((b >> 10) & 31) * wb) >> sh);
    return (u16)(r | (g << 5) | (bl << 10));
}

/* returns texel alpha 0..31, colour as RGB555 in *col */
static int tex_sample(const texinfo_t *ti, int s, int t, u16 *col)
{
    int x = wrapc(s, ti->w, ti->rs, ti->fs);
    int y = wrapc(t, ti->h, ti->rt, ti->ft);
    u32 i = (u32)(y * ti->w + x);
    u32 a = ti->addr;
    int idx, v;
    switch (ti->fmt) {
    case 1:
        v = tex8(a + i);
        *col = pal16(ti->palbase + (v & 31) * 2);
        v >>= 5;
        return v * 4 + (v >> 1);
    case 2:
        v = tex8(a + (i >> 2));
        idx = (v >> ((i & 3) * 2)) & 3;
        if (idx == 0 && ti->transp0) return 0;
        *col = pal16(ti->palbase + idx * 2);
        return 31;
    case 3:
        v = tex8(a + (i >> 1));
        idx = (i & 1) ? v >> 4 : v & 15;
        if (idx == 0 && ti->transp0) return 0;
        *col = pal16(ti->palbase + idx * 2);
        return 31;
    case 4:
        idx = tex8(a + i);
        if (idx == 0 && ti->transp0) return 0;
        *col = pal16(ti->palbase + idx * 2);
        return 31;
    case 5: {
        u32 blk = (u32)((y >> 2) * (ti->w >> 2) + (x >> 2));
        u32 word = tex32(a + blk * 4);
        u32 pia = 0x20000 + ((a & 0x1ffff) >> 1) + (a >= 0x40000 ? 0x10000 : 0) + blk * 2;
        u16 pw = tex16(pia);
        u32 po = ti->palbase + (pw & 0x3fff) * 4;
        int mode = pw >> 14;
        idx = (word >> ((y & 3) * 8 + (x & 3) * 2)) & 3;
        switch (mode) {
        case 0:
            if (idx == 3) return 0;
            *col = pal16(po + idx * 2);
            return 31;
        case 1:
            if (idx == 3) return 0;
            if (idx == 2) *col = blend555(pal16(po), pal16(po + 2), 1, 1, 1);
            else *col = pal16(po + idx * 2);
            return 31;
        case 2:
            *col = pal16(po + idx * 2);
            return 31;
        default:
            if (idx == 2) *col = blend555(pal16(po), pal16(po + 2), 5, 3, 3);
            else if (idx == 3) *col = blend555(pal16(po), pal16(po + 2), 3, 5, 3);
            else *col = pal16(po + idx * 2);
            return 31;
        }
    }
    case 6:
        v = tex8(a + i);
        *col = pal16(ti->palbase + (v & 7) * 2);
        return v >> 3;
    case 7:
        v = tex16(a + i * 2);
        *col = (u16)v;
        return (v & 0x8000) ? 31 : 0;
    default:
        *col = 0x7fff;
        return 31;
    }
}

typedef struct {
    u32 disp3dcnt;
    int wbuf, blend, atest, aref, texen, highlight;
    u16 toon[32];
} rctx_t;


#define NA 7 /* interpolated: 1/w, z, r/w, g/w, b/w, s/w, t/w */
static unsigned g_frags[64];

/* rasterise one polygon into the rows [by0, by1) of the RW x RH buffers; *shadow_drawn: a non-mask shadow polygon
 * was drawn in this band since its stencil was last cleared */
static void raster_poly(const poly_t *p, const rctx_t *rc, int by0, int by1, int *shadow_drawn)
{
    float at[MAX_PV][NA], px[MAX_PV], py[MAX_PV];
    int n = p->nv, i, y, y0, y1, wy0, wy1, k;
    u32 attr = p->attr;
    int mode = (attr >> 4) & 3;
    int palpha = (attr >> 16) & 31;
    int wire = palpha == 0;
    int pid = (attr >> 24) & 63;
    int fog = (attr >> 15) & 1;
    int deq = (attr >> 14) & 1;
    int trans_dw = (attr >> 11) & 1;
    int fmt = (p->tex >> 26) & 7;
    int textured = rc->texen && fmt != 0;
    texinfo_t ti;

    memset(&ti, 0, sizeof(ti));

    if (wire) palpha = 31;
    if (mode == 3) {
        if (pid == 0) {
            if (*shadow_drawn) {
                for (i = by0 * RW; i < by1 * RW; i++) rb_flags[i] &= ~4;
                *shadow_drawn = 0;
            }
        } else
            *shadow_drawn = 1;
    }
    if (textured) {
        ti.addr = (p->tex & 0xffff) << 3;
        ti.fmt = fmt;
        ti.w = 8 << ((p->tex >> 20) & 7);
        ti.h = 8 << ((p->tex >> 23) & 7);
        ti.rs = (p->tex >> 16) & 1;
        ti.rt = (p->tex >> 17) & 1;
        ti.fs = (p->tex >> 18) & 1;
        ti.ft = (p->tex >> 19) & 1;
        ti.transp0 = (p->tex >> 29) & 1;
        ti.palbase = fmt == 2 ? p->pal << 3 : p->pal << 4;
    }
    for (i = 0; i < n; i++) {
        const vtx_t *v = &p->v[i];
        float iw = 1.0f / v->f[VW];
        at[i][0] = iw;
        at[i][1] = v->f[VZ];
        at[i][2] = v->f[VR] * iw;
        at[i][3] = v->f[VG] * iw;
        at[i][4] = v->f[VB] * iw;
        at[i][5] = v->f[VS] * iw;
        at[i][6] = v->f[VT] * iw;
        px[i] = v->f[VX] * (float)g_scale;
        py[i] = v->f[VY] * (float)g_scale;
    }
    {
        float f = p->ymin * (float)g_scale - 0.5f;
        y0 = (int)f;
        if ((float)y0 < f) y0++; /* ceil */
        f = p->ymax * (float)g_scale - 0.5f;
        y1 = (int)f;
        if ((float)y1 < f) y1++;
    }
    wy0 = y0;
    wy1 = y1;
    if (y0 < by0) y0 = by0;
    if (y1 > by1) y1 = by1;

    for (y = y0; y < y1; y++) {
        float yc = (float)y + 0.5f;
        float xl = 1e30f, xr = -1e30f, L[NA], R[NA], st[NA], cur[NA], dx, f;
        int x0, x1, x;
        for (i = 0; i < n; i++) {
            int ia = i, ib = (i + 1) % n;
            float ya = py[ia], yb = py[ib], t, x;
            if (ya == yb) continue;
            if (ya > yb) {
                ia = ib; ib = i;
                ya = py[ia]; yb = py[ib];
            }
            if (yc < ya || yc >= yb) continue;
            t = (yc - ya) / (yb - ya);
            x = px[ia] + (px[ib] - px[ia]) * t;
            if (x < xl) {
                xl = x;
                for (k = 0; k < NA; k++) L[k] = at[ia][k] + (at[ib][k] - at[ia][k]) * t;
            }
            if (x > xr) {
                xr = x;
                for (k = 0; k < NA; k++) R[k] = at[ia][k] + (at[ib][k] - at[ia][k]) * t;
            }
        }
        if (xl > xr) continue;
        f = xl - 0.5f; x0 = (int)f; if ((float)x0 < f) x0++;
        f = xr - 0.5f; x1 = (int)f; if ((float)x1 < f) x1++;
        if (x1 <= x0) continue;
        dx = xr - xl;
        for (k = 0; k < NA; k++) {
            st[k] = dx > 0 ? (R[k] - L[k]) / dx : 0;
            cur[k] = L[k] + ((float)x0 + 0.5f - xl) * st[k];
        }
        if (x0 < 0) {
            for (k = 0; k < NA; k++) cur[k] += st[k] * (float)(-x0);
            x0 = 0;
        }
        if (x1 > RW) x1 = RW;
        g_frags[by0 & 63] += x1 - x0;
        for (x = x0; x < x1; x++, ({ for (k = 0; k < NA; k++) cur[k] += st[k]; })) {
            int idx = y * RW + x;
            float w = 1.0f / cur[0];
            u32 depth, dd;
            int r, g, b, a, pass;
            if (wire && !(x == x0 || x == x1 - 1 || y == wy0 || y == wy1 - 1))
                continue;
            if (rc->wbuf) {
                float wd = w;
                depth = wd >= 16777215.0f ? 0xffffff : wd <= 0 ? 0 : (u32)wd;
            } else
                depth = (u32)cur[1];
            dd = rb_depth[idx];
            if (deq)
                pass = (depth > dd ? depth - dd : dd - depth) <= 0x200;
            else
                pass = depth < dd;

            if (mode == 3 && pid == 0) { /* shadow mask: stencil where the depth test fails */
                if (!pass) rb_flags[idx] |= 4;
                continue;
            }
            if (!pass) continue;
            if (mode == 3) {
                if (!(rb_flags[idx] & 4)) continue;
                rb_flags[idx] &= ~4;
                if (rb_opid[idx] == pid) continue;
            }

            r = (int)(cur[2] * w + 0.5f);
            g = (int)(cur[3] * w + 0.5f);
            b = (int)(cur[4] * w + 0.5f);
            r = r < 0 ? 0 : r > 63 ? 63 : r;
            g = g < 0 ? 0 : g > 63 ? 63 : g;
            b = b < 0 ? 0 : b > 63 ? 63 : b;
            a = palpha;

            if (mode == 2) {
                u16 tc = rc->toon[r >> 1];
                if (!rc->highlight) {
                    r = c5to6(tc & 31); g = c5to6((tc >> 5) & 31); b = c5to6((tc >> 10) & 31);
                } else {
                    g = b = r; /* highlight: modulate with the red component, add toon colour below */
                }
            }
            if (textured) {
                u16 tc;
                int ta, tr, tg, tb;
                float sf = cur[5] * w + 0.01f, tf = cur[6] * w + 0.01f;
                int s = (int)sf, t = (int)tf;
                if ((float)s > sf) s--;
                if ((float)t > tf) t--;
                ta = tex_sample(&ti, s >> 4, t >> 4, &tc);
                if (ta == 0) continue;
                tr = c5to6(tc & 31); tg = c5to6((tc >> 5) & 31); tb = c5to6((tc >> 10) & 31);
                if (mode == 1) { /* decal */
                    if (ta == 31) { r = tr; g = tg; b = tb; }
                    else if (ta > 0) {
                        r = (tr * ta + r * (31 - ta)) >> 5;
                        g = (tg * ta + g * (31 - ta)) >> 5;
                        b = (tb * ta + b * (31 - ta)) >> 5;
                    }
                } else { /* modulate (also toon/highlight/shadow) */
                    r = ((tr + 1) * (r + 1) - 1) >> 6;
                    g = ((tg + 1) * (g + 1) - 1) >> 6;
                    b = ((tb + 1) * (b + 1) - 1) >> 6;
                    a = ((ta + 1) * (a + 1) - 1) >> 5;
                }
            }
            if (mode == 2 && rc->highlight) {
                u16 tc = rc->toon[((int)(cur[2] * w + 0.5f) >> 1) & 31];
                r += c5to6(tc & 31); g += c5to6((tc >> 5) & 31); b += c5to6((tc >> 10) & 31);
                r = r > 63 ? 63 : r;
                g = g > 63 ? 63 : g;
                b = b > 63 ? 63 : b;
            }
            if (a == 0) continue;
            if (rc->atest && a <= rc->aref) continue;

            if (a >= 31) {
                rb_color[idx] = (u32)r | ((u32)g << 8) | ((u32)b << 16) | (31u << 24);
                rb_depth[idx] = depth;
                rb_opid[idx] = (u8)pid;
                rb_trid[idx] = 0xff;
                rb_flags[idx] = (u8)((rb_flags[idx] & 4) | fog | 2);
            } else {
                u32 dc = rb_color[idx];
                int da = dc >> 24;
                if (rb_trid[idx] == pid) continue;
                if (rc->blend && da > 0) {
                    r = (r * (a + 1) + (int)(dc & 63) * (31 - a)) >> 5;
                    g = (g * (a + 1) + (int)((dc >> 8) & 63) * (31 - a)) >> 5;
                    b = (b * (a + 1) + (int)((dc >> 16) & 63) * (31 - a)) >> 5;
                    if (da > a) a = da;
                }
                rb_color[idx] = (u32)r | ((u32)g << 8) | ((u32)b << 16) | ((u32)a << 24);
                if (trans_dw) rb_depth[idx] = depth;
                rb_trid[idx] = (u8)pid;
                rb_flags[idx] = (u8)((rb_flags[idx] & ~1) | (rb_flags[idx] & fog & 1));
            }
        }
    }
}

static const poly_t *g_sort_polys;
static int sort_cmp(int a, int b)
{
    const poly_t *pa = &g_sort_polys[a], *pb = &g_sort_polys[b];
    if (pa->ymax != pb->ymax) return pa->ymax < pb->ymax ? -1 : 1;
    if (pa->ymin != pb->ymin) return pa->ymin < pb->ymin ? -1 : 1;
    return a - b;
}

static void sort_idx(int *v, int n) /* merge sort (stable, keys compared through sort_cmp) */
{
    static int tmp[MAX_POLY];
    int width, i;
    for (width = 1; width < n; width *= 2) {
        for (i = 0; i < n; i += 2 * width) {
            int lo = i, mid = i + width < n ? i + width : n, hi = i + 2 * width < n ? i + 2 * width : n;
            int a = lo, b = mid, k = lo;
            while (a < mid && b < hi) tmp[k++] = sort_cmp(v[a], v[b]) <= 0 ? v[a++] : v[b++];
            while (a < mid) tmp[k++] = v[a++];
            while (b < hi) tmp[k++] = v[b++];
        }
        memcpy(v, tmp, sizeof(int) * n);
    }
}

static inline u16 io16(const u8 *io, u32 o) { return (u16)(io[o] | (io[o + 1] << 8)); }
static inline u32 io32(const u8 *io, u32 o) { return io16(io, o) | ((u32)io16(io, o + 2) << 16); }
static inline u32 c6to8(u32 c) { return (c << 2) | (c >> 4); }

/* ---- frame: banded rendering ---------------------------------------------------------------------------- */
typedef struct {
    const plist_t *L;
    const rctx_t *rc;
    const int *order;
    int n, bands;
    const u8 *io;
    u32 clr, cc, cdepth;
} RenderJob;

static u32 g_hires[NPIX];  /* last frame at RW x RH, 0x80RRGGBB (bit 31 = opaque) */

static void band_rows(const RenderJob *j, int b, int *y0, int *y1)
{
    *y0 = RH * b / j->bands;
    *y1 = RH * (b + 1) / j->bands;
}

static void raster_band(int b, void *arg)
{
    const RenderJob *j = arg;
    int y0, y1, i, shadow = 0;
    band_rows(j, b, &y0, &y1);
    for (i = y0 * RW; i < y1 * RW; i++) {
        rb_color[i] = j->cc;
        rb_depth[i] = j->cdepth;
    }
    memset(rb_opid + y0 * RW, (j->clr >> 24) & 63, (size_t)(y1 - y0) * RW);
    memset(rb_trid + y0 * RW, 0xff, (size_t)(y1 - y0) * RW);
    memset(rb_flags + y0 * RW, (j->clr >> 15) & 1, (size_t)(y1 - y0) * RW);
    for (i = 0; i < j->n; i++)
        raster_poly(&j->L->poly[j->order[i]], j->rc, y0, y1, &shadow);
}

static void finish_band(int b, void *arg)
{
    const RenderJob *j = arg;
    const u8 *io = j->io;
    u32 disp3dcnt = j->rc->disp3dcnt;
    int y0, y1, x, y, i;
    band_rows(j, b, &y0, &y1);

    /* edge marking: opaque polygon pixels in front of a neighbour with a different polygon ID (the neighbour
     * buffers are final: all bands finished rasterising) */
    for (y = y0; y < y1; y++)
        for (x = 0; x < RW; x++) {
            int idx = y * RW + x, k, edge = 0;
            u8 id = rb_opid[idx];
            u32 d = rb_depth[idx], c = rb_color[idx];
            if ((disp3dcnt & 0x20) && (rb_flags[idx] & 2)) {
                for (k = 0; k < 4 && !edge; k++) {
                    int nx = x + (k == 0 ? -g_scale : k == 1 ? g_scale : 0), ny = y + (k == 2 ? -g_scale : k == 3 ? g_scale : 0);
                    u8 nid;
                    u32 nd;
                    if (nx < 0 || nx >= RW || ny < 0 || ny >= RH) { nid = (j->clr >> 24) & 63; nd = j->cdepth; }
                    else { nid = rb_opid[ny * RW + nx]; nd = rb_depth[ny * RW + nx]; }
                    if (nid != id && d < nd) edge = 1;
                }
                if (edge) {
                    u16 e = io16(io, 0x330 + (id >> 3) * 2);
                    c = c5to6(e & 31) | (c5to6((e >> 5) & 31) << 8) | (c5to6((e >> 10) & 31) << 16) | (c & 0xff000000u);
                }
            }
            g_hires[idx] = c;  /* 6-bit colour for now */
        }

    /* fog */
    if (disp3dcnt & 0x80) {
        int shift = (disp3dcnt >> 8) & 15, alpha_only = (disp3dcnt >> 6) & 1;
        int step = shift > 10 ? 1 : 0x400 >> shift;
        int offset = io16(io, 0x35c) & 0x7fff;
        u32 fc = io32(io, 0x358);
        int fr = c5to6(fc & 31), fg = c5to6((fc >> 5) & 31), fb = c5to6((fc >> 10) & 31), fa = (fc >> 16) & 31;
        u8 dens_tab[32];
        for (i = 0; i < 32; i++) dens_tab[i] = io[0x360 + i] & 127;
        for (i = y0 * RW; i < y1 * RW; i++) {
            int d, dens, pos, frac;
            u32 c;
            if (!(rb_flags[i] & 1)) continue;
            d = (int)(rb_depth[i] >> 9);
            /* entry k applies at offset + (k+1)*step; linear in between */
            pos = d - offset - step;
            if (pos <= 0) dens = dens_tab[0];
            else {
                int k = pos / step;
                frac = pos % step;
                if (k >= 31) dens = dens_tab[31];
                else dens = (dens_tab[k] * (step - frac) + dens_tab[k + 1] * frac) / step;
            }
            if (dens >= 127) dens = 128;
            c = g_hires[i];
            {
                int r = c & 63, g = (c >> 8) & 63, bb = (c >> 16) & 63, a = c >> 24;
                if (!alpha_only) {
                    r = (fr * dens + r * (128 - dens)) >> 7;
                    g = (fg * dens + g * (128 - dens)) >> 7;
                    bb = (fb * dens + bb * (128 - dens)) >> 7;
                }
                a = (fa * dens + a * (128 - dens)) >> 7;
                g_hires[i] = (u32)r | ((u32)g << 8) | ((u32)bb << 16) | ((u32)a << 24);
            }
        }
    }

    /* to 0x80RRGGBB */
    for (i = y0 * RW; i < y1 * RW; i++) {
        u32 c = g_hires[i];
        u32 o = (c6to8(c & 63) << 16) | (c6to8((c >> 8) & 63) << 8) | c6to8((c >> 16) & 63);
        g_hires[i] = (c >> 24) ? o | 0x80000000u : o;
    }
}

/* the native 256x192 frame for the 2D compositor: each pixel the average of its g_scale x g_scale block (opaque if at
 * least half of it is) */
static void downsample(u32 *out)
{
    int x, y, sx, sy, n2 = g_scale * g_scale;
    if (g_scale == 1) {
        for (y = 0; y < DS_H; y++)
            memcpy(out + y * DS_W, g_hires + (y + g_yoff) * RW + g_xoff, DS_W * 4);
        return;
    }
    for (y = 0; y < DS_H; y++)
        for (x = 0; x < DS_W; x++) {
            u32 r = 0, g = 0, b = 0, cnt = 0;
            for (sy = 0; sy < g_scale; sy++) {
                const u32 *p = g_hires + ((y + g_yoff) * g_scale + sy) * RW + (x + g_xoff) * g_scale;
                for (sx = 0; sx < g_scale; sx++)
                    if (p[sx] >> 31) {
                        r += (p[sx] >> 16) & 255; g += (p[sx] >> 8) & 255; b += p[sx] & 255;
                        cnt++;
                    }
            }
            out[y * DS_W + x] = cnt * 2 >= (u32)n2 ? 0x80000000u | (r / cnt) << 16 | (g / cnt) << 8 | (b / cnt) : 0;
        }
}

void gx_set_scale(int sc)
{
    g_scale = sc < 1 ? 1 : sc > MAXS ? MAXS : sc;
    RW = g_wpx * g_scale;
    RH = g_hpx * g_scale;
}
int gx_scale(void) { return g_scale; }
const u32 *gx_hires(void) { return g_hires + (g_yoff * RW + g_xoff) * g_scale; }  /* the DS screen's part */
int gx_stride(void) { return RW; }
int gx_wide_px(void) { return g_wpx; }
int gx_tall_px(void) { return g_hpx; }
const u32 *gx_hires_wide(void) { return g_hires; }

/* the 3D view's width and height over the DS screen's (each 1 to 2, their product at most 2: the buffers); takes
 * effect at the next render */
void gx_set_view(float wf, float hf)
{
    int w = (int)(DS_W * wf / 2 + 0.5f) * 2, h = (int)(DS_H * hf / 2 + 0.5f) * 2;
    w = w < DS_W ? DS_W : w > DS_W * WMAX ? DS_W * WMAX : w;
    h = h < DS_H ? DS_H : h > DS_H * WMAX ? DS_H * WMAX : h;
    if (w * h > DS_W * DS_H * WMAX)
        h = DS_W * DS_H * WMAX / w / 2 * 2;
    g_wpx = w;
    g_xoff = (w - DS_W) / 2;
    g_wide = (float)w / DS_W;
    g_hpx = h;
    g_yoff = (h - DS_H) / 2;
    g_tall = (float)h / DS_H;
    RW = g_wpx * g_scale;
    RH = g_hpx * g_scale;
}

static void render_list(const plist_t *L, const u8 *io, u32 *out, int flatten)
{
    static int order[MAX_POLY];
    rctx_t rc;
    u32 clr = io32(io, 0x350), cdepth = (io16(io, 0x354) & 0x7fff) * 0x200 + 0x1ff;
    u32 cc;
    int i, nop = 0, ntr = 0, n;

    rc.disp3dcnt = io16(io, 0x60);
    rc.texen = rc.disp3dcnt & 1;
    rc.highlight = (rc.disp3dcnt >> 1) & 1;
    rc.atest = (rc.disp3dcnt >> 2) & 1;
    rc.blend = (rc.disp3dcnt >> 3) & 1;
    rc.aref = io[0x340] & 31;
    rc.wbuf = (L->swapflags >> 1) & 1;
    for (i = 0; i < 32; i++)
        rc.toon[i] = io16(io, 0x380 + i * 2);

    double tp0 = now_ms(), tp1, tp2;
    static RenderJob rj;
    if (flatten) {  /* interpolated frames reuse the textures of the list's VBlank */
        vram_flatten(VRAM_TEX, tex_mem, sizeof tex_mem);
        vram_flatten(VRAM_TEXPAL, texpal_mem, 0x18000);
    }

    /* opaque polygons first (Y-sorted), then translucent (Y-sorted unless manual sort) */
    g_sort_polys = L->poly;
    for (i = 0; i < L->npoly; i++)
        if (!L->poly[i].translucent) order[nop++] = i;
    sort_idx(order, nop);
    for (i = 0; i < L->npoly; i++)
        if (L->poly[i].translucent) order[nop + ntr++] = i;
    if (!(L->swapflags & 1))
        sort_idx(order + nop, ntr);
    n = nop + ntr;

    {
        u32 r = c5to6(clr & 31), g = c5to6((clr >> 5) & 31), b = c5to6((clr >> 10) & 31);
        cc = r | (g << 8) | (b << 16) | (((clr >> 16) & 31) << 24);
    }
    rj.L = L;
    rj.rc = &rc;
    rj.order = order;
    rj.n = n;
    rj.io = io;
    rj.clr = clr;
    rj.cc = cc;
    rj.cdepth = cdepth;
    rj.bands = workers_count() * 2;
    if (rj.bands > RH)
        rj.bands = RH;
    tp1 = now_ms();
    memset(g_frags, 0, sizeof g_frags);
    workers_run(rj.bands, raster_band, &rj);   /* rear plane + polygons */
    double tpf = now_ms();
    workers_run(rj.bands, finish_band, &rj);   /* edge marking, fog, output */
    double tpd = now_ms();
    downsample(out);
    tp2 = now_ms();
    if (getenv("NDOGS_VLOG")) {
        unsigned fr = 0;
        for (i = 0; i < 64; i++)
            fr += g_frags[i];
        fprintf(stderr, "3d: %d polys (%d translucent), setup %.2fms raster %.2fms at %dx (polys %.2f finish %.2f "
                "down %.2f, %u fragments)\n", n, ntr, tp1 - tp0, tp2 - tp1, g_scale, tpf - tp1, tpd - tpf, tp2 - tpd, fr);
    }
}

/* pair the batches of recording c with recording p's: same primitive type, vertex count and texture, in order (a
 * batch may be missing or added: searched a little ahead); cached */
static const short *pair_match(int c, int p)
{
    const rec_t *C = &g_rec[c], *P = &g_rec[p];
    match_t *M;
    int i, j = 0, k;
    for (k = 0; k < 4; k++)
        if (g_match[k].c == c && g_match[k].p == p)
            return g_match[k].m;
    M = &g_match[g_match_next++ & 3];
    M->c = c;
    M->p = p;
    for (i = 0; i < C->nb; i++) {
        const rbat_t *b = &C->b[i];
        M->m[i] = -1;
        for (k = j; k < P->nb && k < j + 32; k++) {
            const rbat_t *a = &P->b[k];
            if (a->prim == b->prim && a->n == b->n &&
                (!b->n || P->v[a->first].tex == C->v[b->first].tex)) {
                M->m[i] = (short)k;
                j = k + 1;
                break;
            }
        }
    }
    return M->m;
}

static void rec_rotate(void)
{
    if (r_swapped < 0)
        return;
    r_hist[3] = r_hist[2];
    r_hist[2] = r_hist[1];
    r_hist[1] = r_hist[0];
    r_hist[0] = r_swapped;
    r_swapped = -1;
    g_rec[r_cur].time = now_ms();
    r_seq++;
}

unsigned gx_list_seq(void) { return r_seq; }

void gx_render_swapped(const u8 *io, u32 *out)
{
    render_list(&g_list[g_cur ^ 1], io, out, 1);
    g_swap_pending = 0;
    rec_rotate();
}

/* VBlank with frame interpolation: every frame presented will be an interpolated one (gx_render_interp), so the
 * swapped list itself needn't be rasterised (half the work), only its textures taken. Rasterised as usual (0) if
 * it can't be interpolated with the one before. */
int gx_take_swapped(const u8 *io, u32 *out)
{
    if (r_swapped >= 0 && r_cur >= 0 && !g_rec[r_swapped].bad && !g_rec[r_cur].bad &&
        now_ms() - g_rec[r_cur].time < 50.0) {
        vram_flatten(VRAM_TEX, tex_mem, sizeof tex_mem);
        vram_flatten(VRAM_TEXPAL, texpal_mem, 0x18000);
        g_swap_pending = 0;
        rec_rotate();
        return 1;
    }
    gx_render_swapped(io, out);
    return 0;
}

/* ---- frame interpolation: replay ---------------------------------------------------------------------------- */
/* row-vector 4x4 matrices as recorded (raw 20.12 values as float): rows 0-2 the basis, row 3 the translation */
static void quat_from_rows(const float r[3][3], float *q)
{
    float tr = r[0][0] + r[1][1] + r[2][2], s;
    if (tr > 0) {
        s = sqrtf(tr + 1.0f) * 2;
        q[0] = 0.25f * s; q[1] = (r[1][2] - r[2][1]) / s; q[2] = (r[2][0] - r[0][2]) / s; q[3] = (r[0][1] - r[1][0]) / s;
    } else if (r[0][0] > r[1][1] && r[0][0] > r[2][2]) {
        s = sqrtf(1.0f + r[0][0] - r[1][1] - r[2][2]) * 2;
        q[0] = (r[1][2] - r[2][1]) / s; q[1] = 0.25f * s; q[2] = (r[1][0] + r[0][1]) / s; q[3] = (r[2][0] + r[0][2]) / s;
    } else if (r[1][1] > r[2][2]) {
        s = sqrtf(1.0f + r[1][1] - r[0][0] - r[2][2]) * 2;
        q[0] = (r[2][0] - r[0][2]) / s; q[1] = (r[1][0] + r[0][1]) / s; q[2] = 0.25f * s; q[3] = (r[2][1] + r[1][2]) / s;
    } else {
        s = sqrtf(1.0f + r[2][2] - r[0][0] - r[1][1]) * 2;
        q[0] = (r[0][1] - r[1][0]) / s; q[1] = (r[2][0] + r[0][2]) / s; q[2] = (r[2][1] + r[1][2]) / s; q[3] = 0.25f * s;
    }
}

static void rows_from_quat(const float *q, float r[3][3])
{
    float w = q[0], x = q[1], y = q[2], z = q[3];
    r[0][0] = 1 - 2 * (y * y + z * z); r[0][1] = 2 * (x * y + w * z);     r[0][2] = 2 * (x * z - w * y);
    r[1][0] = 2 * (x * y - w * z);     r[1][1] = 1 - 2 * (x * x + z * z); r[1][2] = 2 * (y * z + w * x);
    r[2][0] = 2 * (x * z + w * y);     r[2][1] = 2 * (y * z - w * x);     r[2][2] = 1 - 2 * (x * x + y * y);
}

/* split a matrix's basis into scales and a rotation; 0 if it isn't one (degenerate or mirrored) */
static int decompose(const float *m, float sc[3], float r[3][3])
{
    int i, j;
    float det;
    for (i = 0; i < 3; i++) {
        sc[i] = sqrtf(m[i * 4] * m[i * 4] + m[i * 4 + 1] * m[i * 4 + 1] + m[i * 4 + 2] * m[i * 4 + 2]);
        if (sc[i] < 1e-3f)
            return 0;
        for (j = 0; j < 3; j++)
            r[i][j] = m[i * 4 + j] / sc[i];
    }
    det = r[0][0] * (r[1][1] * r[2][2] - r[1][2] * r[2][1]) - r[0][1] * (r[1][0] * r[2][2] - r[1][2] * r[2][0]) +
          r[0][2] * (r[1][0] * r[2][1] - r[1][1] * r[2][0]);
    return det > 0.5f;
}

/* interpolate a position matrix: rotation along the shorter arc (a turn of over ~100 degrees in one frame is a cut:
 * snap to the new one), scale and translation linearly; other matrices element by element */
static void mtx_interp(const float *a, const float *b, float t, float *o)
{
    float sa[3], sb[3], ra[3][3], rb[3][3], qa[4], qb[4], q[4], d = 0, len = 0, r[3][3];
    int i, j;
    if (!decompose(a, sa, ra) || !decompose(b, sb, rb)) {
        for (i = 0; i < 16; i++)
            o[i] = a[i] + (b[i] - a[i]) * t;
        return;
    }
    quat_from_rows(ra, qa);
    quat_from_rows(rb, qb);
    for (i = 0; i < 4; i++)
        d += qa[i] * qb[i];
    if (d < 0) {
        d = -d;
        for (i = 0; i < 4; i++)
            qa[i] = -qa[i];
    }
    if (d < 0.64f) {  /* cos(50 degrees): half the angle */
        memcpy(o, b, 64);
        return;
    }
    for (i = 0; i < 4; i++) {
        q[i] = qa[i] + (qb[i] - qa[i]) * t;  /* nlerp: equal to slerp for the small angles between frames */
        len += q[i] * q[i];
    }
    len = 1.0f / sqrtf(len);
    for (i = 0; i < 4; i++)
        q[i] *= len;
    rows_from_quat(q, r);
    for (i = 0; i < 3; i++) {
        float s = sa[i] + (sb[i] - sa[i]) * t;
        for (j = 0; j < 3; j++)
            o[i * 4 + j] = r[i][j] * s;
        o[i * 4 + 3] = a[i * 4 + 3] + (b[i * 4 + 3] - a[i * 4 + 3]) * t;
    }
    for (j = 0; j < 4; j++)
        o[12 + j] = a[12 + j] + (b[12 + j] - a[12 + j]) * t;
}

/* clip matrix = position matrix x projection, both interpolated */
static void clip_interp(const rmtx_t *a, const rmtx_t *b, float t, float *c)
{
    float pos[16], proj[16];
    int i, j;
    mtx_interp(a->pos, b->pos, t, pos);
    for (i = 0; i < 16; i++)
        proj[i] = a->proj[i] + (b->proj[i] - a->proj[i]) * t;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            c[i * 4 + j] = (pos[i * 4] * proj[j] + pos[i * 4 + 1] * proj[4 + j] + pos[i * 4 + 2] * proj[8 + j] +
                            pos[i * 4 + 3] * proj[12 + j]) / 4096.0f;
}

/* Interpolation position at time <now>: 0 = the previous list, 1 = the shown one, which arrived one frame after
 * it; -1 if there is nothing to interpolate (no pair, a gap of over 3 frames, or a list that overflowed the
 * recording). Showing the previous-to-shown motion delays the 3D by one game frame, as in other ports. */
float gx_interp_pos(double now)
{
    double span;
    if (r_cur < 0 || r_prev < 0 || g_rec[r_cur].bad || g_rec[r_prev].bad)
        return -1;
    span = g_rec[r_cur].time - g_rec[r_prev].time;
    if (span <= 0 || span > 50.0)
        return -1;
    now = (now - g_rec[r_cur].time) / span;
    return now < 0 ? 0.0f : now > 1 ? 1.0f : (float)now;
}

/* Render the 3D frame between two shown lists (ages: 0 the newest, up to 3) at t (0 the older, 1 the newer) into out
 * and gx_hires(). 0 (nothing rendered) if they aren't both there and recorded. */
int gx_render_pair(const u8 *io, int newer, int older, float t, u32 *out)
{
    static plist_t L;
    static float cm[REC_M][16];
    static int cm_prev[REC_M];
    const rec_t *C, *P;
    vtx_t v;
    int i, k, attr_save = g_attr, prim_save = g_prim, nv_save = g_nv, odd_save = g_strip_odd;
    int vp_save[4] = { g_vp_x1, g_vp_y1, g_vp_w, g_vp_h };
    u32 tex_save = g_teximage, pal_save = g_pltt;
    vtx_t vbuf_save[4];
    double t0 = now_ms(), tr;
    const short *match;
    if (t < 0 || newer < 0 || newer > 3 || older < 0 || older > 3 || r_hist[newer] < 0 || r_hist[older] < 0 ||
        g_rec[r_hist[newer]].bad || g_rec[r_hist[older]].bad)
        return 0;
    C = &g_rec[r_hist[newer]];
    P = &g_rec[r_hist[older]];
    match = pair_match(r_hist[newer], r_hist[older]);
    memcpy(vbuf_save, g_vbuf, sizeof vbuf_save);
    for (i = 0; i < C->nm; i++)
        cm_prev[i] = -2;  /* clip matrix for C's matrix i not computed yet */
    L.npoly = 0;
    L.swapflags = C->swapflags;
    g_emit = &L;
    for (k = 0; k < C->nb; k++) {
        const rbat_t *b = &C->b[k];
        const rbat_t *a = match[k] >= 0 ? &P->b[match[k]] : 0;
        g_attr = b->attr;
        g_prim = b->prim;
        g_vp_x1 = b->vp[0]; g_vp_y1 = b->vp[1]; g_vp_w = b->vp[2]; g_vp_h = b->vp[3];
        g_nv = 0;
        g_strip_odd = 0;
        for (i = 0; i < b->n; i++) {
            const rvtx_t *cv = &C->v[b->first + i], *pv = a ? &P->v[a->first + i] : cv;
            float x, y, z, *m, tt = a ? t : 1.0f;
            int j;
            if (cm_prev[cv->m] != (a ? pv->m : -1)) {  /* the matrix pair (cached: bones repeat) */
                cm_prev[cv->m] = a ? pv->m : -1;
                if (a)
                    clip_interp(&P->m[pv->m], &C->m[cv->m], t, cm[cv->m]);
                else
                    clip_interp(&C->m[cv->m], &C->m[cv->m], 1.0f, cm[cv->m]);
            }
            m = cm[cv->m];
            x = pv->x + (cv->x - pv->x) * tt;
            y = pv->y + (cv->y - pv->y) * tt;
            z = pv->z + (cv->z - pv->z) * tt;
            for (j = 0; j < 4; j++)
                v.f[VX + j] = (x * m[j] + y * m[4 + j] + z * m[8 + j]) / 4096.0f + m[12 + j];
            v.f[VR] = pv->r + (cv->r - pv->r) * tt;
            v.f[VG] = pv->g + (cv->g - pv->g) * tt;
            v.f[VB] = pv->b + (cv->b - pv->b) * tt;
            if (abs(cv->s - pv->s) < 4096 && abs(cv->t - pv->t) < 4096) {  /* not across a texture wrap */
                v.f[VS] = pv->s + (cv->s - pv->s) * tt;
                v.f[VT] = pv->t + (cv->t - pv->t) * tt;
            } else {
                v.f[VS] = (float)cv->s;
                v.f[VT] = (float)cv->t;
            }
            g_teximage = cv->tex;
            g_pltt = cv->pal;
            assemble(&v);
        }
    }
    g_emit = 0;
    g_attr = attr_save; g_prim = prim_save; g_nv = nv_save; g_strip_odd = odd_save;
    g_vp_x1 = vp_save[0]; g_vp_y1 = vp_save[1]; g_vp_w = vp_save[2]; g_vp_h = vp_save[3];
    g_teximage = tex_save; g_pltt = pal_save;
    memcpy(g_vbuf, vbuf_save, sizeof vbuf_save);
    tr = now_ms();
    render_list(&L, io, out, 0);
    if (getenv("NDOGS_ILOG")) {
        int matched = 0;
        for (k = 0; k < C->nb; k++)
            matched += match[k] >= 0;
        fprintf(stderr, "interp: replay %.2fms raster %.2fms, %d polys, %d/%d batches matched, %d matrices\n",
                tr - t0, now_ms() - tr, L.npoly, matched, C->nb, C->nm);
    }
    return 1;
}

/* the shown list interpolated from the previous one (one screen with 3D) */
int gx_render_interp(const u8 *io, float t, u32 *out)
{
    return gx_render_pair(io, 0, 1, t, out);
}
