#include "ctx.h"
/* MIi_UncompressBackward: in-place backward LZ decompression of a module whose footer ends at bufEnd
 * (word at -8: header length << 24 | compressed length, word at -4: extra decompressed length); the original
 * then flushes the data cache and invalidates the instruction cache over the result (nothing on the host). */

void func_02000914(u32 bufEnd)
{
    u8 *end;
    u8 *src;
    u8 *lim;
    u8 *dst;
    u32 w0;
    u32 w1;

    if (bufEnd == 0) {
        return;
    }
    end = (u8 *)bufEnd;
    w0 = *(u32 *)(end - 8);
    w1 = *(u32 *)(end - 4);
    dst = end + w1;
    src = end - (w0 >> 24);
    lim = end - (w0 & 0x00ffffffu);
    while (src > lim) {
        u32 flags = *--src;
        int bit;
        for (bit = 0; bit < 8; bit++) {
            if ((flags & 0x80) == 0) {
                u8 b = *--src;
                *--dst = b;
            } else {
                u32 hi = *--src;
                u32 lo = *--src;
                u32 disp = (((hi << 8) | lo) & 0x0fffu) + 2;
                u32 len = (hi >> 4) + 3;
                while (len-- != 0) {
                    u8 b = dst[disp];
                    *--dst = b;
                }
            }
            if (src <= lim) {
                break;
            }
            flags <<= 1;
        }
    }
}
