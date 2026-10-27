#include "ctx.h"
/* MI_CpuCopy8: copies size bytes from src to dst using only 16/32-bit accesses (VRAM-safe). */

void func_0206c9b8(const void *src, void *dst, u32 size)
{
    u32 s = (u32)src;
    u32 d = (u32)dst;
    u32 n = size;
    u32 t;
    u32 end;

    if (n == 0) {
        return;
    }
    if (d & 1) {
        /* finish the destination halfword's high byte */
        u32 lo = *(u16 *)(d - 1) & 0xff;
        if (s & 1) {
            t = (u32)*(const u16 *)(s - 1) >> 8;
        } else {
            t = *(const u16 *)s;
        }
        *(u16 *)(d - 1) = (u16)(lo | (t << 8));
        s++;
        d++;
        n--;
        if (n == 0) {
            return;
        }
    }
    if ((d ^ s) & 1) {
        /* destination even, source odd: reassemble halfwords from byte pairs */
        u32 h;
        s &= ~1u;
        t = (u32)*(const u16 *)s >> 8;
        s += 2;
        while (n >= 2) {
            n -= 2;
            h = *(const u16 *)s;
            s += 2;
            h = t | (h << 8);
            *(u16 *)d = (u16)h;
            d += 2;
            t = h >> 16;
        }
        if (n & 1) {
            *(u16 *)d = (u16)((*(u16 *)d & 0xff00u) | t);
        }
        return;
    }
    if ((d ^ s) & 2) {
        /* both even, different word alignment: halfword copy */
        t = n & ~1u;
        if (t != 0) {
            n -= t;
            end = d + t;
            do {
                *(u16 *)d = *(const u16 *)s;
                s += 2;
                d += 2;
            } while (d < end);
        }
    } else if (n >= 2) {
        if (d & 2) {
            *(u16 *)d = *(const u16 *)s;
            s += 2;
            d += 2;
            n -= 2;
            if (n == 0) {
                return;
            }
        }
        t = n & ~3u;
        if (t != 0) {
            n -= t;
            end = d + t;
            do {
                *(u32 *)d = *(const u32 *)s;
                s += 4;
                d += 4;
            } while (d < end);
        }
        if (n & 2) {
            *(u16 *)d = *(const u16 *)s;
            s += 2;
            d += 2;
        }
    }
    if (n & 1) {
        *(u16 *)d = (u16)((*(u16 *)d & 0xff00u) | (*(const u16 *)s & 0xffu));
    }
}
