/* DS 3D rendering engine on the GPU (OpenGL 3.3 + texture barriers): the same polygon list, rules and output as the
 * CPU rasteriser in gpu3d.c (render_list -> raster_poly / finish_band), which stays the fallback and reference.
 * NDOGS_GPU3D=0 CPU, 1 OpenGL, 2 both with the outputs compared (NDOGS_GLDUMP=<render>,... saves both and the diff).
 *
 * Each polygon is drawn as its bounding box; the fragment shader walks the polygon's edges for its scanline exactly
 * as raster_poly does (span ends, attributes interpolated along the edges and then across, 1/w perspective), so
 * coverage and colours follow the CPU rasteriser rather than the GPU's triangle rules. Polygon data lives in a
 * buffer texture, textures are decoded once into an integer atlas (cached by parameters and a hash of their bytes).
 * Buffers (rw x rh): colour (RGBA8UI: 6-bit r g b, 5-bit alpha), attributes (RG32UI: depth, opaque ID | translucent
 * ID << 8 | flags << 16) and a 32-bit float depth buffer holding the same depth (exact for 24-bit values).
 * Ordinary opaque polygons are drawn in one call with the hardware depth test (LESS). Translucent, shadow and
 * depth-equal polygons read the buffers they write (translucent ID rules, blending with the destination alpha,
 * shadow stencil, the depth-equal margin): batches of polygons whose boxes don't overlap, separated by texture
 * barriers. A last pass does edge marking, fog and the 0x80RRGGBB conversion, another the native 256x192 frame;
 * both are read back for the 2D compositor. Windows only for now (a hidden window holds the context). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gpu3d_gl.h"
extern double now_ms(void);

#define PSTRIDE (3 + 3 * MAX_PV)  /* buffer texels (4 x 32 bits) per polygon */
#define ATLAS 2048

static int g_mode = -1;

int gl3d_mode(void)
{
    if (g_mode < 0) {
        const char *e = getenv("NDOGS_GPU3D");
#ifdef _WIN32
        g_mode = e ? atoi(e) : 1;
#else
        g_mode = e ? atoi(e) : 0;
#endif
        if (g_mode < 0 || g_mode > 2)
            g_mode = 0;
    }
    return g_mode;
}

#ifdef _WIN32
#include <windows.h>

typedef unsigned GLenum, GLuint, GLbitfield;
typedef int GLint, GLsizei;
typedef float GLfloat;
typedef ptrdiff_t GLsizeiptr, GLintptr;
typedef char GLchar;
typedef struct __GLsync *GLsync;
typedef unsigned long long GLuint64;

#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_BUFFER 0x8C2A
#define GL_RGBA32UI 0x8D70
#define GL_R32UI 0x8236
#define GL_RGBA8UI 0x8D7C
#define GL_RG32UI 0x823C
#define GL_RGBA_INTEGER 0x8D99
#define GL_RG_INTEGER 0x8228
#define GL_UNSIGNED_INT 0x1405
#define GL_UNSIGNED_BYTE 0x1401
#define GL_FLOAT 0x1406
#define GL_DEPTH_COMPONENT32F 0x8CAC
#define GL_DEPTH_COMPONENT 0x1902
#define GL_RGBA8 0x8058
#define GL_RGBA 0x1908
#define GL_BGRA 0x80E1
#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_NEAREST 0x2600
#define GL_STREAM_DRAW 0x88E0
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_TRIANGLES 0x0004
#define GL_DEPTH_TEST 0x0B71
#define GL_LESS 0x0201
#define GL_ALWAYS 0x0207
#define GL_COLOR 0x1800
#define GL_DEPTH 0x1801
#define GL_TEXTURE0 0x84C0
#define GL_PACK_ROW_LENGTH 0x0D02
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_UNPACK_ROW_LENGTH 0x0CF2
#define GL_VERSION 0x1F02
#define GL_RENDERER 0x1F01
#define GL_BLEND 0x0BE2
#define GL_DITHER 0x0BD0
#define GL_CULL_FACE 0x0B44
#define GL_PIXEL_PACK_BUFFER 0x88EB
#define GL_STREAM_READ 0x88E1
#define GL_MAP_READ_BIT 0x0001
#define GL_SYNC_GPU_COMMANDS_COMPLETE 0x9117
#define GL_SYNC_FLUSH_COMMANDS_BIT 0x0001
#define GL_TIMEOUT_EXPIRED 0x911B

#define GLFN(ret, name, args) static ret (WINAPI *name) args;
#define GL_FUNCS                                                                                                      \
    GLFN(void, glViewport, (GLint, GLint, GLsizei, GLsizei))                                                          \
    GLFN(void, glEnable, (GLenum))                                                                                    \
    GLFN(void, glDisable, (GLenum))                                                                                   \
    GLFN(void, glDepthFunc, (GLenum))                                                                                 \
    GLFN(void, glDepthMask, (unsigned char))                                                                          \
    GLFN(void, glGenTextures, (GLsizei, GLuint *))                                                                    \
    GLFN(void, glDeleteTextures, (GLsizei, const GLuint *))                                                           \
    GLFN(void, glBindTexture, (GLenum, GLuint))                                                                       \
    GLFN(void, glTexParameteri, (GLenum, GLenum, GLint))                                                              \
    GLFN(void, glTexImage2D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *))           \
    GLFN(void, glTexSubImage2D, (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *))        \
    GLFN(void, glPixelStorei, (GLenum, GLint))                                                                        \
    GLFN(void, glReadPixels, (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *))                                \
    GLFN(void, glDrawArrays, (GLenum, GLint, GLsizei))                                                                \
    GLFN(const unsigned char *, glGetString, (GLenum))                                                                \
    GLFN(GLenum, glGetError, (void))                                                                                  \
    GLFN(void, glActiveTexture, (GLenum))                                                                             \
    GLFN(void, glGenFramebuffers, (GLsizei, GLuint *))                                                                \
    GLFN(void, glBindFramebuffer, (GLenum, GLuint))                                                                   \
    GLFN(void, glFramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint))                                       \
    GLFN(GLenum, glCheckFramebufferStatus, (GLenum))                                                                  \
    GLFN(void, glDrawBuffers, (GLsizei, const GLenum *))                                                              \
    GLFN(void, glClearBufferuiv, (GLenum, GLint, const GLuint *))                                                     \
    GLFN(void, glClearBufferfv, (GLenum, GLint, const GLfloat *))                                                     \
    GLFN(void, glGenBuffers, (GLsizei, GLuint *))                                                                     \
    GLFN(void, glBindBuffer, (GLenum, GLuint))                                                                        \
    GLFN(void, glBufferData, (GLenum, GLsizeiptr, const void *, GLenum))                                              \
    GLFN(void, glBufferSubData, (GLenum, GLintptr, GLsizeiptr, const void *))                                         \
    GLFN(void, glTexBuffer, (GLenum, GLenum, GLuint))                                                                 \
    GLFN(GLuint, glCreateShader, (GLenum))                                                                            \
    GLFN(void, glShaderSource, (GLuint, GLsizei, const GLchar *const *, const GLint *))                               \
    GLFN(void, glCompileShader, (GLuint))                                                                             \
    GLFN(void, glGetShaderiv, (GLuint, GLenum, GLint *))                                                              \
    GLFN(void, glGetShaderInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *))                                            \
    GLFN(GLuint, glCreateProgram, (void))                                                                             \
    GLFN(void, glAttachShader, (GLuint, GLuint))                                                                      \
    GLFN(void, glLinkProgram, (GLuint))                                                                               \
    GLFN(void, glGetProgramiv, (GLuint, GLenum, GLint *))                                                             \
    GLFN(void, glGetProgramInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *))                                           \
    GLFN(void, glUseProgram, (GLuint))                                                                                \
    GLFN(GLint, glGetUniformLocation, (GLuint, const GLchar *))                                                       \
    GLFN(void, glUniform1i, (GLint, GLint))                                                                           \
    GLFN(void, glUniform2i, (GLint, GLint, GLint))                                                                    \
    GLFN(void, glUniform1ui, (GLint, GLuint))                                                                         \
    GLFN(void, glUniform1uiv, (GLint, GLsizei, const GLuint *))                                                       \
    GLFN(void, glUniform1iv, (GLint, GLsizei, const GLint *))                                                         \
    GLFN(void, glGenVertexArrays, (GLsizei, GLuint *))                                                                \
    GLFN(void, glBindVertexArray, (GLuint))                                                                           \
    GLFN(void, glFlush, (void))                                                                                       \
    GLFN(GLsync, glFenceSync, (GLenum, GLbitfield))                                                                   \
    GLFN(GLenum, glClientWaitSync, (GLsync, GLbitfield, GLuint64))                                                    \
    GLFN(void, glDeleteSync, (GLsync))                                                                                \
    GLFN(void *, glMapBufferRange, (GLenum, GLintptr, GLsizeiptr, GLbitfield))                                        \
    GLFN(unsigned char, glUnmapBuffer, (GLenum))                                                                      \
    GLFN(void, glGenQueries, (GLsizei, GLuint *))                                                                     \
    GLFN(void, glQueryCounter, (GLuint, GLenum))                                                                      \
    GLFN(void, glGetQueryObjectui64v, (GLuint, GLenum, GLuint64 *))                                                   \
    GLFN(void, glTextureBarrier, (void))
GL_FUNCS
#undef GLFN

/* ---- shaders ------------------------------------------------------------------------------------------------------ */
#define S(...) #__VA_ARGS__ "\n"
/* Arithmetic as the CPU rasteriser's (SSE, IEEE single precision): no fused multiply-adds (precise) and divisions
 * correctly rounded (a reciprocal refined by one FMA step), where GL_ARB_gpu_shader5 (OpenGL 4.0) has them. */
#define PREC_HEADER                                                                                                   \
    "#version 330\n"                                                                                                  \
    "#extension GL_ARB_gpu_shader5 : enable\n"                                                                        \
    "#ifdef GL_ARB_gpu_shader5\n"                                                                                     \
    "#define PR precise\n"                                                                                            \
    "float dv(float a, float b) { float r = 1.0 / b; precise float q = a * r; precise float e = fma(-q, b, a);"       \
    " return fma(e, r, q); }\n"                                                                                       \
    "#else\n"                                                                                                         \
    "#define PR\n"                                                                                                    \
    "float dv(float a, float b) { return a / b; }\n"                                                                  \
    "#endif\n"                                                                                                        \
    "vec4 dv4(vec4 a, float b) { return vec4(dv(a.x, b), dv(a.y, b), dv(a.z, b), dv(a.w, b)); }\n"

/* one row of a polygon (R: slot | y << 16): its span and attributes at the span's left end, as raster_poly works
 * them out per scanline; drawn as a one-pixel-high quad over [x0, x1) */
static const char vs_poly[] =
    PREC_HEADER
    S(uniform usamplerBuffer P;)
    S(uniform usamplerBuffer R;)
    S(uniform ivec2 uSize;)
    S(flat out int k;)
    S(flat out float xl;)
    S(flat out ivec2 span;)
    S(flat out vec4 La, Lb, Sa, Sb;)
    S(void main() {)
    S(    int r = gl_VertexID / 6, c = gl_VertexID - r * 6;)
    S(    uint rv = texelFetch(R, r).x;)
    S(    k = int(rv & 0xffffu);)
    S(    int y = int(rv >> 16), base = k * 33, n = int(texelFetch(P, base).x);)
    S(    float yc = float(y) + 0.5, xr = -1e30;)
    S(    vec4 Ra = vec4(0), Rb = vec4(0);)
    S(    xl = 1e30; La = vec4(0); Lb = vec4(0);)
    S(    for (int i = 0; i < n; i++) {)
    S(        int ia = base + 3 + i * 3, ib = base + 3 + (i + 1 == n ? 0 : i + 1) * 3;)
    S(        vec4 a0 = uintBitsToFloat(texelFetch(P, ia)), b0 = uintBitsToFloat(texelFetch(P, ib));)
    S(        if (a0.y == b0.y) continue;)
    S(        if (a0.y > b0.y) { int t = ia; ia = ib; ib = t; vec4 tv = a0; a0 = b0; b0 = tv; })
    S(        if (yc < a0.y || yc >= b0.y) continue;)
    S(        PR float t = dv(yc - a0.y, b0.y - a0.y);)
    S(        PR float x = a0.x + (b0.x - a0.x) * t;)
    S(        if (x < xl || x > xr) {)
    S(            vec4 a1 = uintBitsToFloat(texelFetch(P, ia + 1)), b1 = uintBitsToFloat(texelFetch(P, ib + 1));)
    S(            vec4 a2 = uintBitsToFloat(texelFetch(P, ia + 2)), b2 = uintBitsToFloat(texelFetch(P, ib + 2));)
    S(            vec4 va = vec4(a0.zw, a1.xy), vb = vec4(b0.zw, b1.xy);)
    S(            vec4 wa = vec4(a1.zw, a2.x, 0.0), wb = vec4(b1.zw, b2.x, 0.0);)
    S(            PR vec4 ea = va + (vb - va) * t, eb = wa + (wb - wa) * t;)
    S(            if (x < xl) { xl = x; La = ea; Lb = eb; })
    S(            if (x > xr) { xr = x; Ra = ea; Rb = eb; })
    S(        })
    S(    })
    S(    int x0 = int(ceil(xl - 0.5)), x1 = int(ceil(xr - 0.5));)
    S(    float dx = xr - xl;)
    S(    Sa = dx > 0.0 ? dv4(Ra - La, dx) : vec4(0.0);)
    S(    Sb = dx > 0.0 ? dv4(Rb - Lb, dx) : vec4(0.0);)
    S(    span = ivec2(max(x0, 0), min(x1, uSize.x));)
    S(    if (xl > xr || span.y <= span.x) { gl_Position = vec4(-2.0, -2.0, 0.0, 1.0); return; })
    S(    float px = float((c == 1 || c == 2 || c == 4) ? span.y : span.x);)
    S(    float py = float((c == 2 || c == 4 || c == 5) ? y + 1 : y);)
    S(    gl_Position = vec4(px / float(uSize.x) * 2.0 - 1.0, py / float(uSize.y) * 2.0 - 1.0, 0.0, 1.0);)
    S(});

static const char vs_full[] =
    "#version 330\n"
    S(void main() {)
    S(    vec2 p = vec2(gl_VertexID == 1 ? 3.0 : -1.0, gl_VertexID == 2 ? 3.0 : -1.0);)
    S(    gl_Position = vec4(p, 0.0, 1.0);)
    S(});

/* raster_poly, per fragment */
static const char fs_poly[] =
    PREC_HEADER
    S(uniform usamplerBuffer P;)
    S(uniform usampler2D Tex;)
    S(uniform usampler2D C0;)
    S(uniform usampler2D C1;)
    S(uniform int uSpecial, uWbuf, uTexen, uHighlight, uAtest, uAref, uBlend;)
    S(uniform ivec2 uSize;)
    S(uniform uint uToon[32];)
    S(flat in int k;)
    S(flat in float xl;)
    S(flat in ivec2 span;)
    S(flat in vec4 La, Lb, Sa, Sb;)
    S(layout(location = 0) out uvec4 o0;)
    S(layout(location = 1) out uvec4 o1;)
    S(uvec4 dc; uint dd, dopid, dtrid, dflags;)
    S(void keep() {)  /* write the destination back unchanged but for its flags */
    S(    o0 = dc; o1 = uvec4(dd, dopid | (dtrid << 8) | (dflags << 16), 0u, 0u);)
    S(    gl_FragDepth = float(dd) * (1.0 / 16777216.0);)
    S(})
    S(int c5to6(uint c) { return c != 0u ? int(c) * 2 + 1 : 0; })
    S(int wrapc(int c, int size, bool rep, bool flip) {)
    S(    if (rep) {)
    S(        if (flip && (c & size) != 0) return size - 1 - (c & (size - 1));)
    S(        return c & (size - 1);)
    S(    })
    S(    return clamp(c, 0, size - 1);)
    S(})
    S(void main() {)
    S(    int base = k * 33;)
    S(    uvec4 h0 = texelFetch(P, base);)
    S(    ivec4 h1 = ivec4(texelFetch(P, base + 1));)
    S(    uint attr = h0.y, tex = h0.z;)
    S(    int px = int(gl_FragCoord.x), py = int(gl_FragCoord.y);)
    S(    PR float f = float(px) + 0.5 - xl;)
    S(    PR vec4 ca = La + f * Sa, cb = Lb + f * Sb;)
    S(    int mode = int(attr >> 4) & 3, palpha = int(attr >> 16) & 31, pid = int(attr >> 24) & 63;)
    S(    bool wire = palpha == 0;)
    S(    uint fog = (attr >> 15) & 1u;)
    S(    bool deq = ((attr >> 14) & 1u) != 0u, trans_dw = ((attr >> 11) & 1u) != 0u;)
    S(    int fmt = int(tex >> 26) & 7;)
    S(    bool textured = uTexen != 0 && fmt != 0;)
    S(    if (wire) {)
    S(        palpha = 31;)
    S(        if (!(px == span.x || px == span.y - 1 || py == h1.x || py == h1.y - 1)) discard;)
    S(    })
    S(    float w = dv(1.0, ca.x);)
    S(    PR vec4 cw = vec4(ca.zw, cb.x, 0.0) * w + 0.5;)
    S(    PR vec2 stw = cb.yz * w + 0.01;)
    S(    uint depth;)
    S(    if (uWbuf != 0) depth = w >= 16777215.0 ? 0xffffffu : w <= 0.0 ? 0u : uint(w);)
    S(    else depth = uint(max(ca.y, 0.0));)
    S(    bool pass = true, dirty = false;)
    S(    dc = uvec4(0u); dd = 0u; dopid = 0u; dtrid = 255u; dflags = 0u;)
    S(    if (uSpecial != 0) {)
    S(        ivec2 pc = ivec2(px, py);)
    S(        dc = texelFetch(C0, pc, 0);)
    S(        uvec2 d1 = texelFetch(C1, pc, 0).xy;)
    S(        dd = d1.x; dopid = d1.y & 255u; dtrid = (d1.y >> 8) & 255u; dflags = (d1.y >> 16) & 255u;)
    S(        if (deq) pass = (depth > dd ? depth - dd : dd - depth) <= 0x200u;)
    S(        else pass = depth < dd;)
    S(        if (mode == 3 && pid == 0) {)  /* shadow mask: stencil where the depth test fails */
    S(            if (!pass) { dflags |= 4u; keep(); return; })
    S(            discard;)
    S(        })
    S(        if (!pass) discard;)
    S(        if (mode == 3) {)
    S(            if ((dflags & 4u) == 0u) discard;)
    S(            dflags &= ~4u;)
    S(            dirty = true;)
    S(            if (dopid == uint(pid)) { keep(); return; })
    S(        })
    S(    })
    S(    int r = clamp(int(cw.x), 0, 63);)
    S(    int g = clamp(int(cw.y), 0, 63);)
    S(    int b = clamp(int(cw.z), 0, 63);)
    S(    int r0 = int(cw.x);)
    S(    int a = palpha;)
    S(    if (mode == 2) {)
    S(        if (uHighlight == 0) {)
    S(            uint tc = uToon[r >> 1];)
    S(            r = c5to6(tc & 31u); g = c5to6((tc >> 5) & 31u); b = c5to6((tc >> 10) & 31u);)
    S(        } else { g = r; b = r; })
    S(    })
    S(    if (textured) {)
    S(        int tw = 8 << int((tex >> 20) & 7u), th = 8 << int((tex >> 23) & 7u);)
    S(        int s = int(floor(stw.x)) >> 4, t = int(floor(stw.y)) >> 4;)
    S(        s = wrapc(s, tw, ((tex >> 16) & 1u) != 0u, ((tex >> 18) & 1u) != 0u);)
    S(        t = wrapc(t, th, ((tex >> 17) & 1u) != 0u, ((tex >> 19) & 1u) != 0u);)
    S(        uvec4 tv = texelFetch(Tex, ivec2(int(h0.w & 0xffffu) + s, int(h0.w >> 16) + t), 0);)
    S(        int ta = int(tv.z);)
    S(        if (ta == 0) { if (dirty) { keep(); return; } discard; })
    S(        uint col = tv.x | (tv.y << 8);)
    S(        int tr = c5to6(col & 31u), tg = c5to6((col >> 5) & 31u), tb = c5to6((col >> 10) & 31u);)
    S(        if (mode == 1) {)
    S(            if (ta == 31) { r = tr; g = tg; b = tb; })
    S(            else {)
    S(                r = (tr * ta + r * (31 - ta)) >> 5;)
    S(                g = (tg * ta + g * (31 - ta)) >> 5;)
    S(                b = (tb * ta + b * (31 - ta)) >> 5;)
    S(            })
    S(        } else {)
    S(            r = ((tr + 1) * (r + 1) - 1) >> 6;)
    S(            g = ((tg + 1) * (g + 1) - 1) >> 6;)
    S(            b = ((tb + 1) * (b + 1) - 1) >> 6;)
    S(            a = ((ta + 1) * (a + 1) - 1) >> 5;)
    S(        })
    S(    })
    S(    if (mode == 2 && uHighlight != 0) {)
    S(        uint tc = uToon[(r0 >> 1) & 31];)
    S(        r = min(r + c5to6(tc & 31u), 63); g = min(g + c5to6((tc >> 5) & 31u), 63);)
    S(        b = min(b + c5to6((tc >> 10) & 31u), 63);)
    S(    })
    S(    if (a == 0 || (uAtest != 0 && a <= uAref)) { if (dirty) { keep(); return; } discard; })
    S(    if (a >= 31) {)
    S(        o0 = uvec4(uint(r), uint(g), uint(b), 31u);)
    S(        o1 = uvec4(depth, uint(pid) | (255u << 8) | (((uSpecial != 0 ? dflags & 4u : 0u) | fog | 2u) << 16), 0u, 0u);)
    S(        gl_FragDepth = float(depth) * (1.0 / 16777216.0);)
    S(        return;)
    S(    })
    S(    if (dtrid == uint(pid)) { if (dirty) { keep(); return; } discard; })
    S(    int da = int(dc.w);)
    S(    if (uBlend != 0 && da > 0) {)
    S(        r = (r * (a + 1) + int(dc.x) * (31 - a)) >> 5;)
    S(        g = (g * (a + 1) + int(dc.y) * (31 - a)) >> 5;)
    S(        b = (b * (a + 1) + int(dc.z) * (31 - a)) >> 5;)
    S(        a = max(a, da);)
    S(    })
    S(    if (trans_dw) dd = depth;)
    S(    o0 = uvec4(uint(r), uint(g), uint(b), uint(a));)
    S(    o1 = uvec4(dd, dopid | (uint(pid) << 8) | (((dflags & ~1u) | (dflags & fog)) << 16), 0u, 0u);)
    S(    gl_FragDepth = float(dd) * (1.0 / 16777216.0);)
    S(});

/* the shadow stencil cleared everywhere (a new shadow mask after shadows were drawn) */
static const char fs_shclr[] =
    "#version 330\n"
    S(uniform usampler2D C0;)
    S(uniform usampler2D C1;)
    S(layout(location = 0) out uvec4 o0;)
    S(layout(location = 1) out uvec4 o1;)
    S(void main() {)
    S(    ivec2 p = ivec2(gl_FragCoord.xy);)
    S(    o0 = texelFetch(C0, p, 0);)
    S(    uvec2 a = texelFetch(C1, p, 0).xy;)
    S(    o1 = uvec4(a.x, a.y & ~(4u << 16), 0u, 0u);)
    S(});

/* finish_band: edge marking, fog, 0x80RRGGBB (read back as BGRA bytes) */
static const char fs_finish[] =
    "#version 330\n"
    S(uniform usampler2D C0;)
    S(uniform usampler2D C1;)
    S(uniform int uEdge, uS, uFog, uFogShift, uFogAlphaOnly, uFogOffset;)
    S(uniform ivec2 uSize;)
    S(uniform uint uClrId, uCdepth, uFogColor;)
    S(uniform uint uEdgeCol[8];)
    S(uniform int uFogTab[32];)
    S(out vec4 o;)
    S(int c5to6(uint c) { return c != 0u ? int(c) * 2 + 1 : 0; })
    S(void main() {)
    S(    ivec2 p = ivec2(gl_FragCoord.xy);)
    S(    uvec4 c = texelFetch(C0, p, 0);)
    S(    uvec2 at = texelFetch(C1, p, 0).xy;)
    S(    uint id = at.y & 255u, d = at.x, flags = (at.y >> 16) & 255u;)
    S(    int r = int(c.x), g = int(c.y), b = int(c.z), a = int(c.w);)
    S(    if (uEdge != 0 && (flags & 2u) != 0u) {)
    S(        bool edge = false;)
    S(        for (int i = 0; i < 4 && !edge; i++) {)
    S(            ivec2 q = p + ivec2(i == 0 ? -uS : i == 1 ? uS : 0, i == 2 ? -uS : i == 3 ? uS : 0);)
    S(            uint nid, nd;)
    S(            if (q.x < 0 || q.x >= uSize.x || q.y < 0 || q.y >= uSize.y) { nid = uClrId; nd = uCdepth; })
    S(            else { uvec2 n = texelFetch(C1, q, 0).xy; nid = n.y & 255u; nd = n.x; })
    S(            if (nid != id && d < nd) edge = true;)
    S(        })
    S(        if (edge) {)
    S(            uint e = uEdgeCol[id >> 3];)
    S(            r = c5to6(e & 31u); g = c5to6((e >> 5) & 31u); b = c5to6((e >> 10) & 31u);)
    S(        })
    S(    })
    S(    if (uFog != 0 && (flags & 1u) != 0u) {)
    S(        int step = uFogShift > 10 ? 1 : 0x400 >> uFogShift;)
    S(        int pos = int(d >> 9) - uFogOffset - step, dens;)
    S(        if (pos <= 0) dens = uFogTab[0];)
    S(        else {)
    S(            int kk = pos / step, frac = pos - kk * step;)
    S(            if (kk >= 31) dens = uFogTab[31];)
    S(            else dens = (uFogTab[kk] * (step - frac) + uFogTab[kk + 1] * frac) / step;)
    S(        })
    S(        if (dens >= 127) dens = 128;)
    S(        if (uFogAlphaOnly == 0) {)
    S(            r = (c5to6(uFogColor & 31u) * dens + r * (128 - dens)) >> 7;)
    S(            g = (c5to6((uFogColor >> 5) & 31u) * dens + g * (128 - dens)) >> 7;)
    S(            b = (c5to6((uFogColor >> 10) & 31u) * dens + b * (128 - dens)) >> 7;)
    S(        })
    S(        a = (int((uFogColor >> 16) & 31u) * dens + a * (128 - dens)) >> 7;)
    S(    })
    S(    ivec3 o8 = (ivec3(r, g, b) << 2) | (ivec3(r, g, b) >> 4);)
    S(    o = vec4(vec3(o8), a != 0 ? 128.0 : 0.0) / 255.0;)
    S(});

/* downsample: the native frame, each pixel the average of its S x S block (opaque if at least half of it is) */
static const char fs_down[] =
    "#version 330\n"
    S(uniform sampler2D H;)
    S(uniform int uS;)
    S(uniform ivec2 uOff;)
    S(out vec4 o;)
    S(void main() {)
    S(    ivec2 p = (ivec2(gl_FragCoord.xy) + uOff) * uS;)
    S(    ivec3 sum = ivec3(0); int cnt = 0;)
    S(    for (int y = 0; y < uS; y++))
    S(        for (int x = 0; x < uS; x++) {)
    S(            ivec4 c = ivec4(round(texelFetch(H, p + ivec2(x, y), 0) * 255.0));)
    S(            if (c.w >= 128) { sum += c.xyz; cnt++; })
    S(        })
    S(    if (cnt * 2 >= uS * uS) o = vec4(vec3(sum / cnt), 128.0) / 255.0;)
    S(    else o = vec4(0.0);)
    S(});
#undef S

/* ---- state ---------------------------------------------------------------------------------------------------- */
static int g_ok = -1;         /* context and programs ready (-1 not tried) */
static GLuint p_poly, p_shclr, p_finish, p_down, vao;
static GLuint t_c0, t_c1, t_depth, t_out, t_down, t_atlas, t_pbuf, t_rows, b_poly, b_rows, b_pack;
static GLuint fb_main, fb_out, fb_down;
static int fb_w, fb_h;
static GLint u_special, u_wbuf, u_texen, u_highlight, u_atest, u_aref, u_blend, u_toon, u_psize;
static GLint uf_edge, uf_s, uf_fog, uf_fogshift, uf_fogalpha, uf_fogoff, uf_size, uf_clrid, uf_cdepth, uf_fogcol,
    uf_edgecol, uf_fogtab, ud_s, ud_off;
static u32 g_pdata[MAX_POLY * PSTRIDE * 4];
#define MAX_ROWS (1 << 20)
static u32 g_rows[MAX_ROWS];   /* rows to draw: slot | y << 16 */
static int g_rs[MAX_POLY + 1]; /* each slot's first row */
static u32 cmp_hires[DS_W * 2 * DS_H * 16], cmp_out[DS_W * DS_H];  /* mode 2: the OpenGL result */

static void *glproc(HMODULE lib, PROC (WINAPI *gpa)(LPCSTR), const char *name)
{
    void *p = (void *)gpa(name);
    if (!p || p == (void *)1 || p == (void *)2 || p == (void *)3 || p == (void *)-1)
        p = (void *)GetProcAddress(lib, name);
    return p;
}

static GLuint shader(GLenum type, const char *src, const char *name)
{
    GLuint sh = glCreateShader(type);
    GLint ok = 0;
    glShaderSource(sh, 1, &src, 0);
    glCompileShader(sh);
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        const char *p = src;
        int line = 1;
        glGetShaderInfoLog(sh, sizeof log, 0, log);
        fprintf(stderr, "gpu3d_gl: %s: %s\n", name, log);
        if (getenv("NDOGS_GLPROF"))  /* the source with line numbers */
            for (; *p; line++) {
                const char *e = strchr(p, '\n');
                int len = e ? (int)(e - p) : (int)strlen(p);
                fprintf(stderr, "%4d %.*s\n", line, len, p);
                p += len + (e != 0);
            }
        return 0;
    }
    return sh;
}

static GLuint program(const char *vs, const char *fs, const char *name)
{
    GLuint v = shader(GL_VERTEX_SHADER, vs, name), f = shader(GL_FRAGMENT_SHADER, fs, name), p;
    GLint ok = 0;
    if (!v || !f)
        return 0;
    p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(p, sizeof log, 0, log);
        fprintf(stderr, "gpu3d_gl: %s: %s\n", name, log);
        return 0;
    }
    return p;
}

static GLuint mktex(GLint ifmt, int w, int h, GLenum fmt, GLenum type)
{
    GLuint t;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, ifmt, w, h, 0, fmt, type, 0);
    return t;
}

/* a context of our own on a hidden window (the presenter's context belongs to its thread) */
static int gl_init(void)
{
    HMODULE o = LoadLibraryA("opengl32.dll"), u = LoadLibraryA("user32.dll"), g = LoadLibraryA("gdi32.dll");
    ATOM (WINAPI *RegisterClassA_)(const WNDCLASSA *);
    HWND (WINAPI *CreateWindowExA_)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
    HDC (WINAPI *GetDC_)(HWND);
    int (WINAPI *ChoosePixelFormat_)(HDC, const PIXELFORMATDESCRIPTOR *);
    BOOL (WINAPI *SetPixelFormat_)(HDC, int, const PIXELFORMATDESCRIPTOR *);
    HGLRC (WINAPI *CreateContext_)(HDC);
    BOOL (WINAPI *MakeCurrent_)(HDC, HGLRC);
    PROC (WINAPI *gpa)(LPCSTR);
    WNDCLASSA wc;
    PIXELFORMATDESCRIPTOR pfd;
    HWND hw;
    HDC dc;
    HGLRC rc;
    int fmt, major = 0, minor = 0;
    const char *ver;
    static const GLenum bufs[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT0 + 1 };

    if (!o || !u || !g)
        return 0;
    *(FARPROC *)&RegisterClassA_ = GetProcAddress(u, "RegisterClassA");
    *(FARPROC *)&CreateWindowExA_ = GetProcAddress(u, "CreateWindowExA");
    *(FARPROC *)&GetDC_ = GetProcAddress(u, "GetDC");
    *(FARPROC *)&ChoosePixelFormat_ = GetProcAddress(g, "ChoosePixelFormat");
    *(FARPROC *)&SetPixelFormat_ = GetProcAddress(g, "SetPixelFormat");
    *(FARPROC *)&CreateContext_ = GetProcAddress(o, "wglCreateContext");
    *(FARPROC *)&MakeCurrent_ = GetProcAddress(o, "wglMakeCurrent");
    *(FARPROC *)&gpa = GetProcAddress(o, "wglGetProcAddress");
    if (!RegisterClassA_ || !CreateWindowExA_ || !GetDC_ || !ChoosePixelFormat_ || !SetPixelFormat_ ||
        !CreateContext_ || !MakeCurrent_ || !gpa)
        return 0;
    memset(&wc, 0, sizeof wc);
    wc.style = CS_OWNDC;
    *(FARPROC *)&wc.lpfnWndProc = GetProcAddress(u, "DefWindowProcA");
    wc.hInstance = GetModuleHandleA(0);
    wc.lpszClassName = "ndogs_gl3d";
    RegisterClassA_(&wc);
    hw = CreateWindowExA_(0, "ndogs_gl3d", "", WS_POPUP, 0, 0, 16, 16, 0, 0, wc.hInstance, 0);
    if (!hw || !(dc = GetDC_(hw)))
        return 0;
    memset(&pfd, 0, sizeof pfd);
    pfd.nSize = sizeof pfd;
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    fmt = ChoosePixelFormat_(dc, &pfd);
    if (!fmt || !SetPixelFormat_(dc, fmt, &pfd) || !(rc = CreateContext_(dc)) || !MakeCurrent_(dc, rc))
        return 0;

#define GLFN(ret, name, args) *(void **)&name = glproc(o, gpa, #name);
    GL_FUNCS
#undef GLFN
    if (!glTextureBarrier)
        *(void **)&glTextureBarrier = glproc(o, gpa, "glTextureBarrierNV");
    ver = glGetString ? (const char *)glGetString(GL_VERSION) : 0;
    if (ver)
        sscanf(ver, "%d.%d", &major, &minor);
    if (major * 10 + minor < 33 || !glTextureBarrier || !glTexBuffer || !glClearBufferuiv || !glGenVertexArrays ||
        !glFenceSync || !glMapBufferRange) {
        fprintf(stderr, "gpu3d_gl: OpenGL %s lacks 3.3 or texture barriers; 3D stays on the CPU\n", ver ? ver : "?");
        return 0;
    }
    fprintf(stderr, "gpu3d_gl: 3D on the GPU (%s, OpenGL %s)\n", (const char *)glGetString(GL_RENDERER), ver);

    if (!(p_poly = program(vs_poly, fs_poly, "polygons")) || !(p_shclr = program(vs_full, fs_shclr, "stencil")) ||
        !(p_finish = program(vs_full, fs_finish, "finish")) || !(p_down = program(vs_full, fs_down, "downsample")))
        return 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glDisable(GL_BLEND);
    glDisable(GL_DITHER);
    glDisable(GL_CULL_FACE);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    glGenBuffers(1, &b_pack);
    glGenBuffers(1, &b_rows);
    glBindBuffer(GL_TEXTURE_BUFFER, b_rows);
    glBufferData(GL_TEXTURE_BUFFER, sizeof g_rows, 0, GL_STREAM_DRAW);
    glGenTextures(1, &t_rows);
    glBindTexture(GL_TEXTURE_BUFFER, t_rows);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_R32UI, b_rows);
    glGenBuffers(1, &b_poly);
    glBindBuffer(GL_TEXTURE_BUFFER, b_poly);
    glBufferData(GL_TEXTURE_BUFFER, sizeof g_pdata, 0, GL_STREAM_DRAW);
    glGenTextures(1, &t_pbuf);
    glBindTexture(GL_TEXTURE_BUFFER, t_pbuf);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32UI, b_poly);
    t_atlas = mktex(GL_RGBA8UI, ATLAS, ATLAS, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE);
    t_down = mktex(GL_RGBA8, DS_W, DS_H, GL_RGBA, GL_UNSIGNED_BYTE);
    glGenFramebuffers(1, &fb_main);
    glGenFramebuffers(1, &fb_out);
    glGenFramebuffers(1, &fb_down);
    glBindFramebuffer(GL_FRAMEBUFFER, fb_down);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t_down, 0);
    glDrawBuffers(1, bufs);

    /* texture units: 0 polygon buffer, 1 atlas, 2 colour, 3 attributes, 4 finished frame */
#define U(p, n) glGetUniformLocation(p, n)
    glUseProgram(p_poly);
    glUniform1i(U(p_poly, "P"), 0);
    glUniform1i(U(p_poly, "R"), 5);
    glUniform1i(U(p_poly, "Tex"), 1);
    glUniform1i(U(p_poly, "C0"), 2);
    glUniform1i(U(p_poly, "C1"), 3);
    u_special = U(p_poly, "uSpecial"); u_wbuf = U(p_poly, "uWbuf"); u_texen = U(p_poly, "uTexen");
    u_highlight = U(p_poly, "uHighlight"); u_atest = U(p_poly, "uAtest"); u_aref = U(p_poly, "uAref");
    u_blend = U(p_poly, "uBlend"); u_toon = U(p_poly, "uToon"); u_psize = U(p_poly, "uSize");
    glUseProgram(p_shclr);
    glUniform1i(U(p_shclr, "C0"), 2);
    glUniform1i(U(p_shclr, "C1"), 3);
    glUseProgram(p_finish);
    glUniform1i(U(p_finish, "C0"), 2);
    glUniform1i(U(p_finish, "C1"), 3);
    uf_edge = U(p_finish, "uEdge"); uf_s = U(p_finish, "uS"); uf_fog = U(p_finish, "uFog");
    uf_fogshift = U(p_finish, "uFogShift"); uf_fogalpha = U(p_finish, "uFogAlphaOnly");
    uf_fogoff = U(p_finish, "uFogOffset"); uf_size = U(p_finish, "uSize"); uf_clrid = U(p_finish, "uClrId");
    uf_cdepth = U(p_finish, "uCdepth"); uf_fogcol = U(p_finish, "uFogColor"); uf_edgecol = U(p_finish, "uEdgeCol");
    uf_fogtab = U(p_finish, "uFogTab");
    glUseProgram(p_down);
    glUniform1i(U(p_down, "H"), 4);
    ud_s = U(p_down, "uS"); ud_off = U(p_down, "uOff");
#undef U
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_BUFFER, t_pbuf);
    glActiveTexture(GL_TEXTURE0 + 5);
    glBindTexture(GL_TEXTURE_BUFFER, t_rows);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, t_atlas);
    return glGetError() == 0;
}

/* the rw x rh buffers */
static int fb_size(int w, int h)
{
    static const GLenum bufs[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT0 + 1 };
    if (w == fb_w && h == fb_h)
        return 1;
    if (fb_w) {
        GLuint t[4] = { t_c0, t_c1, t_depth, t_out };
        glDeleteTextures(4, t);
    }
    t_c0 = mktex(GL_RGBA8UI, w, h, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE);
    t_c1 = mktex(GL_RG32UI, w, h, GL_RG_INTEGER, GL_UNSIGNED_INT);
    t_depth = mktex(GL_DEPTH_COMPONENT32F, w, h, GL_DEPTH_COMPONENT, GL_FLOAT);
    t_out = mktex(GL_RGBA8, w, h, GL_RGBA, GL_UNSIGNED_BYTE);
    glBindFramebuffer(GL_FRAMEBUFFER, fb_main);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t_c0, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + 1, GL_TEXTURE_2D, t_c1, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, t_depth, 0);
    glDrawBuffers(2, bufs);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        return 0;
    glBindFramebuffer(GL_FRAMEBUFFER, fb_out);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t_out, 0);
    glDrawBuffers(1, bufs);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        return 0;
    glActiveTexture(GL_TEXTURE0 + 2);
    glBindTexture(GL_TEXTURE_2D, t_c0);
    glActiveTexture(GL_TEXTURE0 + 3);
    glBindTexture(GL_TEXTURE_2D, t_c1);
    glActiveTexture(GL_TEXTURE0 + 4);
    glBindTexture(GL_TEXTURE_2D, t_out);
    glActiveTexture(GL_TEXTURE0 + 1);
    fb_w = w;
    fb_h = h;
    return 1;
}

/* ---- texture cache ---------------------------------------------------------------------------------------------
 * Decoded textures live in the atlas, found by their parameters (address, size, format, colour 0 transparency;
 * palette base unless direct colour) and checked against a hash of their texel and palette bytes once per texture
 * memory refresh. Shelves of equal height; when the atlas is full it starts over. */
#define NCACHE 2048
typedef struct { u32 key, pal, hash; unsigned gen; int x, y, used; } tcache_t;
static tcache_t g_tc[NCACHE];
static int g_ntc;
static struct { int y, h, x; } g_shelf[64];
static int g_nshelf, g_atlas_top;
static u32 g_decode[1024 * 1024];

static u32 hash_bytes(const u8 *m, u32 mask, u32 off, u32 len, u32 h)
{
    u32 i;
    for (i = 0; i < len; i += 4) {
        u32 o = (off + i) & mask;
        h = (h ^ (u32)(m[o] | m[o + 1] << 8 | m[o + 2] << 16 | (u32)m[o + 3] << 24)) * 0x01000193u;
        h ^= h >> 15;
    }
    return h;
}

static u32 tex_hash(const gl3d_job_t *j, u32 tex, u32 pal)
{
    static const int bits[8] = { 0, 8, 2, 4, 8, 2, 8, 16 }, pals[8] = { 0, 64, 8, 32, 512, 0, 16, 0 };
    int fmt = (tex >> 26) & 7, w = 8 << ((tex >> 20) & 7), h = 8 << ((tex >> 23) & 7);
    u32 addr = (tex & 0xffff) << 3, palbase = fmt == 2 ? pal << 3 : pal << 4, hs = 0x811c9dc5u;
    hs = hash_bytes(j->tex_mem, 0x7fffc, addr, (u32)(w * h * bits[fmt] / 8), hs);
    if (fmt == 5) {
        u32 pia = 0x20000 + ((addr & 0x1ffff) >> 1) + (addr >= 0x40000 ? 0x10000 : 0), i, maxp = 0;
        for (i = 0; i < (u32)(w * h / 16); i++) {
            u32 o = (pia + i * 2) & 0x7fffe, pw = (u32)(j->tex_mem[o] | j->tex_mem[o + 1] << 8) & 0x3fff;
            maxp = pw > maxp ? pw : maxp;
        }
        hs = hash_bytes(j->tex_mem, 0x7fffc, pia, (u32)(w * h / 16 * 2 + 3) & ~3u, hs);
        hs = hash_bytes(j->texpal_mem, 0x1fffc, palbase, maxp * 4 + 8, hs);
    } else if (pals[fmt])
        hs = hash_bytes(j->texpal_mem, 0x1fffc, palbase, (u32)pals[fmt], hs);
    return hs;
}

static void atlas_reset(void)
{
    memset(g_tc, 0, sizeof g_tc);
    g_ntc = g_nshelf = g_atlas_top = 0;
}

static int atlas_alloc(int w, int h, int *x, int *y)
{
    int i;
    for (i = 0; i < g_nshelf; i++)
        if (g_shelf[i].h == h && g_shelf[i].x + w <= ATLAS) {
            *x = g_shelf[i].x;
            *y = g_shelf[i].y;
            g_shelf[i].x += w;
            return 1;
        }
    if (g_nshelf >= 64 || g_atlas_top + h > ATLAS)
        return 0;
    g_shelf[g_nshelf].y = g_atlas_top;
    g_shelf[g_nshelf].h = h;
    g_shelf[g_nshelf].x = w;
    g_nshelf++;
    *x = 0;
    *y = g_atlas_top;
    g_atlas_top += h;
    return 1;
}

/* the atlas position of a polygon's texture (x | y << 16), or -1 if the atlas is full */
static int tex_lookup(const gl3d_job_t *j, u32 tex, u32 pal)
{
    int fmt = (tex >> 26) & 7, w = 8 << ((tex >> 20) & 7), h = 8 << ((tex >> 23) & 7);
    u32 key = tex & 0x3ff0ffffu, kp = fmt == 7 ? 0 : pal, slot = (key * 0x9e3779b1u ^ kp * 0x85ebca6bu) % NCACHE, hs;
    tcache_t *e;
    int n;
    for (n = 0; n < NCACHE; n++, slot = (slot + 1) % NCACHE) {
        e = &g_tc[slot];
        if (!e->used || (e->key == key && e->pal == kp))
            break;
    }
    if (n == NCACHE || (!e->used && g_ntc >= NCACHE * 3 / 4))
        return -1;
    if (e->used && e->gen == j->texgen)
        return e->x | e->y << 16;
    hs = tex_hash(j, tex, pal);
    if (!e->used) {
        if (!atlas_alloc(w, h, &e->x, &e->y))
            return -1;
        e->used = 1;
        e->key = key;
        e->pal = kp;
        g_ntc++;
    } else if (e->hash == hs) {
        e->gen = j->texgen;
        return e->x | e->y << 16;
    }
    e->hash = hs;
    e->gen = j->texgen;
    gx_tex_decode(tex, pal, w, h, g_decode);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexSubImage2D(GL_TEXTURE_2D, 0, e->x, e->y, w, h, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, g_decode);
    return e->x | e->y << 16;
}

/* ---- a frame ---------------------------------------------------------------------------------------------------- */
static inline u16 io16(const u8 *io, u32 o) { return (u16)(io[o] | (io[o + 1] << 8)); }
static inline u32 io32(const u8 *io, u32 o) { return io16(io, o) | ((u32)io16(io, o + 2) << 16); }
static inline u32 fbits(float f) { u32 u; memcpy(&u, &f, 4); return u; }

typedef struct { int x0, y0, x1, y1; u8 special, mask, shadow; } pinfo_t;
static pinfo_t g_pi[MAX_POLY];

/* write the polygons' buffer data and rows in drawing order; 0 if a texture didn't fit the atlas, -1 if there are
 * too many rows */
static int pack_polys(const gl3d_job_t *j, int texen)
{
    int i, k, S = j->scale, nr = 0;
    for (i = 0; i < j->n; i++) {
        const poly_t *p = &j->L->poly[j->order[i]];
        u32 *d = g_pdata + (size_t)i * PSTRIDE * 4;
        pinfo_t *pi = &g_pi[i];
        float xmin = 1e30f, xmax = -1e30f, f;
        int fmt = (p->tex >> 26) & 7, at = 0, wy0, wy1, mode = (p->attr >> 4) & 3;
        if (texen && fmt) {
            at = tex_lookup(j, p->tex, p->pal);
            if (at < 0)
                return 0;
        }
        f = p->ymin * (float)S - 0.5f;
        wy0 = (int)f;
        if ((float)wy0 < f) wy0++;
        f = p->ymax * (float)S - 0.5f;
        wy1 = (int)f;
        if ((float)wy1 < f) wy1++;
        d[0] = (u32)p->nv; d[1] = p->attr; d[2] = p->tex; d[3] = (u32)at;
        d[4] = (u32)wy0; d[5] = (u32)wy1; d[6] = d[7] = 0;
        for (k = 0; k < p->nv; k++) {
            const vtx_t *v = &p->v[k];
            float iw = 1.0f / v->f[VW], px = v->f[VX] * (float)S, py = v->f[VY] * (float)S;
            u32 *o = d + 12 + k * 12;
            o[0] = fbits(px); o[1] = fbits(py); o[2] = fbits(iw); o[3] = fbits(v->f[VZ]);
            o[4] = fbits(v->f[VR] * iw); o[5] = fbits(v->f[VG] * iw); o[6] = fbits(v->f[VB] * iw);
            o[7] = fbits(v->f[VS] * iw); o[8] = fbits(v->f[VT] * iw); o[9] = o[10] = o[11] = 0;
            if (px < xmin) xmin = px;
            if (px > xmax) xmax = px;
        }
        pi->x0 = (int)floorf(xmin) - 1;
        pi->x1 = (int)ceilf(xmax) + 1;
        pi->y0 = wy0;
        pi->y1 = wy1;
        if (pi->x0 < 0) pi->x0 = 0;
        if (pi->x1 > j->rw) pi->x1 = j->rw;
        if (pi->y0 < 0) pi->y0 = 0;
        if (pi->y1 > j->rh) pi->y1 = j->rh;
        if (pi->x1 < pi->x0) pi->x1 = pi->x0;
        if (pi->y1 < pi->y0) pi->y1 = pi->y0;
        d[8] = (u32)pi->x0; d[9] = (u32)pi->y0; d[10] = (u32)pi->x1; d[11] = (u32)pi->y1;
        g_rs[i] = nr;
        if (nr + (pi->y1 - pi->y0) > MAX_ROWS)
            return -1;
        for (k = pi->y0; k < pi->y1; k++)
            g_rows[nr++] = (u32)i | (u32)k << 16;
        pi->special = p->translucent || ((p->attr >> 14) & 1);
        pi->mask = mode == 3 && !((p->attr >> 24) & 63);
        pi->shadow = mode == 3 && !pi->mask;
    }
    g_rs[j->n] = nr;
    return 1;
}

static int g_fail, g_draws;
static int g_prof = -1;  /* NDOGS_GLPROF=1: GPU time of each pass */
static GLuint g_q[5];
#define TS(n) do { if (g_prof) glQueryCounter(g_q[n], 0x8E28 /* GL_TIMESTAMP */); } while (0)

/* until the GPU has passed the fence, asleep: polled every 0.2 ms with a high-resolution timer (Windows 10 1803+),
 * else 1 ms sleeps (hw.c raised the timer resolution) */
static void gpu_wait(GLsync fence)
{
    static HANDLE timer;
    static int tried;
    if (!tried) {
        HANDLE (WINAPI *create)(LPSECURITY_ATTRIBUTES, LPCWSTR, DWORD, DWORD);
        *(FARPROC *)&create = GetProcAddress(GetModuleHandleA("kernel32.dll"), "CreateWaitableTimerExW");
        if (create)
            timer = create(0, 0, 2 /* CREATE_WAITABLE_TIMER_HIGH_RESOLUTION */, 0x1F0003 /* TIMER_ALL_ACCESS */);
        tried = 1;
    }
    while (glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, 0) == GL_TIMEOUT_EXPIRED) {
        if (timer) {
            LARGE_INTEGER due;
            due.QuadPart = -2000;  /* 0.2 ms */
            SetWaitableTimer(timer, &due, 0, 0, 0, FALSE);
            WaitForSingleObject(timer, 5);
        } else
            Sleep(1);
    }
}

/* a frame submitted ahead (gl3d_prefetch): its own pixel buffer and fence, collected or dropped later */
static GLuint b_pack2;
static GLsync g_pf_fence;
static int g_pf_rw, g_pf_rh;

static int render(const gl3d_job_t *j, u32 *hires, u32 *out, int ahead)
{
    const u8 *io = j->io;
    u32 disp3dcnt = io16(io, 0x60), clr = io32(io, 0x350), cdepth = (io16(io, 0x354) & 0x7fff) * 0x200 + 0x1ff;
    int texen = disp3dcnt & 1, i, nb, shadow_drawn = 0, drawn = 0, pk;
    GLuint cc[4], ca[4];
    GLfloat cd;
    u32 toon[32], ecol[8];
    GLint ftab[32];
    g_draws = 0;
    if (g_prof < 0) {
        g_prof = getenv("NDOGS_GLPROF") && glQueryCounter;
        if (g_prof)
            glGenQueries(5, g_q);
    }

    double t0 = now_ms(), t1, t2, t3;
    if (!fb_size(j->rw, j->rh))
        return 0;
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, t_atlas);
    pk = pack_polys(j, texen);
    if (!pk) {
        atlas_reset();
        pk = pack_polys(j, texen);
    }
    if (pk <= 0)
        return -1;  /* this frame on the CPU */
    glBindBuffer(GL_TEXTURE_BUFFER, b_poly);
    glBufferData(GL_TEXTURE_BUFFER, sizeof g_pdata, 0, GL_STREAM_DRAW);  /* orphan: no wait for the last frame */
    if (j->n)
        glBufferSubData(GL_TEXTURE_BUFFER, 0, (GLsizeiptr)j->n * PSTRIDE * 16, g_pdata);
    glBindBuffer(GL_TEXTURE_BUFFER, b_rows);
    glBufferData(GL_TEXTURE_BUFFER, sizeof g_rows, 0, GL_STREAM_DRAW);
    if (g_rs[j->n])
        glBufferSubData(GL_TEXTURE_BUFFER, 0, (GLsizeiptr)g_rs[j->n] * 4, g_rows);

    t1 = now_ms();
    TS(0);
    /* rear plane */
    glBindFramebuffer(GL_FRAMEBUFFER, fb_main);
    glViewport(0, 0, j->rw, j->rh);
    cc[0] = clr & 31 ? (clr & 31) * 2 + 1 : 0;
    cc[1] = (clr >> 5) & 31 ? ((clr >> 5) & 31) * 2 + 1 : 0;
    cc[2] = (clr >> 10) & 31 ? ((clr >> 10) & 31) * 2 + 1 : 0;
    cc[3] = (clr >> 16) & 31;
    ca[0] = cdepth;
    ca[1] = ((clr >> 24) & 63) | 0xff00u | ((clr >> 15) & 1) << 16;
    ca[2] = ca[3] = 0;
    cd = (float)cdepth * (1.0f / 16777216.0f);
    glDepthMask(1);
    glClearBufferuiv(GL_COLOR, 0, cc);
    glClearBufferuiv(GL_COLOR, 1, ca);
    glClearBufferfv(GL_DEPTH, 0, &cd);

    TS(1);
    /* polygons */
    glUseProgram(p_poly);
    glUniform2i(u_psize, j->rw, j->rh);
    glUniform1i(u_wbuf, (j->L->swapflags >> 1) & 1);
    glUniform1i(u_texen, texen);
    glUniform1i(u_highlight, (disp3dcnt >> 1) & 1);
    glUniform1i(u_atest, (disp3dcnt >> 2) & 1);
    glUniform1i(u_blend, (disp3dcnt >> 3) & 1);
    glUniform1i(u_aref, io[0x340] & 31);
    for (i = 0; i < 32; i++)
        toon[i] = io16(io, 0x380 + i * 2);
    glUniform1uiv(u_toon, 32, toon);
    glEnable(GL_DEPTH_TEST);
    for (i = 0; i < j->n; i = nb) {
        const pinfo_t *pi = &g_pi[i];
        if (!pi->special) {  /* a run of ordinary opaque polygons: one draw, hardware depth test */
            for (nb = i + 1; nb < j->n && !g_pi[nb].special; nb++)
                ;
            glUniform1i(u_special, 0);
            glDepthFunc(GL_LESS);
            glDrawArrays(GL_TRIANGLES, g_rs[i] * 6, (g_rs[nb] - g_rs[i]) * 6);
            g_draws++;
            drawn = 1;
            continue;
        }
        if (pi->mask && shadow_drawn) {  /* raster_poly: a new mask clears the shadow stencil */
            glTextureBarrier();
            glUseProgram(p_shclr);
            glDepthFunc(GL_ALWAYS);
            glDepthMask(0);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glDepthMask(1);
            glUseProgram(p_poly);
            shadow_drawn = 0;
        }
        /* polygons reading the buffers: as many in one draw as don't overlap each other */
        {
            int ux0 = pi->x0, uy0 = pi->y0, ux1 = pi->x1, uy1 = pi->y1;
            if (pi->shadow)
                shadow_drawn = 1;
            for (nb = i + 1; nb < j->n; nb++) {
                const pinfo_t *q = &g_pi[nb];
                if (!q->special || (q->mask && shadow_drawn) ||
                    (q->x0 < ux1 && ux0 < q->x1 && q->y0 < uy1 && uy0 < q->y1))
                    break;
                if (q->shadow)
                    shadow_drawn = 1;
                if (q->x0 < ux0) ux0 = q->x0;
                if (q->y0 < uy0) uy0 = q->y0;
                if (q->x1 > ux1) ux1 = q->x1;
                if (q->y1 > uy1) uy1 = q->y1;
            }
        }
        if (drawn)
            glTextureBarrier();
        glUniform1i(u_special, 1);
        glDepthFunc(GL_ALWAYS);
        glDrawArrays(GL_TRIANGLES, g_rs[i] * 6, (g_rs[nb] - g_rs[i]) * 6);
        g_draws++;
        drawn = 1;
    }
    glDisable(GL_DEPTH_TEST);
    glTextureBarrier();

    TS(2);
    /* edge marking, fog, output */
    glBindFramebuffer(GL_FRAMEBUFFER, fb_out);
    glUseProgram(p_finish);
    glUniform1i(uf_edge, (disp3dcnt >> 5) & 1);
    glUniform1i(uf_s, j->scale);
    glUniform2i(uf_size, j->rw, j->rh);
    glUniform1ui(uf_clrid, (clr >> 24) & 63);
    glUniform1ui(uf_cdepth, cdepth);
    for (i = 0; i < 8; i++)
        ecol[i] = io16(io, 0x330 + i * 2);
    glUniform1uiv(uf_edgecol, 8, ecol);
    glUniform1i(uf_fog, (disp3dcnt >> 7) & 1);
    glUniform1i(uf_fogshift, (disp3dcnt >> 8) & 15);
    glUniform1i(uf_fogalpha, (disp3dcnt >> 6) & 1);
    glUniform1i(uf_fogoff, io16(io, 0x35c) & 0x7fff);
    glUniform1ui(uf_fogcol, io32(io, 0x358));
    for (i = 0; i < 32; i++)
        ftab[i] = io[0x360 + i] & 127;
    glUniform1iv(uf_fogtab, 32, ftab);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    t2 = now_ms();
    /* read back through a pixel buffer behind a fence, waited for asleep (a plain glReadPixels spins the CPU until
     * the GPU is done) */
    {
        size_t hsize = (size_t)j->rw * j->rh * 4;
        GLsync fence;
        const u8 *m;
        glBindBuffer(GL_PIXEL_PACK_BUFFER, ahead ? b_pack2 : b_pack);
        glBufferData(GL_PIXEL_PACK_BUFFER, (GLsizeiptr)(hsize + DS_W * DS_H * 4), 0, GL_STREAM_READ);
        glPixelStorei(GL_PACK_ROW_LENGTH, 0);
        glReadPixels(0, 0, j->rw, j->rh, GL_BGRA, GL_UNSIGNED_BYTE, (void *)0);
        TS(3);

        /* the native frame */
        glBindFramebuffer(GL_FRAMEBUFFER, fb_down);
        glViewport(0, 0, DS_W, DS_H);
        glUseProgram(p_down);
        glUniform1i(ud_s, j->scale);
        glUniform2i(ud_off, j->xoff, j->yoff);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glReadPixels(0, 0, DS_W, DS_H, GL_BGRA, GL_UNSIGNED_BYTE, (void *)hsize);
        TS(4);
        fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        glFlush();
        if (ahead) {
            g_pf_fence = fence;
            g_pf_rw = j->rw;
            g_pf_rh = j->rh;
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
            return glGetError() ? 0 : 1;
        }
        gpu_wait(fence);
        glDeleteSync(fence);
        if (g_prof) {
            GLuint64 v[5];
            for (i = 0; i < 5; i++)
                glGetQueryObjectui64v(g_q[i], 0x8866 /* GL_QUERY_RESULT */, &v[i]);
            fprintf(stderr, "glprof: clear %.3f polys %.3f finish+read %.3f down+read %.3f ms\n", (v[1] - v[0]) / 1e6,
                    (v[2] - v[1]) / 1e6, (v[3] - v[2]) / 1e6, (v[4] - v[3]) / 1e6);
        }
        t3 = now_ms();
        m = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, (GLsizeiptr)(hsize + DS_W * DS_H * 4), GL_MAP_READ_BIT);
        if (m) {
            memcpy(hires, m, hsize);
            memcpy(out, m + hsize, DS_W * DS_H * 4);
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        if (!m)
            return 0;
    }
    if (getenv("NDOGS_VLOG"))
        fprintf(stderr, "gl3d: pack %.2fms draw %.2fms wait %.2fms copy %.2fms (%d draws)\n", t1 - t0, t2 - t1, t3 - t2,
                now_ms() - t3, g_draws);
    if (glGetError()) {
        if (++g_fail > 10)
            return 0;
    }
    return 1;
}

int gl3d_render(const gl3d_job_t *j)
{
    int r = 0;
    if (g_ok < 0) {
        g_ok = gl_init();
        if (!g_ok)
            fprintf(stderr, "gpu3d_gl: OpenGL renderer unavailable, using the CPU rasteriser\n");
    }
    if (g_ok && !(g_mode == 2 && j->rw * j->rh > (int)(sizeof cmp_hires / 4)))
        r = render(j, g_mode == 2 ? cmp_hires : j->hires, g_mode == 2 ? cmp_out : j->out, 0);
    if (r < 0)
        return 0;  /* only this frame on the CPU */
    if (!r) {
        if (g_ok)
            fprintf(stderr, "gpu3d_gl: rendering failed, back to the CPU rasteriser\n");
        g_ok = 0;
        g_mode = 0;
        return 0;
    }
    return 1;
}

/* Frames submitted ahead: the GPU draws while the game goes on (it runs at low clocks under this light load, so
 * waiting for each frame cost 2-3 ms); gl3d_collect fetches the result when it is due, gl3d_drop forgets it. */
void gl3d_drop(void)
{
    if (g_pf_fence) {
        glDeleteSync(g_pf_fence);
        g_pf_fence = 0;
    }
}

int gl3d_prefetch(const gl3d_job_t *j)
{
    int r;
    if (g_ok <= 0 || g_mode != 1)
        return 0;
    gl3d_drop();
    if (!b_pack2)
        glGenBuffers(1, &b_pack2);
    r = render(j, 0, 0, 1);
    if (r <= 0) {
        gl3d_drop();
        return 0;
    }
    return 1;
}

int gl3d_collect(int rw, int rh, u32 *hires, u32 *out)
{
    size_t hsize = (size_t)rw * rh * 4;
    const u8 *m;
    double t0 = now_ms();
    if (!g_pf_fence || rw != g_pf_rw || rh != g_pf_rh) {
        gl3d_drop();
        return 0;
    }
    double tw, tm;
    int ready = glClientWaitSync(g_pf_fence, 0, 0) != GL_TIMEOUT_EXPIRED;
    gpu_wait(g_pf_fence);
    gl3d_drop();
    tw = now_ms();
    glBindBuffer(GL_PIXEL_PACK_BUFFER, b_pack2);
    m = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, (GLsizeiptr)(hsize + DS_W * DS_H * 4), GL_MAP_READ_BIT);
    tm = now_ms();
    if (m) {
        memcpy(hires, m, hsize);
        memcpy(out, m + hsize, DS_W * DS_H * 4);
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    if (getenv("NDOGS_VLOG"))
        fprintf(stderr, "gl3d: collected ahead, %.2fms (ready %d, wait %.2f map %.2f copy %.2f)\n", now_ms() - t0, ready,
                tw - t0, tm - tw, now_ms() - tm);
    return m != 0;
}

/* ---- comparison with the CPU rasteriser (NDOGS_GPU3D=2) -------------------------------------------------------- */
static void save_bmp(const char *name, const u32 *px, int w, int h)
{
    FILE *f = fopen(name, "wb");
    u8 hd[54];
    int y, size = w * h * 4;
    if (!f)
        return;
    memset(hd, 0, sizeof hd);
    hd[0] = 'B'; hd[1] = 'M';
    *(u32 *)(hd + 2) = (u32)(54 + size);
    *(u32 *)(hd + 10) = 54;
    *(u32 *)(hd + 14) = 40;
    *(s32 *)(hd + 18) = w;
    *(s32 *)(hd + 22) = -h;
    *(u16 *)(hd + 26) = 1;
    *(u16 *)(hd + 28) = 32;
    *(u32 *)(hd + 34) = (u32)size;
    fwrite(hd, 1, 54, f);
    for (y = 0; y < h; y++)
        fwrite(px + (size_t)y * w, 4, (size_t)w, f);
    fclose(f);
}

void gl3d_compare(const gl3d_job_t *j)
{
    static gl3d_job_t pending;
    static int have, renders;
    static double sum_px;
    static u32 worst, frames_diff, frames, native_diff;
    int i, n, diff = 0, alpha = 0, maxd = 0, ndiff = 0;
    if (j) {
        pending = *j;
        have = 1;
        return;
    }
    if (!have)
        return;
    have = 0;
    n = pending.rw * pending.rh;
    for (i = 0; i < n; i++) {
        u32 a = pending.hires[i], b = cmp_hires[i];
        if (a != b) {
            int k, d = 0;
            diff++;
            if ((a ^ b) >> 31)
                alpha++;
            for (k = 0; k < 24; k += 8) {
                int e = abs((int)((a >> k) & 255) - (int)((b >> k) & 255));
                d = e > d ? e : d;
            }
            maxd = d > maxd ? d : maxd;
        }
    }
    for (i = 0; i < DS_W * DS_H; i++)  /* the native frame */
        ndiff += pending.out[i] != cmp_out[i];
    renders++;
    frames++;
    if (diff)
        frames_diff++;
    native_diff += ndiff;
    sum_px += diff;
    if ((u32)diff > worst)
        worst = (u32)diff;
    if (getenv("NDOGS_GLCMP") && (diff || ndiff))
        fprintf(stderr, "glcmp: render %d (%dx%d): %d pixels differ (%.3f%%), %d in coverage, max channel %d; native %d\n",
                renders, pending.rw, pending.rh, diff, 100.0 * diff / n, alpha, maxd, ndiff);
    {   /* NDOGS_GLDUMP=<render>,...: save both and the difference; NDOGS_GLDUMPMIN=<n>: the first 4 with over n */
        const char *e = getenv("NDOGS_GLDUMP"), *m = getenv("NDOGS_GLDUMPMIN");
        static int dumped;
        int dump = m && diff > atoi(m) && dumped < 4;
        while (e && *e && !dump) {
            dump = atoi(e) == renders;
            while (*e && *e != ',') e++;
            if (*e) e++;
        }
        if (dump) {
            static u32 dimg[DS_W * 2 * DS_H * 16];
            char name[64];
            dumped++;
            for (i = 0; i < n; i++)
                dimg[i] = pending.hires[i] != cmp_hires[i] ? 0xffff00ffu : (pending.hires[i] >> 2) & 0x3f3f3f;
            sprintf(name, "gl_cpu_%d.bmp", renders);
            save_bmp(name, pending.hires, pending.rw, pending.rh);
            sprintf(name, "gl_gpu_%d.bmp", renders);
            save_bmp(name, cmp_hires, pending.rw, pending.rh);
            sprintf(name, "gl_diff_%d.bmp", renders);
            save_bmp(name, dimg, pending.rw, pending.rh);
        }
    }
    if (frames == 300) {
        fprintf(stderr, "glcmp: last 300 renders: %u differ, %.1f pixels on average (%.4f%%), worst %u; native frame: %u "
                "pixels in all\n", frames_diff, sum_px / frames, 100.0 * sum_px / frames / n, worst, native_diff);
        frames = frames_diff = worst = native_diff = 0;
        sum_px = 0;
    }
}

#else
int gl3d_render(const gl3d_job_t *j) { (void)j; g_mode = 0; return 0; }
int gl3d_prefetch(const gl3d_job_t *j) { (void)j; return 0; }
int gl3d_collect(int rw, int rh, u32 *hires, u32 *out) { (void)rw; (void)rh; (void)hires; (void)out; return 0; }
void gl3d_drop(void) {}
void gl3d_compare(const gl3d_job_t *j) { (void)j; }
#endif
