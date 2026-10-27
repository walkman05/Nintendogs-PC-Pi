/* Force-included first in every decompiled file when building for the PC host (NDOGS_PC).
 * Maps mwccarm-isms onto GCC/Clang; nothing here is used by the matching DS build. */
#ifndef HOST_COMPAT_H
#define HOST_COMPAT_H
#ifdef NDOGS_PC
/* mwcc accepts `asm` blocks and functions; host builds replace those files individually (see port/asm/). */
#endif
#endif
