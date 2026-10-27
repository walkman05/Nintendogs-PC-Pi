#include "ctx.h"
/* Binds resource slot b to channel a of p: reuses the pool entry already holding b, else takes the first entry
 * whose use count is <= 0 and loads b into it; then flags the channel, stores the entry in its back buffer and
 * bumps the entry's use count. Pool: data_02157ccc -> { s16 count; ...; u8 *entries (stride 0x5048) at +4 }. */

extern u8 *data_02157ccc;
extern u32 data_02157cd4[];           /* per-slot pair of words passed to func_0208c98c */
extern void func_0208c98c(s32 a, s32 b, s32 c, s32 d);
extern void func_02087ea0(u32 a, u32 b, u32 c);

#define POOL_STRIDE 0x5048

void func_01ffe154(u8 *p, s32 a, u32 b)
{
    u8 *pool = data_02157ccc;
    s16 count = *(s16 *)pool;
    u8 *chan = p + 0xc0 + (s32)(s16)a * 12;
    u8 *ent = NULL;
    u8 *e;
    s16 i;

    if (count > 0) {
        e = *(u8 **)(pool + 4);
        i = 0;
        do {
            if ((u32)*(u16 *)(e + 0x5044) == b) {
                ent = e;
                break;
            }
            i = (s16)(i + 1);
            e += POOL_STRIDE;
        } while (i < *(s16 *)pool);
    }
    if (ent == NULL) {
        if (count > 0) {
            pool = data_02157ccc;
            e = *(u8 **)(pool + 4);
            i = 0;
            do {
                if (*(s16 *)(e + 0x5046) <= 0) {
                    ent = e;
                    break;
                }
                i = (s16)(i + 1);
                e += POOL_STRIDE;
            } while (i < *(s16 *)pool);
        }
        /* r3 (the fourth argument) is a leftover register in the original; func_0208c98c ignores it. */
        func_0208c98c((s32)(u32)(ent + 0x5000), (s32)data_02157cd4[b * 2], (s32)data_02157cd4[b * 2 + 1], 0);
        func_02087ea0((u32)(ent + 0x5000), (u32)ent,
                      *(u32 *)(ent + 0x5024) - *(u32 *)(ent + 0x5020));
        *(u16 *)(ent + 0x5044) = (u16)b;
    }
    *(u16 *)(chan + 2) = 1;
    *(u8 **)(chan + 4 + (u32)(*(u16 *)chan ^ 1) * 4) = ent;
    *(s16 *)(ent + 0x5046) = (s16)(*(s16 *)(ent + 0x5046) + 1);
}
