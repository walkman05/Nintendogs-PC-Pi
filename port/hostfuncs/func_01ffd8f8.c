#include "ctx.h"
/* Starts blend-animation channel idx of the dog: binds resource res to entry idx of the 0x24-byte list at
 * dog+0x74, sets its speed (spd << 12), mode byte, frame counter/length, and start frame (by startMode:
 * -1 -> frame, 0 -> keep, 1 -> last frame, 2 -> frame or random when frame == -1, other -> startMode - 3),
 * starts the linked effect slots (data_0213b670) in dog+0x95c, then recursively starts each child channel
 * (data_0213b794 list, -1 terminated) whose resource slot (dog+0x17c + data_0213b484[child]*4) is empty. */

extern u8 *data_0214f464;          /* game state; word 0 is the random seed */
extern s16 data_0213b670[];        /* per channel: 2 effect slots (-1 = none) */
extern s16 data_0213b794[];        /* per channel: 6 children, -1 terminated */
extern s16 data_0213b484[];        /* channel -> resource slot */

extern void func_0208e690(u32 *param_1, u8 *param_2);
extern void func_0209c598(u8 *p, u32 i, u32 a, u32 b);

void func_01ffd8f8(u8 *dog, s32 idx, u32 res, u32 id, s32 frame, s16 spd, s32 mode, s16 startMode, s16 len)
{
    u8 *e = *(u8 **)(dog + 0x74) + (s32)(s16)idx * 0x24;
    const s16 *kids;
    s16 v;
    s16 k;
    s8 j;

    func_0208e690((u32 *)e, (u8 *)res);
    *(s32 *)(e + 0x1c) = (s32)((u32)(s32)spd << 12);
    e[0x14] = (u8)mode;
    *(s16 *)(e + 4) = 0;
    *(s16 *)(e + 6) = len;
    switch (startMode + 1) {
    case 0:
        *(s32 *)(e + 0x20) = (s32)((u32)frame << 12);
        break;
    case 1:
        break;
    case 2:
        *(s32 *)(e + 0x20) = (s32)((u32)(s32)*(s16 *)(e + 0x18) << 12);
        break;
    case 3:
        if (frame == -1) {
            u32 *seed = (u32 *)data_0214f464;
            u32 s = *seed * 0x10dcdu + 1u;

            *seed = s;
            frame = (s32)((((s & 0xffffu) * (u32)*(u16 *)(e + 0x18)) >> 16) & 0xffffu);
        }
        *(s32 *)(e + 0x20) = (s32)((u32)frame << 12);
        break;
    default:
        *(s32 *)(e + 0x20) = (s32)((u32)(startMode - 3) << 12);
        break;
    }

    for (j = 0; j < 2; j = (s8)(j + 1)) {
        v = data_0213b670[idx * 2 + j];
        if (v == -1) {
            break;
        }
        func_0209c598(dog + 0x95c, (u32)(s32)v, id, spd < 0 ? 1u : 0u);
    }

    kids = &data_0213b794[idx * 6];
    v = kids[0];
    if (v == -1) {
        return;
    }
    /* As in the original, the first child is visited twice (the index is read before it is advanced). */
    k = 0;
    do {
        if (*(u32 *)(dog + 0x17c + data_0213b484[v] * 4) == 0) {
            func_01ffd8f8(dog, v, res, id, frame, spd, mode, startMode, len);
        }
        v = kids[k];
        k = (s16)(k + 1);
    } while (v != -1);
}
