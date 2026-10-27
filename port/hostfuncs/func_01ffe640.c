#include "ctx.h"
/* Joint blend callback (funcBlendJnt replacement): evaluates the joint animation (func_01ffe6cc), blends the
 * result with the node's stored matrix when the node's weight (table entry +4 -> +8) is below 1.0 (func_01ffe7b4),
 * then stores the result's rotation and translation back as the node's 4x3 matrix. Returns func_01ffe6cc's result. */

extern u8 *data_027c0360;
extern BOOL func_01ffe6cc(u8 *result, u8 *anm, u32 nodeId);
extern void func_01ffe7b4(const s32 *mtx, const u8 *src, u8 *dst, s32 w);
extern void func_0206c8b8(u32 *param_1, u32 *param_2);       /* 36-byte (3x3 matrix) copy */

BOOL func_01ffe640(u8 *result, u8 *anm, u32 nodeId)
{
    u8 *ro = *(u8 **)(data_027c0360 + 0x1c);
    u8 *ent = *(u8 **)(ro + 0x5c) + (s32)(s16)nodeId * 12;
    u8 *mtx = *(u8 **)(ro + 0x60) + nodeId * 0x30;
    s32 w = *(s32 *)(*(u8 **)(ent + 4) + 8);
    BOOL ret = func_01ffe6cc(result, anm, nodeId);

    if (w < 0x1000) {
        func_01ffe7b4((const s32 *)mtx, result, result, w);
    }
    func_0206c8b8((u32 *)(result + 0x28), (u32 *)mtx);
    *(u32 *)(mtx + 0x24) = *(u32 *)(result + 0x4c);
    *(u32 *)(mtx + 0x28) = *(u32 *)(result + 0x50);
    *(u32 *)(mtx + 0x2c) = *(u32 *)(result + 0x54);
    return ret;
}
