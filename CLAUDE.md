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
M5 window + rendering (boots to the menu and gameplay) · M6 audio + input polish.

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

## Running (milestone 3)
`nintendogs_pc --data extracted` (Windows build from CI artifact `nintendogs-pc-windows`). It maps the DS memory
map at the real addresses (`port/src/platform_mem.c`, I/O region as plain RAM for now), loads main/ITCM/DTCM from
`extracted/rom`, rewrites code pointers in game data to the compiled C (`port/src/loader.c` + generated
`host_functab` from `port/tools/gen_functab.py`; `extracted/host/` tables from extract_rom.py), replays the ROM's
startup and calls the game's main(). Overlays load through FS_LoadOverlay (src hook -> `ndogs_load_overlay`).
`NDOGS_WATCHDOG=<s>` prints the main thread's backtrace after <s> seconds; resolve it with the `.map` artifact.
Status: the game runs its main loop at 60 fps (3D command stream and sound commands every frame); nothing is
drawn yet (milestone 5).

## Hardware layer (milestone 4)
- `port/src/hw_trap.c`: DS register pages (0x04000000-0x04001fff, 0x04100000) are inaccessible; a fault gives
  the address and direction, the model refreshes/handles the register, the instruction re-runs single-stepped.
  Host code uses `HostIoRead/Write*` (same model, no fault). gdb can't be used past the first trap: use the
  crash/watchdog backtraces (`port/tools/addr2func.py <map>`), `NDOGS_STATUS=1`, `NDOGS_IOLOG=1/2`, `NDOGS_PEEK`.
- `port/src/hw.c`: register model (IPC sync/FIFO, IME/IE/IF, divider/sqrt, DMA immediate/card/3D-FIFO, timers,
  keys, GXSTAT), ARM7 stand-in over PXI (SPI devices 4/6/8/9, RTC 5 with host time, sound 7 acknowledged),
  interrupt delivery at CPSR unmask / halt / register writes, IRQ-exit thread reschedule, 60 Hz VBlank (paced by the game, see M5).
  `hw_gx_command()` is the 3D command sink for the renderer.
- `port/src/hw_card.c`: game card reads from the user's ROM (path in extracted/host/rom_path.txt).
- `port/src/host_threads.c`: SDK threads as host fibers (OS_Save/LoadContext, OS_InitContext hook).
- Porting hazards found and fixed: adjacent locals used as one struct (func_02086d58/02086e68), a printf
  argument supplied through a stack slot (func_0208c5b4); more of these likely exist.

## Video (milestone 5)
- `port/src/window.c`: one window, top screen above bottom (Win32 GDI, user32/gdi32 loaded at run time so they
  don't land in the DS address range; Linux is headless for now). Mouse on the bottom screen = stylus; arrows,
  Z=A, X=B, A=Y, S=X, Q=L, W=R, Enter=Start, Shift=Select. `NDOGS_HEADLESS=1` runs without a window.
- `port/src/vram.c`: VRAM banks A-I and VRAMCNT. CPU-visible mappings live at their DS address; hidden roles
  (textures, palettes, extended palettes) in bank storage; a remap moves the contents.
- `port/src/gpu2d.c` (both 2D engines) and `port/src/gpu3d.c` (geometry + rasteriser), written from GBATEK.
- `port/src/video.c`: at VBlank the swapped 3D list is rasterised; at "line 0" (after the game's VBlank work) both
  engines are drawn from a register snapshot, display capture is applied (the game shows 3D on both screens by
  swapping screens every frame and capturing), and the frame is presented.
- Frame pacing (`hw.c`): VBlank comes when the game halts for it (never mid-frame: screen swap and 3D swap are in
  step), on a fixed 60 Hz schedule. The game logic advances once per VBlank as on the DS, so this is real speed
  (each screen shows 30 fps, the game alternates them); after a slow frame the next ones run back to back until
  the schedule is caught up, so game speed follows the clock, not the frame rate.
- Debugging: `NDOGS_SHOT=<frame,...>` saves shot_<frame>.bmp, `NDOGS_VLOG=1` logs per-frame render times and
  display registers, `NDOGS_WATCH=<reg>` logs who writes a register and when.
- Optimisation: the renderers build at -O2; game code at -O0 (`NDOGS_OPT`): at -O1 some decompiled C relies on
  undefined behaviour (a file open fails, then func_0208b15c passes NULL to func_02095298).
- Performance: a trapped register access costs ~17 us (two exceptions), so the hottest register users
  (IME/IE, divider/sqrt, DMA status, card, GX FIFO and matrix registers, PXI) have `#ifdef NDOGS_PC` versions using
  `HostIo*` (declared in host_compat.h). Those go through an always-accessible alias of the register memory
  (`io_reg()`, platform_mem.h) and cost ~0.01 us. To find new hot spots: `NDOGS_TRAPPROF=<n>` (+`NDOGS_PROFFROM=<frame>`)
  prints the busiest trapping instructions, resolve with addr2func.py; `NDOGS_STATUS=2` prints per-frame busy time
  and traps; `NDOGS_TRAPBENCH=1` times one trap and one host access.
- 3D: textures/palettes are copied once per frame into flat arrays (vram_flatten) for the rasteriser.
- 3D debugging: `NDOGS_GXSTAT=1` prints per-second polygon fates (drawn/clipped/culled) and command counts;
  `NDOGS_GXDUMP=<swap>` traces one frame's matrix commands, POLYGON_ATTR/TEXIMAGE writes with host call chains,
  and its polygon list. `NDOGS_HWWATCH=<addr>` (Windows) logs every write to a RAM word with the writer's chain.
- Timers (`hw.c`): the four DS timers run on the game clock (VBlanks plus time into the frame, minus host work
  such as drawing and audio), with prescaler, reload, cascade and overflow IRQs; the SDK's OS_GetTick and alarms
  depend on them. `NDOGS_JANK=1` logs per-frame present intervals and 3D/screen swap pairing.
- Scripted input for headless tests: `NDOGS_TOUCH="<frame>,<x>,<y>,<frames>;..."` (stylus on the bottom screen),
  `NDOGS_KEYS="<frame>,<mask>,<frames>;..."` (KEYINPUT bits). Reaching the demo's menu and gameplay:
  `NDOGS_TOUCH="900,128,100,6;940,128,100,6;980,128,100,6;1300,207,166,6;1500,207,166,6;1700,115,131,6"`
  (knock x3, two "proceed" arrows, Look) with `NDOGS_SHOT=1600,2400`.
Status: boots, knocks, reaches the Kennel menu (Buy/Look) and the Home gameplay screen with the three dogs at 60 fps
(~10 ms per frame).

Porting hazard checks done: a UB-trap build (`NDOGS_CFLAGS="-fsanitize=shift-exponent,integer-divide-by-zero
-fsanitize-trap=all"`, separate objdir; hw_trap.c logs each trap site once and continues) found one shift >= 32
in 90 s of gameplay (func_0209b940, fixed). Checked and fine: char signedness (mwcc -char signed), 64-bit struct
alignment, the game-logic inline-asm replacements, FX_Div/FX_Inv host routines.

Differential testing against the original ARM code (Unicorn: `pip install unicorn`):
- `port/tools/armrun.py` runs ARM9 code from the extracted ROM (main RAM/ITCM/DTCM, divider and sqrt modelled).
- `port/tools/difftest.py` compares every pure host routine (args in, value out) with the original on random and
  edge inputs. All 21 match (compiler runtime incl. soft float, fixed-point helpers).
- Capture/replay for routines working on game state: build with `NDOGS_WRAP=func_a,func_b,...` (linker --wrap,
  separate objdir) and run with `NDOGS_CAPTURE=func_a@<call>,...`: the wrapper dumps DS memory, the caller frames and
  the arguments before/after that call (cap_*.bin) and traces wrapped calls made meanwhile (cap_*.trace, with the
  first argument's object). `port/tools/capdiff.py cap_*.bin` replays on the ARM code from the same memory and
  diffs everything written (host code pointers in game data become ARM jumps via the .map); `--trace` finds the
  first call where the traced object diverges. The dog animation ITCM routines and their helpers match; the per-dog
  update (func_01ffb34c) matches in 32 of 34 replays (one known divergence in func_02038b10's dog loop).
- Found this way: func_0204cc24 read r4 left by its caller (the dog) through an uninitialised `unaff_r4` (fixed);
  narrow return types: on the DS a callee's code decides how r0 is extended and callers use r0 as is, on x86 the
  caller's prototype decides. `port/tools/retscan.py` lists definitions vs mismatching caller declarations and
  `port/tools/retfix.py` gives those callers the definition's type under #ifdef NDOGS_PC (201 declarations in 185
  files, e.g. FX_Atan2Idx (func_02068078) declared s16 made half of all directions negative and broke turning).

Porting hazard: pass-through register arguments. A function that forwards r1-r3 untouched to a callee was
sometimes decompiled with too few parameters (matching on the DS, garbage on the PC). func_0208f7d0 dropped the
light flags for func_02077e04, which set the dogs' material to depth-test-equal and hid them. `port/tools/argscan.py`
finds these from the original asm (registers read before written, or forwarded to a callee that uses them); all
10 more it found are fixed (the dog material colour setters among them). What it still lists was reviewed and is
harmless: func_0204160c/02062700/020627c0/020636d0 take a 64-bit argument (two registers), and
func_0208cc2c/0208cdfc/0208db60/020fdb24 leave don't-care registers for callees called in loops.

## Audio and input (milestone 6)
- `port/src/snd.c`: the ARM7 sound driver stand-in (commands from PXI channel 7 via hw.c): 16 players with SSEQ
  sequence interpretation, SBNK instruments (PCM/PSG/noise/drum set/key split), SWAR waves (PCM8/16, IMA-ADPCM),
  SDK-style envelopes, decibel volume mix, LFO/portamento/sweep, 16 voices mixed at 32768 Hz, SNDSharedWork
  status/variables. Written from GBATEK and the SDAT format descriptions. `port/src/snd_selftest.c` (not part of
  the game build) renders a synthetic bank/sequence to a WAV and checks pitches, status and parameters.
- `port/src/audio.c`: host output, pushed once per VBlank (1/60 s of game time): waveOut on Windows, ALSA via
  dlopen on Linux; +6 dB output gain (`NDOGS_VOLUME=<percent>`), `NDOGS_WAV=<file>` records, `NDOGS_MUTE=1`.
- `port/src/window.c`: resizable window (aspect kept, F11 full screen on Windows), XInput gamepads, X11 window on
  Linux via dlopen (not yet run on Linux). `NDOGS_SNDLOG=1` logs sound commands.
- Internal resolution (`NDOGS_SCALE=1-4`, default 2, F8 cycles): gpu3d rasterises at 256S x 192S in parallel
  scanline bands (`port/src/workers.c`, `NDOGS_THREADS`), plus a downsampled native frame for the 2D engines.
  gpu2d records each pixel's source (`gpu2d_src`: unchanged 3D, or a 1:1 direct-colour bitmap pixel with its VRAM
  address); video.c composes the screens at S x by taking those pixels from the hi-res 3D frame or from a hi-res
  copy of the display capture that wrote that VRAM (so the second screen's 3D is sharp too). Everything else is
  the native pixel enlarged. 2x costs ~2 ms per frame, 3x ~6 ms (at the limit on an 8-thread CPU).
- Window: F9 stacked / side by side (`NDOGS_LAYOUT`), F10 keep aspect / stretch / whole multiples
  (`NDOGS_ASPECT`), F11 full screen, F7 bilinear / nearest scaling.
- Presenting (Windows): an OpenGL 1.1 presenter thread (opengl32.dll loaded at run time) uploads each finished
  canvas as a texture and swaps with vsync; the game thread only copies the frame, so the monitor never delays
  the game. `NDOGS_GL=0` uses GDI instead, `NDOGS_VSYNC=0` turns vsync off. Linux still uses XPutImage.
- Not done: microphone (the full game's name calling), direct channel/stream sound commands (unused so far).

## Build
```
cmake -S . -B build -A Win32                 # Windows (Visual Studio)
cmake -S . -B build -DCMAKE_C_FLAGS=-m32     # Linux (needs gcc-multilib)
cmake --build build
```
The CMake target is a memory-map self-test. The game itself: `python port/tools/host_compile.py --link build/objs`
(CI artifacts `nintendogs-pc-linux` / `nintendogs-pc-windows`), run as `nintendogs_pc --data extracted`.
