/* DS hardware layer (milestone 4). */
#ifndef HW_H
#define HW_H
#include "ctx.h"

/* Register model: called before an access is allowed to happen (read) and after it happened (write).
 * hw_io_prepare_read() refreshes the backing memory of the register containing addr; hw_io_written() applies
 * the side effect of a write to addr, whose new value is already in the backing memory. */
void hw_io_prepare_read(u32 addr);
void hw_io_written(u32 addr);
void hw_init(void);

/* Trap: make the I/O register pages fault on access and route accesses through the model. */
int hw_trap_install(void);

/* Accessors for host code (same model, no fault). */
u32 HostIoRead32(u32 addr);
u16 HostIoRead16(u32 addr);
u8 HostIoRead8(u32 addr);
void HostIoWrite32(u32 addr, u32 value);
void HostIoWrite16(u32 addr, u16 value);
void HostIoWrite8(u32 addr, u8 value);

/* Debug: NDOGS_IOLOG=1 logs every trapped register access (stderr). */
extern int hw_iolog;

#endif
