#include "ctx.h"

extern void func_02077e04(u32 a, u32 b);

typedef struct {
    u8 pad[0x8];
    u32 field_8;
} Inner;

typedef struct {
    u8 pad[0x1c];
    Inner *field_1c;
} Obj;

/* Sets the light-enable flags of every material of the object's model; the flags argument passes straight through
 * (it was dropped here before, which handed func_02077e04 a garbage value). */
void func_0208f7d0(Obj *p, u32 v)
{
    func_02077e04(p->field_1c->field_8, v);
}
