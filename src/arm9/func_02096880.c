#pragma opt_propagation off
#include "ctx.h"

extern u32 data_0212f5c0;
extern u8 *func_020038b8(void *p, u32 n);

u32 func_02096880(u16 *p1, void *p2, u32 p3, u32 p4)
{
  u8 *e;
  u32 n;
  u32 ret;
  u8 *buf;
  u32 vv;
  u8 *bn;
  u32 z;
  e = (u8 *)&data_0212f5c0 + p3 * 12;
  n = *(u32 *)(e + 4) * *(u16 *)(e + 8);
  ret = 0;
  buf = func_020038b8(p2, n * 2);
  p1[0] = (u16)p3;
  p1[1] = *(u16 *)(e + 8);
  *(u32 *)(p1 + 2) = *(u32 *)(e + 4);
  bn = buf + n;
  z = 0;
  *(u32 *)(p1 + 4) = p4;
  *(void **)(p1 + 6) = p2;
  *(u8 **)(p1 + 8) = buf;
  *(u8 **)(p1 + 10) = bn;
#ifdef NDOGS_PC
  /* full-game mode: the retail game reads both copies of the block from the save chip here (port/src/save.c) */
  if (ndogs_full) {
    extern int ndogs_save_read(u32 addr, void *dst, u32 len);
    if (ndogs_save_read(p4, buf, n))
      ndogs_save_read(p4 + 0x20000, bn, n);
  }
#endif
  if (z != 0) goto set3;
  if (z == 0) goto done;
set3:
  ret = 3;
done:
  return ret;
}
