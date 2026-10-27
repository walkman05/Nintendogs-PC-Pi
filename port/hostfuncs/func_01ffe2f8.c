#include "ctx.h"
#include "nitro.h"
/* Model draw callback: publishes obj in data_027c0360 and, when it has a render object (+0x1c) and is not hidden
 * (flag 0x20 bit 0), either draws it through the render-object path (translate, rotation copied to the shared
 * camera block, scale, polygon attributes, draw) or, with flag bit 3, loads its 4x3 matrix and scale straight into
 * the geometry buffer and draws its shape directly. Clears data_027c0360 afterwards. */

extern u8 *data_027c0360;
extern u32 data_02152d94[];
extern u8 data_02152cd8[];

extern void func_0206c8b8(u32 *param_1, u32 *param_2);       /* 36-byte (3x3 matrix) copy */
extern void func_02074768(const VecFx32 *p);
extern void func_02074790(const VecFx32 *p);
extern void func_020746f0(u32 param_1, s32 param_2, s32 param_3, s32 param_4, s32 param_5, u32 param_6);
extern void func_020748a0(void);
extern void func_020747b8(void);
extern void func_02077168(u32 *param_1);
extern void func_020776d4(u32 param_1, u32 *param_2, s32 param_3);
extern void func_02077fb4(u8 *param_1, u32 param_2, u32 param_3);
extern void func_020774c0(u8 *param_1, s32 param_2, s32 param_3, s32 param_4);

void func_01ffe2f8(u8 *obj)
{
    u32 rot[9];
    u32 mtx[12];
    u8 *ro;
    u16 flag;
    int i;

    data_027c0360 = obj;
    ro = *(u8 **)(obj + 0x1c);
    if (ro != NULL && (*(u16 *)(obj + 0x20) & 1) == 0) {
        func_0206c8b8((u32 *)(obj + 0x3c), rot);
        flag = *(u16 *)(obj + 0x20);
        if ((flag & 8) == 0) {
            u32 *fp = (u32 *)(data_02152cd8 + 0xfc);

            func_02074768((const VecFx32 *)(obj + 0x24));
            func_0206c8b8(rot, data_02152d94);
            *fp &= ~0xa4u;
            func_02074790((const VecFx32 *)(obj + 0x30));
            func_020746f0(0xf, 0, 2, obj[0x22], obj[0x23], 0);
            if ((*(u16 *)(obj + 0x20) & 0x10) == 0) {
                func_020748a0();
            } else {
                func_020747b8();
            }
            func_02077168((u32 *)(*(u8 **)(obj + 0x1c) + 4));
        } else {
            u8 *shp = *(u8 **)(ro + 8);

            if (*(u32 *)(data_02152cd8 + 0xfc) & 1) {
                func_020748a0();
            }
            for (i = 0; i < 9; i++) {
                mtx[i] = *(u32 *)(obj + 0x3c + i * 4);
            }
            mtx[9] = *(u32 *)(obj + 0x30);
            mtx[10] = *(u32 *)(obj + 0x34);
            mtx[11] = *(u32 *)(obj + 0x38);
            func_020776d4(0x17, mtx, 12);                    /* MTX_LOAD_4x3 */
            func_020776d4(0x1b, (u32 *)(obj + 0x24), 3);     /* MTX_SCALE */
            func_02077fb4(shp, *(u16 *)(obj + 0x84), obj[0x22]);
            func_020774c0(*(u8 **)(*(u8 **)(obj + 0x1c) + 8), *(u16 *)(obj + 0x84), *(u16 *)(obj + 0x86), 1);
        }
    }
    data_027c0360 = NULL;
}
