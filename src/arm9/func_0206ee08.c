#include "ctx.h"
#pragma opt_propagation off

typedef struct {
    u8 a;
    u8 pad;
    u16 b;
} Hdr;

extern void func_0206c9b8(void *dst, void *src, u32 n);

#ifdef NDOGS_PC /* NDOGS_PC: widened parameter 1: the DS callee uses the whole register */
u32 func_0206ee08(u8 *param_1, u32 param_2, u32 param_3, u32 param_4, u32 param_5)
{
#else
u32 func_0206ee08(u8 *param_1, u16 param_2, u32 param_3, u32 param_4, u32 param_5)
{
#endif
    Hdr h;
    u8 *p;

    h.a = 6;
    h.b = param_2;
    func_0206c9b8(&h, param_1, 1);
    p = param_1 + 1;
    func_0206c9b8(&h.b, p, 2);
    p = p + 2;
    func_0206c9b8(&param_3, p, 1);
    p = p + 1;
    func_0206c9b8(&param_4, p, 4);
    p = p + 4;
    func_0206c9b8(&param_5, p, 4);
    return (u32)(p + 4 - param_1);
}
