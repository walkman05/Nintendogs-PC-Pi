#include "ctx.h"
/* Animation tick for obj (published in data_027c0360 meanwhile): advances the three 0x20-byte animation lists
 * (+0x7c/+0x70, +0x78/+0x6e, +0x80/+0x72), writing each one's current value (+0xc) to its target (+0x18); then the
 * 0x24-byte blend list (+0x74/+0x6c): advances its animation at +0x14, steps the frame counter (+4, capped at +6)
 * when obj flag 0x20 bit 1 is set, recomputing the fx32 weight (+8) = frame / end, and writes +0x20 to *(+0x10). */

extern u8 *data_027c0360;
extern void func_0208e8b4(u8 *param_1);
extern u64 func_0205e270(s32 a, s32 b);    /* signed divide: quotient in r0 (low word), remainder in r1 */

static void TickOne(u8 *e)
{
    u32 *dst;

    func_0208e8b4(e);
    dst = *(u32 **)(e + 0x18);
    if (dst != NULL) {
        *dst = *(u32 *)(e + 0xc);
    }
}

void func_01ffe474(u8 *obj)
{
    s32 i;
    s16 k;

    data_027c0360 = obj;
    /* The count is re-read after each element, as in the original. */
    for (i = 0; i < *(s16 *)(obj + 0x70); i++) {
        TickOne(*(u8 **)(obj + 0x7c) + i * 0x20);
    }
    for (i = 0; i < *(s16 *)(obj + 0x6e); i++) {
        TickOne(*(u8 **)(obj + 0x78) + i * 0x20);
    }
    for (i = 0; i < *(s16 *)(obj + 0x72); i++) {
        TickOne(*(u8 **)(obj + 0x80) + i * 0x20);
    }
    for (k = 0; k < *(s16 *)(obj + 0x6c); k = (s16)(k + 1)) {
        u8 *e = *(u8 **)(obj + 0x74) + (s32)k * 0x24;
        BOOL step = (*(u16 *)(obj + 0x20) & 2) != 0;
        u32 *dst;

        func_0208e8b4(e + 0x14);
        if (step) {
            s16 end;
            s16 cur;

            *(s16 *)(e + 4) = (s16)(*(s16 *)(e + 4) + 1);
            end = *(s16 *)(e + 6);
            cur = *(s16 *)(e + 4);
            if (cur >= end) {
                *(s16 *)(e + 4) = end;
                *(s32 *)(e + 8) = 0x1000;
            } else {
                *(s32 *)(e + 8) = (s32)(u32)func_0205e270((s32)((u32)(s32)cur << 12), end);
            }
        }
        dst = *(u32 **)(e + 0x10);
        if (dst != NULL) {
            *dst = *(u32 *)(e + 0x20);
        }
    }
    data_027c0360 = NULL;
}
