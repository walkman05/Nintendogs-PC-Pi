#pragma avoid_byte strb
#include "ctx.h"

typedef struct {
    void *p0;
    u16 h4;
    u16 h6;
    u8 b[6];
} D;

extern D data_02159500;
extern u32 data_02159560;
#ifdef NDOGS_PC /* NDOGS_PC: return type of func_020645c4 as defined (x86 extends by the caller's prototype, the DS by the callee) */
extern u32 func_020645c4(void);
#else
extern u16 func_020645c4(void);
#endif

void func_020d5da4(u16 param_1)
{
#ifdef NDOGS_PC /* NDOGS_PC: widened: holds func_020645c4's value unchanged, as the DS register does */
    u32 r;
#else
    u16 r;
#endif

    data_02159500.h4 = param_1;
    data_02159500.p0 = &data_02159560;
    r = func_020645c4();
    data_02159500.b[0] = 0xff;
    data_02159500.b[1] = 0xff;
    data_02159500.b[2] = 0xff;
    data_02159500.b[3] = 0xff;
    data_02159500.b[4] = 0xff;
    data_02159500.b[5] = 0xff;
    data_02159500.h6 = r;
}
