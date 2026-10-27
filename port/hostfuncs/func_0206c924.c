#include "ctx.h"
/* MI_CpuFill8: fills size bytes at dst with the byte data using only 16/32-bit stores (VRAM-safe).
 * Returns r0 (low: final cursor) and r1 (high: the widened fill value, or the last halfword written when the
 * size ends on an odd byte) as the original leaves them; func_020873d8 reads r1. */

u64 func_0206c924(void *dst, u32 data, u32 size)
{
    u32 p = (u32)dst;
    u32 n = size;
    u32 v = data;
    u32 end;
    u32 words;

    if (n == 0) {
        return (u64)p | ((u64)v << 32);
    }
    if (p & 1) {
        u16 *h = (u16 *)(p - 1);
        *h = (u16)((*h & 0xff) | (v << 8));
        p++;
        n--;
        if (n == 0) {
            return (u64)p | ((u64)v << 32);
        }
    }
    if (n >= 2) {
        v |= v << 8;
        if (p & 2) {
            *(u16 *)p = (u16)v;
            p += 2;
            n -= 2;
            if (n == 0) {
                return (u64)p | ((u64)v << 32);
            }
        }
        v |= v << 16;
        words = n & ~3u;
        if (words != 0) {
            n -= words;
            end = p + words;
            do {
                *(u32 *)p = v;
                p += 4;
            } while (p < end);
        }
        if (n & 2) {
            *(u16 *)p = (u16)v;
            p += 2;
        }
    }
    if (n & 1) {
        u16 *h = (u16 *)p;
        v = (*h & 0xff00u) | (v & 0xff);
        *h = (u16)v;
    }
    return (u64)p | ((u64)v << 32);
}
