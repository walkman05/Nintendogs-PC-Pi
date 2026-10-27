#include "ctx.h"
/* Joint animation evaluation (NNSi_G3dAnmBlendJnt-style, no blending): clears the 0x58-byte joint result, sets
 * all its flags, and runs the first animation in the list (next at +0x10) whose map entry for nodeId (u16 at
 * +0x1a + 2*nodeId) is enabled (bit 8), then fills the parts its flags mark as identity with 1.0. Returns TRUE. */

extern void func_0206c834(u32 data, void *dst, u32 size);    /* MIi_CpuClearFast */

typedef void (*JntAnmFunc)(u8 *result, u8 *anm, u32 dataIdx);

BOOL func_01ffe6cc(u8 *result, u8 *anm, u32 nodeId)
{
    s32 *r = (s32 *)result;

    func_0206c834(0, result, 0x58);
    r[0] = -1;
    do {
        u16 map = *(u16 *)(anm + nodeId * 2 + 0x1a);

        if (map & 0x100) {
            (*(JntAnmFunc *)(anm + 0xc))(result, anm, map & 0xffu);
            if (r[0] & 1) {          /* scale is 1 */
                r[1] = 0x1000;
                r[2] = 0x1000;
                r[3] = 0x1000;
            }
            if (r[0] & 8) {
                r[4] = 0x1000;
                r[5] = 0x1000;
                r[6] = 0x1000;
            }
            if (r[0] & 0x10) {
                r[7] = 0x1000;
                r[8] = 0x1000;
                r[9] = 0x1000;
            }
            if (r[0] & 2) {          /* rotation is identity: diagonal of the 3x3 at +0x28 */
                r[10] = 0x1000;
                r[14] = 0x1000;
                r[18] = 0x1000;
            }
            return TRUE;
        }
        anm = *(u8 **)(anm + 0x10);
    } while (anm != NULL);
    return TRUE;
}
