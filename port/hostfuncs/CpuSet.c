#include "ctx.h"
/* BIOS CpuSet (SWI 0x0B): copy or fill cnt[20:0] units, 32-bit if cnt bit 26 else 16-bit, fill if bit 24. */

void CpuSet(void *src, void *dst, u32 cnt)
{
    u32 n = cnt & 0x1fffff;
    u32 i;

    if (cnt & (1u << 26)) {
        const vu32 *s = (const vu32 *)((u32)src & ~3u);
        vu32 *d = (vu32 *)((u32)dst & ~3u);
        if (cnt & (1u << 24)) {
            u32 v = *s;
            for (i = 0; i < n; i++) {
                d[i] = v;
            }
        } else {
            for (i = 0; i < n; i++) {
                d[i] = s[i];
            }
        }
    } else {
        const vu16 *s = (const vu16 *)((u32)src & ~1u);
        vu16 *d = (vu16 *)((u32)dst & ~1u);
        if (cnt & (1u << 24)) {
            u16 v = *s;
            for (i = 0; i < n; i++) {
                d[i] = v;
            }
        } else {
            for (i = 0; i < n; i++) {
                d[i] = s[i];
            }
        }
    }
}
