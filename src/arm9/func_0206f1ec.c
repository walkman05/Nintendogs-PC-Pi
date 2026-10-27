#include "ctx.h"

#ifdef NDOGS_PC
extern u32 func_0206f518(u16 *p, s32 q);
#else
extern void func_0206f518(u16 *p, s32 q);
#endif

#ifdef NDOGS_PC
u32 func_0206f1ec(u16 *param_1, s32 param_2)
#else
void func_0206f1ec(u16 *param_1, s32 param_2)
#endif
{
    if (param_2 <= 0) {
#ifdef NDOGS_PC
        return (u32)param_1; /* r0 still holds param_1 */
#else
        return;
#endif
    }
    if (param_1 != NULL) {
#ifdef NDOGS_PC
        return func_0206f518(param_1, param_2);
#else
        func_0206f518(param_1, param_2);
#endif
    }
#ifdef NDOGS_PC
    return 0; /* r0 == param_1 == NULL */
#endif
}
