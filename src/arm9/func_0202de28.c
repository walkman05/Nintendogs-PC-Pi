#include "ctx.h"

extern int data_02159444;
#ifdef NDOGS_PC /* NDOGS_PC: return type of func_02068078 as defined (x86 extends by the caller's prototype, the DS by the callee) */
extern u32 func_02068078(int a, int b);
#else
extern s16 func_02068078(int a, int b);
#endif
extern u32 func_0202dee4(int p);
extern int func_0202e318(int a, int b);
#ifdef NDOGS_PC /* NDOGS_PC: return type of func_0202e500 as defined (x86 extends by the caller's prototype, the DS by the callee) */
extern BOOL func_0202e500(int p, int a, int b);
#else
extern u8 func_0202e500(int p, int a, int b);
#endif

s32 func_0202de28(int param_1, int param_2)
{
    int p1c;
    int p2c;
#ifdef NDOGS_PC /* holds FX_Atan2Idx's 0..0xffff unchanged, as the DS register does */
    s32 sVar2;
#else
    s16 sVar2;
#endif
    int bVar1;
    int iVar3;
    u32 uVar4;
    int iVar5;

    p1c = param_1;
    p2c = param_2;
    iVar5 = *(int *)(data_02159444 + 0x98);
    iVar3 = *(int *)(data_02159444 + 0x94);
    if (iVar3 * iVar3 + iVar5 * iVar5 <= 25) {
        bVar1 = 0;
    } else {
        sVar2 = func_02068078(iVar3, iVar5);
        bVar1 = 1;
    }
    if (bVar1 == 0) {
        goto ret0;
    }
    uVar4 = func_0202dee4(p1c);
    if (uVar4 != 0) {
        return func_0202e500(p1c, p2c, (int)sVar2);
    }
    iVar3 = func_0202e318(p2c, (int)sVar2);
    if (iVar3 == 0) {
        return 0;
    }
    *(s16 *)(p1c + 0xea) = (s16)p2c * 5 + (iVar3 + 0x1de);
    return 1;
ret0:
    return 0;
}
