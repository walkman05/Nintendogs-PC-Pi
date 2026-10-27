#include "ctx.h"
/* SVC_UncompressLZ16-style LZ77 (type 0x10) decompressor writing 16 bits at a time (VRAM-safe).
 * Like the original, a back-reference reads already-written halfwords, and a final odd byte is not stored. */

void func_0206cb00(const void *src, void *dst)
{
    const u8 *s = (const u8 *)src;
    u32 d = (u32)dst;
    u32 hdr = (u32)s[0] | ((u32)s[1] << 8) | ((u32)s[2] << 16) | ((u32)s[3] << 24);
    s32 remain = (s32)(hdr >> 8);
    u32 acc = 0;   /* pending output halfword */
    u32 shift = 0; /* 0: next byte is the low half, 8: the high half */

    s += 4;
    while (remain > 0) {
        u32 flags = *s++;
        s32 bit = 8;
        while (--bit >= 0) {
            if (!(flags & 0x80)) {
                acc |= (u32)*s++ << shift;
                remain--;
                shift ^= 8;
                if (shift == 0) {
                    *(u16 *)d = (u16)acc;
                    d += 2;
                    acc = 0;
                }
            } else {
                s32 len = 3 + (s[0] >> 4);
                u32 disp = ((((u32)s[0] & 0xf) << 8) | s[1]) + 1;
                u32 sel = (8 - shift) ^ ((disp & 1) << 3);
                s += 2;
                remain -= len;
                do {
                    u32 off;
                    u32 h;
                    sel ^= 8;
                    off = ((disp + ((8 - shift) >> 3)) >> 1) << 1;
                    h = *(const u16 *)(d - off);
                    acc |= ((h >> sel) & 0xff) << shift;
                    shift ^= 8;
                    if (shift == 0) {
                        *(u16 *)d = (u16)acc;
                        d += 2;
                        acc = 0;
                    }
                } while (--len > 0);
            }
            if (remain <= 0) {
                break;
            }
            flags <<= 1;
        }
    }
}
