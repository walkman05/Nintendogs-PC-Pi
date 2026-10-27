# Nintendogs decomp + PC port

Two things live here:
- the matching decompilation (`src/`, `include/`, `functions.csv`, `remaining.txt`), built with mwccarm 1.2;
- a native Windows/Linux port in progress (`port/`, `CMakeLists.txt`), which compiles the same C for the host.

## Hard rules
- Never commit ROMs, files extracted from a ROM, or anything derived from game assets (see `.gitignore`).
  Extraction runs at build time from the user's own ROM.
- Don't edit `src/` in ways that break the matching build. Port-only changes go behind `#ifdef NDOGS_PC` or in
  `port/`. `src/arm9/func_0200a648.c` is the one non-matching file (marked `nonmatching` in `functions.csv`).
- Matching-build rules: small inline asm only, no whole-function asm.

## Port conventions
- The host build is 32-bit (Linux `-m32`, Windows `-A Win32`): the game assumes 32-bit pointers and fixed DS
  addresses. DS RAM regions are mapped at their real addresses by `port/src/platform_mem.c`.
- Compile with `-fwrapv -fno-strict-aliasing` (or MSVC equivalents): DS code relies on wrapping signed
  arithmetic and type punning.
- Hardware registers (0x04000000+) are never mapped as memory; accesses go through the hardware layer
  (milestone 4).
- The ARM7 side is not decompiled; the plan is to run the ROM's ARM7 binary in an emulated core behind the
  inter-processor FIFO.
- Keep subsystems in separate directories under `port/` so parallel sessions don't touch the same files.

## Milestones
M0 infra (this) · M1 ROM extraction · M2 host compile of all C · M3 memory/overlay loading · M4 hardware layer ·
M5 boot to main menu · M6 input, audio, polish.

## ROM extraction (milestone 1)
`python port/tools/extract_rom.py --rom <baserom.nds> --dsd <dsd>` uses AetiasHax/ds-decomp (dsd v0.12.1, MIT) to
fill `extracted/` (git-ignored): `rom/` (arm9/arm7/itcm/dtcm binaries, decompressed overlays, game files),
`config/` (symbols, relocations, delinks per module) and `data_symbols.txt` (each data_/bss_ symbol the C uses).
The ROM must be the European kiosk demo, MD5 a190eca6a61b3c69ce48f076d2445eb8.

## Host compile (milestone 2)
`python port/tools/host_compile.py --link build/objs` (CI job `game-compile`) compiles every file with gcc -m32 and
links the whole game. Status: all 8,311 files compile and the game links with 0 undefined symbols.
- Inline asm: wrapped as `#ifdef NDOGS_PC <C> #else asm #endif`; `port/tools/check_ds_unchanged.py <files>` must
  report "unchanged" for any edit in `src/`.
- `port/hostfuncs/`: host-only C for routines with no decompiled C (compiler runtime incl. exact soft float,
  BIOS calls, SDK asm, ITCM code). Hardware registers there go through `HostIoRead/Write*` (`port/src/host_io.c`).
- `port/link/veneers.txt`: the original's linker trampolines, emitted as x86 jump stubs.
- Data symbols resolve to the address in their name (generated linker script).
- Return values: 170 functions that left a value in r0 for callers (tail-call results, swpb old bytes, extended
  loads) now return it explicitly (plain C where the DS object stays identical, else `#ifdef NDOGS_PC`). The rest
  of what `port/tools/proto_check.py` flags was checked and found unread on the DS (dispatchers overwrite r0).
  Open: values returned through vtables/callbacks (e.g. func_0207e1ac's target) depend on those callbacks.

## Build
```
cmake -S . -B build -A Win32                 # Windows (Visual Studio)
cmake -S . -B build -DCMAKE_C_FLAGS=-m32     # Linux (needs gcc-multilib)
cmake --build build
```
`-DNDOGS_BUILD_GAME=ON` adds the decompiled sources (expected to fail until milestone 2).
