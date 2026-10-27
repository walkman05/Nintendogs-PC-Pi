/* The window: both DS screens stacked (top, then bottom), scaled. Win32 (GDI) on Windows; on other systems a
 * headless stand-in for now (frames are dropped, no input). The mouse on the bottom screen is the stylus;
 * keyboard: arrows = D-pad, Z = A, X = B, A = Y, S = X, Q = L, W = R, Enter = Start, Right Shift = Select. */
#include <stdlib.h>
#include <string.h>
#include "ds_video.h"

#ifdef _WIN32
#include <windows.h>

/* user32/gdi32 are loaded at run time, after the DS memory map is reserved: loaded at startup they allocate
 * shared memory inside the DS address range. */
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
    int (WINAPI *StretchDIBits)(HDC, int, int, int, int, int, int, int, int, const void *, const BITMAPINFO *, UINT,
                                DWORD);
} W;

static int load_ui(void)
{
    HMODULE u = LoadLibraryA("user32.dll"), g = LoadLibraryA("gdi32.dll");
    if (!u || !g)
        return 0;
#define GET(lib, name) (*(FARPROC *)&W.name = GetProcAddress(lib, #name))
    GET(u, RegisterClassA); GET(u, LoadCursorA); GET(u, AdjustWindowRect); GET(u, CreateWindowExA);
    GET(u, ShowWindow); GET(u, InvalidateRect); GET(u, PeekMessageA); GET(u, TranslateMessage);
    GET(u, DispatchMessageA); GET(u, DefWindowProcA); GET(u, PostQuitMessage); GET(u, DestroyWindow);
    GET(u, SetCapture); GET(u, ReleaseCapture); GET(u, BeginPaint); GET(u, EndPaint); GET(g, StretchDIBits);
#undef GET
    return W.CreateWindowExA && W.StretchDIBits;
}

static HWND wnd;
static int scale = 2;
static u32 pixels[DS_W * DS_H * 2];
static int closed;
static int mouse_down, mouse_x, mouse_y;
static u32 keys = 0x0fff;

static u32 key_bit(WPARAM vk)
{
    switch (vk) {
    case 'Z': return 1u << 0;        /* A */
    case 'X': return 1u << 1;        /* B */
    case VK_RSHIFT: case VK_SHIFT: return 1u << 2; /* Select */
    case VK_RETURN: return 1u << 3;  /* Start */
    case VK_RIGHT: return 1u << 4;
    case VK_LEFT: return 1u << 5;
    case VK_UP: return 1u << 6;
    case VK_DOWN: return 1u << 7;
    case 'W': return 1u << 8;        /* R */
    case 'Q': return 1u << 9;        /* L */
    case 'S': return 1u << 10;       /* X */
    case 'A': return 1u << 11;       /* Y */
    default: return 0;
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
    case WM_LBUTTONDOWN:
        W.SetCapture(h);
        mouse_down = 1;
        /* fall through */
    case WM_MOUSEMOVE:
        mouse_x = (short)LOWORD(lp) / scale;
        mouse_y = (short)HIWORD(lp) / scale - DS_H;
        return 0;
    case WM_LBUTTONUP:
        W.ReleaseCapture();
        mouse_down = 0;
        return 0;
    case WM_KEYDOWN:
        keys &= ~key_bit(wp);
        return 0;
    case WM_KEYUP:
        keys |= key_bit(wp);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = W.BeginPaint(h, &ps);
        BITMAPINFO bi;
        memset(&bi, 0, sizeof bi);
        bi.bmiHeader.biSize = sizeof bi.bmiHeader;
        bi.bmiHeader.biWidth = DS_W;
        bi.bmiHeader.biHeight = -DS_H * 2;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        W.StretchDIBits(dc, 0, 0, DS_W * scale, DS_H * 2 * scale, 0, 0, DS_W, DS_H * 2, pixels, &bi, DIB_RGB_COLORS,
                      SRCCOPY);
        W.EndPaint(h, &ps);
        return 0;
    }
    }
    return W.DefWindowProcA(h, msg, wp, lp);
}

int window_open(int s)
{
    WNDCLASSA wc;
    RECT r;
    scale = s > 0 ? s : 2;
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
    r.right = DS_W * scale;
    r.bottom = DS_H * 2 * scale;
    W.AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, FALSE);
    wnd = W.CreateWindowExA(0, "nintendogs_pc", "Nintendogs", WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
                        CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, 0, 0, wc.hInstance, 0);
    if (!wnd)
        return 0;
    W.ShowWindow(wnd, SW_SHOW);
    return 1;
}

int window_present(const u32 *top, const u32 *bottom)
{
    MSG m;
    if (!wnd)
        return 1;
    memcpy(pixels, top, DS_W * DS_H * 4);
    memcpy(pixels + DS_W * DS_H, bottom, DS_W * DS_H * 4);
    W.InvalidateRect(wnd, 0, FALSE);
    while (W.PeekMessageA(&m, 0, 0, 0, PM_REMOVE)) {
        W.TranslateMessage(&m);
        W.DispatchMessageA(&m);
    }
    return !closed;
}

int window_touch(int *x, int *y)
{
    if (!mouse_down || mouse_y < 0 || mouse_y >= DS_H || mouse_x < 0 || mouse_x >= DS_W)
        return 0;
    *x = mouse_x;
    *y = mouse_y;
    return 1;
}

u32 window_keys(void) { return keys; }

#else

int window_open(int s) { (void)s; return 1; }
int window_present(const u32 *top, const u32 *bottom) { (void)top; (void)bottom; return 1; }
int window_touch(int *x, int *y) { (void)x; (void)y; return 0; }
u32 window_keys(void) { return 0x0fff; }

#endif
