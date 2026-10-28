/* The window: both DS screens, stacked or side by side (F9), scaled to the window keeping the aspect ratio, stretched
 * to fill it, or at the largest whole multiple (F10); F8 cycles the internal resolution (1-4x, NDOGS_SCALE, default
 * 2). The mouse on the bottom screen is the stylus. Keyboard: arrows = D-pad, Z = A, X = B, A = Y, S = X, Q = L,
 * W = R, Enter = Start, Shift (or Backspace) = Select, F11 = full screen (Windows). Gamepads: XInput on Windows
 * (A/B/X/Y in the Nintendo layout, D-pad or left stick, shoulders = L/R, Start/Back = Start/Select).
 * Windows: Win32 GDI. Linux: X11 through dlopen (no build dependency; headless if there's no display).
 * The UI libraries are loaded at run time, after the DS memory map is reserved: loaded at startup they allocate
 * memory inside the DS address range. */
#include <stdlib.h>
#include <string.h>
#include "ds_video.h"

#define KEY_A 0x001
#define KEY_B 0x002
#define KEY_SELECT 0x004
#define KEY_START 0x008
#define KEY_RIGHT 0x010
#define KEY_LEFT 0x020
#define KEY_UP 0x040
#define KEY_DOWN 0x080
#define KEY_R 0x100
#define KEY_L 0x200
#define KEY_X 0x400
#define KEY_Y 0x800

#define MAXS 4
static u32 pixels[DS_W * DS_H * 2 * MAXS * MAXS];  /* the canvas: both screens in the current layout */
static int can_w = DS_W, can_h = DS_H * 2;        /* canvas size */
static int scr_w = DS_W, scr_h = DS_H;            /* one screen in the canvas */
static int closed;
static int mouse_down, mouse_x, mouse_y;  /* in DS bottom-screen pixels */
static u32 keys_down;                     /* pressed keyboard keys as KEY_ bits */
static u32 pad_down;                      /* pressed gamepad buttons as KEY_ bits */
static int win_w, win_h;                  /* client size */
static int layout;                        /* 0 stacked, 1 side by side */
static int aspect;                        /* 0 keep, 1 stretch, 2 whole multiples */
static int scale_req = -1;

int window_scale(void)
{
    if (scale_req < 0) {
        /* starting layout and aspect mode: NDOGS_LAYOUT=0 stacked / 1 side by side,
         * NDOGS_ASPECT=0 keep / 1 stretch / 2 whole multiples */
        layout = getenv("NDOGS_LAYOUT") ? atoi(getenv("NDOGS_LAYOUT")) & 1 : 0;
        aspect = getenv("NDOGS_ASPECT") ? atoi(getenv("NDOGS_ASPECT")) % 3 : 0;
        scale_req = getenv("NDOGS_SCALE") ? atoi(getenv("NDOGS_SCALE")) : 2;
        scale_req = scale_req < 1 ? 1 : scale_req > MAXS ? MAXS : scale_req;
    }
    return scale_req;
}

/* function keys shared by the platforms: F8 resolution, F9 layout, F10 aspect */
static void fkey(int n)
{
    if (n == 8)
        scale_req = window_scale() % MAXS + 1;
    else if (n == 9)
        layout ^= 1;
    else if (n == 10)
        aspect = (aspect + 1) % 3;
}

/* where the canvas sits in the client area */
static void fit(int *x, int *y, int *w, int *h)
{
    int lw = layout ? DS_W * 2 : DS_W, lh = layout ? DS_H : DS_H * 2;  /* canvas shape in DS pixels */
    if (aspect == 1) {
        *w = win_w;
        *h = win_h;
    } else if (aspect == 2 && win_w >= lw && win_h >= lh) {
        int k = win_w / lw < win_h / lh ? win_w / lw : win_h / lh;
        *w = lw * k;
        *h = lh * k;
    } else if (win_w * lh > win_h * lw) {
        *h = win_h;
        *w = win_h * lw / lh;
    } else {
        *w = win_w;
        *h = win_w * lh / lw;
    }
    *x = (win_w - *w) / 2;
    *y = (win_h - *h) / 2;
}

static void mouse_at(int cx, int cy)
{
    int x, y, w, h;
    fit(&x, &y, &w, &h);
    if (w <= 0 || h <= 0)
        return;
    if (layout) {  /* bottom screen on the right */
        mouse_x = (cx - x) * DS_W * 2 / w - DS_W;
        mouse_y = (cy - y) * DS_H / h;
    } else {
        mouse_x = (cx - x) * DS_W / w;
        mouse_y = (cy - y) * DS_H * 2 / h - DS_H;
    }
}

/* both screens (w x h each) into the canvas */
static void build_canvas(const u32 *top, const u32 *bottom, int w, int h)
{
    int y;
    scr_w = w;
    scr_h = h;
    can_w = layout ? w * 2 : w;
    can_h = layout ? h : h * 2;
    if (!layout) {
        memcpy(pixels, top, (size_t)w * h * 4);
        memcpy(pixels + w * h, bottom, (size_t)w * h * 4);
        return;
    }
    for (y = 0; y < h; y++) {
        memcpy(pixels + y * can_w, top + y * w, (size_t)w * 4);
        memcpy(pixels + y * can_w + w, bottom + y * w, (size_t)w * 4);
    }
}

int window_touch(int *x, int *y)
{
    if (!mouse_down || mouse_y < 0 || mouse_y >= DS_H || mouse_x < 0 || mouse_x >= DS_W)
        return 0;
    *x = mouse_x;
    *y = mouse_y;
    return 1;
}

u32 window_keys(void) { return 0x0fff & ~(keys_down | pad_down); }

#ifdef _WIN32
#include <windows.h>

static struct {
    ATOM (WINAPI *RegisterClassA)(const WNDCLASSA *);
    HCURSOR (WINAPI *LoadCursorA)(HINSTANCE, LPCSTR);
    BOOL (WINAPI *AdjustWindowRect)(LPRECT, DWORD, BOOL);
    HWND (WINAPI *CreateWindowExA)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
    BOOL (WINAPI *ShowWindow)(HWND, int);
    BOOL (WINAPI *InvalidateRect)(HWND, const RECT *, BOOL);
    BOOL (WINAPI *PeekMessageA)(LPMSG, HWND, UINT, UINT, UINT);
    BOOL (WINAPI *TranslateMessage)(const MSG *);
    LRESULT (WINAPI *DispatchMessageA)(const MSG *);
    LRESULT (WINAPI *DefWindowProcA)(HWND, UINT, WPARAM, LPARAM);
    void (WINAPI *PostQuitMessage)(int);
    BOOL (WINAPI *DestroyWindow)(HWND);
    HWND (WINAPI *SetCapture)(HWND);
    BOOL (WINAPI *ReleaseCapture)(void);
    HDC (WINAPI *BeginPaint)(HWND, LPPAINTSTRUCT);
    BOOL (WINAPI *EndPaint)(HWND, const PAINTSTRUCT *);
    LONG (WINAPI *SetWindowLongA)(HWND, int, LONG);
    BOOL (WINAPI *SetWindowPos)(HWND, HWND, int, int, int, int, UINT);
    BOOL (WINAPI *GetWindowRect)(HWND, LPRECT);
    int (WINAPI *GetSystemMetrics)(int);
    int (WINAPI *FillRect)(HDC, const RECT *, HBRUSH);
    int (WINAPI *StretchDIBits)(HDC, int, int, int, int, int, int, int, int, const void *, const BITMAPINFO *, UINT,
                                DWORD);
    int (WINAPI *SetStretchBltMode)(HDC, int);
    HGDIOBJ (WINAPI *GetStockObject)(int);
    DWORD (WINAPI *XInputGetState)(DWORD, void *);
} W;

static int load_ui(void)
{
    HMODULE u = LoadLibraryA("user32.dll"), g = LoadLibraryA("gdi32.dll"), x = LoadLibraryA("xinput1_4.dll");
    if (!x)
        x = LoadLibraryA("xinput9_1_0.dll");
    if (!u || !g)
        return 0;
#define GET(lib, name) (*(FARPROC *)&W.name = GetProcAddress(lib, #name))
    GET(u, RegisterClassA); GET(u, LoadCursorA); GET(u, AdjustWindowRect); GET(u, CreateWindowExA);
    GET(u, ShowWindow); GET(u, InvalidateRect); GET(u, PeekMessageA); GET(u, TranslateMessage);
    GET(u, DispatchMessageA); GET(u, DefWindowProcA); GET(u, PostQuitMessage); GET(u, DestroyWindow);
    GET(u, SetCapture); GET(u, ReleaseCapture); GET(u, BeginPaint); GET(u, EndPaint); GET(u, SetWindowLongA);
    GET(u, SetWindowPos); GET(u, GetWindowRect); GET(u, GetSystemMetrics); GET(u, FillRect);
    GET(g, StretchDIBits); GET(g, SetStretchBltMode); GET(g, GetStockObject);
    if (x)
        GET(x, XInputGetState);
#undef GET
    return W.CreateWindowExA && W.StretchDIBits;
}

static HWND wnd;
static int fullscreen;
static RECT saved_rect;
#define WSTYLE (WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX)

static u32 key_bit(WPARAM vk)
{
    switch (vk) {
    case 'Z': return KEY_A;
    case 'X': return KEY_B;
    case VK_SHIFT: case VK_RSHIFT: case VK_BACK: return KEY_SELECT;
    case VK_RETURN: return KEY_START;
    case VK_RIGHT: return KEY_RIGHT;
    case VK_LEFT: return KEY_LEFT;
    case VK_UP: return KEY_UP;
    case VK_DOWN: return KEY_DOWN;
    case 'W': return KEY_R;
    case 'Q': return KEY_L;
    case 'S': return KEY_X;
    case 'A': return KEY_Y;
    default: return 0;
    }
}

static void toggle_fullscreen(void)
{
    fullscreen = !fullscreen;
    if (fullscreen) {
        W.GetWindowRect(wnd, &saved_rect);
        W.SetWindowLongA(wnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        W.SetWindowPos(wnd, HWND_TOP, 0, 0, W.GetSystemMetrics(SM_CXSCREEN), W.GetSystemMetrics(SM_CYSCREEN),
                       SWP_FRAMECHANGED);
    } else {
        W.SetWindowLongA(wnd, GWL_STYLE, WSTYLE | WS_VISIBLE);
        W.SetWindowPos(wnd, HWND_TOP, saved_rect.left, saved_rect.top, saved_rect.right - saved_rect.left,
                       saved_rect.bottom - saved_rect.top, SWP_FRAMECHANGED);
    }
}

static LRESULT CALLBACK wndproc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CLOSE:
        closed = 1;
        W.DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        W.PostQuitMessage(0);
        return 0;
    case WM_SIZE:
        win_w = LOWORD(lp);
        win_h = HIWORD(lp);
        W.InvalidateRect(h, 0, TRUE);
        return 0;
    case WM_LBUTTONDOWN:
        W.SetCapture(h);
        mouse_down = 1;
        /* fall through */
    case WM_MOUSEMOVE:
        mouse_at((short)LOWORD(lp), (short)HIWORD(lp));
        return 0;
    case WM_LBUTTONUP:
        W.ReleaseCapture();
        mouse_down = 0;
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_F11 && !(lp & 0x40000000))
            toggle_fullscreen();
        if (wp >= VK_F8 && wp <= VK_F10 && !(lp & 0x40000000)) {
            fkey((int)(wp - VK_F1) + 1);
            W.InvalidateRect(h, 0, TRUE);
        }
        keys_down |= key_bit(wp);
        return 0;
    case WM_KEYUP:
        keys_down &= ~key_bit(wp);
        return 0;
    case WM_KILLFOCUS:
        keys_down = 0;
        mouse_down = 0;
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = W.BeginPaint(h, &ps);
        BITMAPINFO bi;
        RECT r;
        int x, y, w, hh;
        fit(&x, &y, &w, &hh);
        r.left = 0; r.top = 0; r.right = win_w; r.bottom = y;
        W.FillRect(dc, &r, (HBRUSH)W.GetStockObject(BLACK_BRUSH));
        r.top = y + hh; r.bottom = win_h;
        W.FillRect(dc, &r, (HBRUSH)W.GetStockObject(BLACK_BRUSH));
        r.top = 0; r.right = x;
        W.FillRect(dc, &r, (HBRUSH)W.GetStockObject(BLACK_BRUSH));
        r.left = x + w; r.right = win_w;
        W.FillRect(dc, &r, (HBRUSH)W.GetStockObject(BLACK_BRUSH));
        memset(&bi, 0, sizeof bi);
        bi.bmiHeader.biSize = sizeof bi.bmiHeader;
        bi.bmiHeader.biWidth = can_w;
        bi.bmiHeader.biHeight = -can_h;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        W.SetStretchBltMode(dc, COLORONCOLOR);
        W.StretchDIBits(dc, x, y, w, hh, 0, 0, can_w, can_h, pixels, &bi, DIB_RGB_COLORS, SRCCOPY);
        W.EndPaint(h, &ps);
        return 0;
    }
    }
    return W.DefWindowProcA(h, msg, wp, lp);
}

/* XInput gamepad 0 (XINPUT_STATE: packet number, then buttons u16, triggers, sticks) */
static void poll_pad(void)
{
    struct { DWORD packet; WORD buttons; BYTE lt, rt; SHORT lx, ly, rx, ry; } st;
    u32 k = 0;
    if (!W.XInputGetState || W.XInputGetState(0, &st) != 0) {
        pad_down = 0;
        return;
    }
    if (st.buttons & 0x0001 || st.ly > 16000) k |= KEY_UP;
    if (st.buttons & 0x0002 || st.ly < -16000) k |= KEY_DOWN;
    if (st.buttons & 0x0004 || st.lx < -16000) k |= KEY_LEFT;
    if (st.buttons & 0x0008 || st.lx > 16000) k |= KEY_RIGHT;
    if (st.buttons & 0x0010) k |= KEY_START;
    if (st.buttons & 0x0020) k |= KEY_SELECT;
    if (st.buttons & 0x0100 || st.lt > 64) k |= KEY_L;
    if (st.buttons & 0x0200 || st.rt > 64) k |= KEY_R;
    if (st.buttons & 0x1000) k |= KEY_B;  /* pad A (bottom) = DS B (bottom), Nintendo layout */
    if (st.buttons & 0x2000) k |= KEY_A;
    if (st.buttons & 0x4000) k |= KEY_Y;
    if (st.buttons & 0x8000) k |= KEY_X;
    pad_down = k;
}

int window_open(int s)
{
    WNDCLASSA wc;
    RECT r;
    int scale = s > 0 ? s : 2;
    window_scale();
    if (!load_ui())
        return 0;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandleA(0);
    wc.hCursor = W.LoadCursorA(0, (LPCSTR)IDC_ARROW);
    wc.lpszClassName = "nintendogs_pc";
    W.RegisterClassA(&wc);
    r.left = 0;
    r.top = 0;
    r.right = DS_W * scale * (layout ? 2 : 1);
    r.bottom = DS_H * scale * (layout ? 1 : 2);
    win_w = r.right;
    win_h = r.bottom;
    W.AdjustWindowRect(&r, WSTYLE, FALSE);
    wnd = W.CreateWindowExA(0, "nintendogs_pc", "Nintendogs", WSTYLE, CW_USEDEFAULT, CW_USEDEFAULT,
                            r.right - r.left, r.bottom - r.top, 0, 0, wc.hInstance, 0);
    if (!wnd)
        return 0;
    W.ShowWindow(wnd, SW_SHOW);
    return 1;
}

int window_present(const u32 *top, const u32 *bottom, int w, int h)
{
    MSG m;
    if (!wnd)
        return 1;
    build_canvas(top, bottom, w, h);
    W.InvalidateRect(wnd, 0, FALSE);
    while (W.PeekMessageA(&m, 0, 0, 0, PM_REMOVE)) {
        W.TranslateMessage(&m);
        W.DispatchMessageA(&m);
    }
    poll_pad();
    return !closed;
}

#else
/* ---- Linux: X11 via dlopen ------------------------------------------------------------------------------------ */
#include <dlfcn.h>
#include <stdio.h>

/* the few Xlib types and constants used here (layouts as in Xlib.h, 32-bit) */
typedef unsigned long XID;
typedef struct { int type; unsigned long serial; int send_event; void *display; XID window; int pad[24]; } XAnyEvent;
typedef struct {
    int type; unsigned long serial; int send_event; void *display; XID window, root, subwindow;
    unsigned long time; int x, y, x_root, y_root; unsigned int state, keycode_or_button; int same_screen;
} XInputEv;
typedef struct {
    int type; unsigned long serial; int send_event; void *display; XID event, window; int x, y, width, height;
} XConfigureEv;
typedef union { int type; XAnyEvent any; XInputEv input; XConfigureEv conf; long pad[24]; } XEv;
typedef struct {
    int width, height, xoffset, format; char *data; int byte_order, bitmap_unit, bitmap_bit_order, bitmap_pad, depth,
        bytes_per_line, bits_per_pixel; unsigned long red_mask, green_mask, blue_mask; void *obdata;
    void *funcs[6];
} XImg;

#define KeyPress 2
#define KeyRelease 3
#define ButtonPress 4
#define ButtonRelease 5
#define MotionNotify 6
#define ConfigureNotify 22
#define ClientMessage 33

static struct {
    void *(*XOpenDisplay)(const char *);
    int (*XDefaultScreen)(void *);
    XID (*XRootWindow)(void *, int);
    XID (*XCreateSimpleWindow)(void *, XID, int, int, unsigned, unsigned, unsigned, unsigned long, unsigned long);
    int (*XSelectInput)(void *, XID, long);
    int (*XMapWindow)(void *, XID);
    int (*XStoreName)(void *, XID, const char *);
    int (*XPending)(void *);
    int (*XNextEvent)(void *, XEv *);
    unsigned long (*XLookupKeysym)(XInputEv *, int);
    void *(*XDefaultGC)(void *, int);
    void *(*XDefaultVisual)(void *, int);
    int (*XDefaultDepth)(void *, int);
    XImg *(*XCreateImage)(void *, void *, unsigned, int, int, char *, unsigned, unsigned, int, int);
    int (*XPutImage)(void *, XID, void *, XImg *, int, int, int, int, unsigned, unsigned);
    int (*XFlush)(void *);
    unsigned long (*XInternAtom)(void *, const char *, int);
    int (*XSetWMProtocols)(void *, XID, unsigned long *, int);
} X;
static void *dpy;
static XID xwin;
static XImg *ximg;
static u32 *frame;      /* scaled picture, win_w x win_h */
static int frame_w, frame_h;
static unsigned long wm_delete;

static u32 xkey_bit(unsigned long ks)
{
    switch (ks) {
    case 'z': return KEY_A;
    case 'x': return KEY_B;
    case 0xffe1: case 0xffe2: case 0xff08: return KEY_SELECT;  /* Shift_L/R, BackSpace */
    case 0xff0d: return KEY_START;                            /* Return */
    case 0xff53: return KEY_RIGHT;
    case 0xff51: return KEY_LEFT;
    case 0xff52: return KEY_UP;
    case 0xff54: return KEY_DOWN;
    case 'w': return KEY_R;
    case 'q': return KEY_L;
    case 's': return KEY_X;
    case 'a': return KEY_Y;
    default: return 0;
    }
}

int window_open(int s)
{
    void *lib = dlopen("libX11.so.6", RTLD_NOW);
    int scr, scale = s > 0 ? s : 2;
#define GET(name) (*(void **)&X.name = dlsym(lib, #name))
    if (!lib)
        return 0;
    GET(XOpenDisplay); GET(XDefaultScreen); GET(XRootWindow); GET(XCreateSimpleWindow); GET(XSelectInput);
    GET(XMapWindow); GET(XStoreName); GET(XPending); GET(XNextEvent); GET(XLookupKeysym); GET(XDefaultGC);
    GET(XDefaultVisual); GET(XDefaultDepth); GET(XCreateImage); GET(XPutImage); GET(XFlush); GET(XInternAtom);
    GET(XSetWMProtocols);
#undef GET
    if (!X.XOpenDisplay || !X.XPutImage || !(dpy = X.XOpenDisplay(0)))
        return 0;
    if (X.XDefaultDepth(dpy, X.XDefaultScreen(dpy)) < 24) {
        fprintf(stderr, "window: needs a 24/32-bit X display\n");
        return 0;
    }
    scr = X.XDefaultScreen(dpy);
    win_w = DS_W * scale;
    win_h = DS_H * 2 * scale;
    xwin = X.XCreateSimpleWindow(dpy, X.XRootWindow(dpy, scr), 0, 0, (unsigned)win_w, (unsigned)win_h, 0, 0, 0);
    /* key press/release, button press/release, pointer motion, structure (resize) */
    X.XSelectInput(dpy, xwin, (1L << 0) | (1L << 1) | (1L << 2) | (1L << 3) | (1L << 6) | (1L << 17));
    X.XStoreName(dpy, xwin, "Nintendogs");
    wm_delete = X.XInternAtom(dpy, "WM_DELETE_WINDOW", 0);
    X.XSetWMProtocols(dpy, xwin, &wm_delete, 1);
    X.XMapWindow(dpy, xwin);
    return 1;
}

int window_present(const u32 *top, const u32 *bottom, int sw, int sh)
{
    int x, y, w, h, i, j;
    if (!dpy)
        return 1;
    build_canvas(top, bottom, sw, sh);
    while (X.XPending(dpy)) {
        XEv e;
        X.XNextEvent(dpy, &e);
        switch (e.type) {
        case KeyPress:
        case KeyRelease: {
            unsigned long ks = X.XLookupKeysym(&e.input, 0);
            u32 b = xkey_bit(ks);
            keys_down = e.type == KeyPress ? keys_down | b : keys_down & ~b;
            if (e.type == KeyPress && ks >= 0xffc5 && ks <= 0xffc7)  /* F8-F10 */
                fkey((int)(ks - 0xffbe) + 1);
            break;
        }
        case ButtonPress:
        case ButtonRelease:
            if (e.input.keycode_or_button == 1) {
                mouse_down = e.type == ButtonPress;
                mouse_at(e.input.x, e.input.y);
            }
            break;
        case MotionNotify:
            mouse_at(e.input.x, e.input.y);
            break;
        case ConfigureNotify:
            win_w = e.conf.width;
            win_h = e.conf.height;
            break;
        case ClientMessage:
            closed = 1;
            break;
        }
    }
    if (win_w != frame_w || win_h != frame_h || !ximg) {
        int scr = X.XDefaultScreen(dpy);
        free(frame);  /* XCreateImage's image keeps a pointer to it; the old image is leaked (rare) */
        frame = calloc((size_t)win_w * win_h, 4);
        frame_w = win_w;
        frame_h = win_h;
        ximg = frame ? X.XCreateImage(dpy, X.XDefaultVisual(dpy, scr), 24, 2 /* ZPixmap */, 0, (char *)frame,
                                      (unsigned)win_w, (unsigned)win_h, 32, 0) : 0;
        if (!ximg)
            return !closed;
    }
    fit(&x, &y, &w, &h);
    memset(frame, 0, (size_t)frame_w * frame_h * 4);  /* borders (layout or aspect may have changed) */
    for (j = 0; j < h; j++) {
        const u32 *src = pixels + (j * can_h / h) * can_w;
        u32 *dst = frame + (y + j) * frame_w + x;
        for (i = 0; i < w; i++)
            dst[i] = src[i * can_w / w] & 0xffffff;
    }
    X.XPutImage(dpy, xwin, X.XDefaultGC(dpy, X.XDefaultScreen(dpy)), ximg, 0, 0, 0, 0, (unsigned)frame_w,
                (unsigned)frame_h);
    X.XFlush(dpy);
    return !closed;
}

#endif
