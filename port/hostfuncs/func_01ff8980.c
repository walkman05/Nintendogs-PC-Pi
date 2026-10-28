#include "ctx.h"
#include "nitro.h"
/* Flattens v onto the XZ plane as a unit direction: v = (sin, 0, cos) of FX_Atan2Idx(v->x, v->z). */

#ifdef NDOGS_PC /* NDOGS_PC: return type of func_02068078 as defined (x86 extends by the caller's prototype, the DS by the callee) */
extern u32 func_02068078(s32 p1, s32 p2); /* FX_Atan2Idx */
#else
extern u16 func_02068078(s32 p1, s32 p2); /* FX_Atan2Idx */
#endif
extern const s16 data_02126868[];         /* FX_SinCosTable_: sin, cos pairs */

void func_01ff8980(VecFx32 *v)
{
    u32 idx = (u32)func_02068078(v->x, v->z) >> 4;

    v->x = data_02126868[idx * 2];
    v->y = 0;
    v->z = data_02126868[idx * 2 + 1];
}
