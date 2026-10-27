/* Force-included first in every decompiled file when building for the PC host (NDOGS_PC).
 * Maps mwccarm-isms onto GCC/Clang; nothing here is used by the matching DS build. */
#ifndef HOST_COMPAT_H
#define HOST_COMPAT_H
#ifdef NDOGS_PC
/* Inline `asm {}` blocks are mwcc-only: each one is wrapped as #ifdef NDOGS_PC <C equivalent> #else asm {...} #endif
 * in its file, using the helpers in host_cpu.h. */
#include "host_cpu.h"
#endif
#endif
