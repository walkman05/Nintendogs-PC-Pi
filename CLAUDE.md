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
Without `--data` (e.g. double-clicked) it looks for `extracted/` in the current folder, next to the exe and its parent folders, then the source tree it was built from (`NDOGS_DEFAULT_DATA`), and shows a message box if none loads. On Windows, loading user32 maps a ~20 MB system section at a random low address that usually covers the DS map (any launch outside MSYS bash); the exe then starts itself again suspended with the DS ranges reserved (`NDOGS_RESERVED`, `platform_relaunch_reserved`).
First-run setup (`port/src/setup.c`, so a shared exe needs no Python or dsd): when no data folder is found, or it
has no `host/full.nds`, the exe looks for the ROMs (by header game code: demo A36P, checked by CRC32 because the
tables below only fit it; Dachshund & Friends ADGP) on the command line, next to the exe and in the current folder,
and writes `<exe dir>/extracted`: the ARM9 binary (BLZ-decompressed), ITCM/DTCM from its autoload table, the
overlays, the header, rom_path.txt, and `full.nds` as make_full_rom.py builds it (byte-identical; keep the two in
step). modules.txt and fix_*.txt come from dsd's code analysis, so `port/tools/gen_hosttabs.py` compiles them into
the exe at build time (empty without extracted/, e.g. CI: such an exe can't set up). Without full.nds the game runs
as the kiosk demo. `NDOGS_NODEFAULT=1` ignores the build tree's data (to test a fresh setup), `NDOGS_NOSETUP=1`
skips setup. To share a test build: `strip -s` the exe (debug info is ~11 MB of the ~15 MB) and zip it with a
readme; never include extracted/.
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
  update (func_01ffb34c) matches in all replays.
- Found this way: func_0204cc24 read r4 left by its caller (the dog) through an uninitialised `unaff_r4` (fixed);
  narrow return types: on the DS a callee's code decides how r0 is extended and callers use r0 as is, on x86 the
  caller's prototype decides. `port/tools/retscan.py` lists definitions vs mismatching caller declarations and
  `port/tools/retfix.py` gives those callers the definition's type under #ifdef NDOGS_PC (201 declarations in 185
  files, e.g. FX_Atan2Idx (func_02068078) declared s16 made half of all directions negative and broke turning).
  Same cause one step further: a caller that stores such a result in a narrow local keeps the callee's value on
  the DS (the register isn't re-extended), so those locals are widened under NDOGS_PC (the 11 s16 locals that
  receive FX_Atan2Idx: dog chase velocity, other-dog loop, turning helpers). `port/tools/retlocals.py` does this for
  every mismatched function: it finds narrow locals receiving the result, checks the original ARM code after the
  call for re-extension, and (`--apply`) widens the ones the DS kept unextended, unless the variable has other
  writes or its address is taken (listed as REVIEW): 80 files widened, plus 7 where the declaration sits inside a
  multi-line macro (func_020cac38/cae5c/cb080/cb7f4/cbc3c/cc084/cc4cc: an NDOGS_RECV16 type alias). The 8 REVIEW
  sites were checked by hand: their callees return values that fit (0/1 flags, a u16 global, an atan2 angle in u16).
- Narrow parameters, the mirror image: a function defined with an s8/u8/s16/u16 parameter that callers declare
  (and pass) as 32 bits. On x86 the callee narrows; on the DS it narrows only if its code does.
  `port/tools/paramscan.py` checks each such parameter (r0-r3) in the original callee: if it uses the full register
  (arithmetic, compare, logic, addressing; byte/halfword stores don't count) the parameter is widened under
  NDOGS_PC (`--apply`): 139 parameters in 108 files.
- Captures include the I/O register page (e.g. POWCNT, which the game reads for the screen swap) as replay input;
  capdiff aligns traces (ARM traces also show recursion and helpers the host calls directly) and resumes at the
  ARM original when execution enters host code (a code hook over the host range: host function addresses are not
  word aligned, so planted ARM jumps misfired). Bytes written asynchronously meanwhile (OS tick counter, the ARM7
  stand-in's touch/keys at 0x027fffa8) are reported as ignored. Capture wrappers forward 12 arguments (an 8-argument
  wrapper gave func_01ffd8f8 a garbage 9th one) and record 12; captures nest (one file each; a call that never
  returns, like a scene loop, leaves an incomplete file that capdiff reports); a first argument that is a number
  (not a DS RAM pointer) is traced as zeros. Tracing is opt-in (`NDOGS_CAPTRACE=1`). `NDOGS_WRAP=@<file>` and
  `NDOGS_CAPTURE=@<file>` take long lists from a file (wrapping all ~8,350 C routines works: the link flags go
  through the response file); the caller-frame window is clamped to the stack region (fiber stacks are small). Host function pointers stored in game data count as equal to
  the ARM original. armrun.py models ARMv5 unaligned LDR/STR (rotated word / aligned store; Unicorn does ARMv6+
  whatever the CPU model) in the 23 SDK string/printf functions that rely on it. In the whistle/jump-at-camera
  scenario all 50 replays of the dog update (calls 2500-14750, every 250th) match, and so do all 221 replays of
  the 214 routines in the wrapped set (`NDOGS_CAPTURE=<each>@300,<each>@2000`).
- Whole-game sweep: every C routine wrapped (8,354; a scalar/pointer return and at most 12 argument words), each
  captured at its 3rd and 40th call (2,699 captures, ~23 GB) and at its 300th (511): 2,533 + 487 match. Replays
  enter Thumb routines in Thumb state, and armrun models DMA (immediate transfers run, other modes and transfers
  already running complete at once). The rest were all explained: async writes from IRQ handlers taken when the
  host re-enables interrupts (flags at 0x0214e6ba/0x02150e88/0x02150f30/0x02155c44), SNDSharedWork (0x021561e0,
  the sound stand-in), OS_GetTick, GX matrix readback registers, card ROM transfers and IRQ/halt waits (not
  modelled), OS_SaveContext/LoadContext (host fibers), stack addresses stored in frames, uninitialised padding.

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

## Full-game mode
The kiosk demo is the full game with gates in front: the code for the shops, walks, hotel, remodelling, training
and contests is in the ROM, but most text was blanked out and a few files left out. `ndogs_full` (port/src/main.c,
default on; `--demo` restores the kiosk) lifts the gates behind `#ifdef NDOGS_PC` + `if (ndogs_full)`.
- Data: `python port/tools/make_full_rom.py --demo baserom.nds --retail <Dachshund & Friends (E).nds>` builds
  `extracted/host/full.nds` (+ `full_header.bin`): the demo ROM with every message file rebuilt in the demo's layout
  with the retail text by message ID (message ends found by walking 0x1a control codes, which can hold zero
  halfwords) and the retail-only messages inserted in ID order (MID1 is sorted; e.g. the Dalmatian/Boxer/Husky
  breed names), the retail-only files appended, and every other differing English or language-free file taken
  from retail (the demo's UI graphics grey out Buy and shop/menu entries) except the sound archive, the build
  stamps and archives missing an entry name the demo uses (some Scene/ ones); other languages stay demo (space). The demo's directory/file order (file IDs) is kept and the image
  stays within 64 MB. The port reads card and header from it in full-game mode when present.
- Kennel: Buy (func_0211c69c button 0, dropped in the demo) opens the kennel shop through the kept state-4 path;
  func_0211c964 no longer greys it out. func_0208b604 borrows a similar breed's shop icon if one is missing.
- func_02099ffc: no kiosk reset (NIS_ResetTrial) after 2 minutes without input.
- Microphone (needed to name a puppy): port/src/mic.c serves PXI channel 9 (hw.c collects each command's words).
  Auto sampling (0x41: type bits 0 12-bit, 1 signed, 2 filter off, 4 loop; buffer, size, rate in 33.514 MHz cycles)
  fills the game's ring buffer each VBlank with rate/60 samples and keeps the newest sample's address at
  0x027fff90; one-shot (0x30) writes 0x027fff94. Source: the default recording device (Windows waveIn via winmm at
  run time; silence elsewhere for now), NDOGS_MICWAV=<16-bit mono .wav>,<frame> for tests, NDOGS_NOMIC=1 none.
- Mode values (`NDOGS_PTRWATCH=0215942c` logs changes): c000 door/title, 0xd kennel, 0x12 kennel Look (demo),
  0 Home, 17 end-of-demo scene (entered via func_020a8868 event 22 -> func_020a7d74; still open).
- Save: the demo has no backup-memory code; the retail game's (CARD backup calls around 0x02096000-0x02097800 in
  its arm9) reads/writes the blocks of the layout table data_0212f5c0 (Registory, Dog, SoundCol, Voice, HotelDog,
  HotelSC, Friend), each twice (copy B 0x20000 higher). port/src/save.c stands in for the chip with
  <data>/save/nintendogs.sav at chip addresses: func_02096880 reads both copies, func_02097608 (the write task;
  entries {chip address, copy A, copy B, length, desc}) writes where retail starts each card write.

## Retail retarget (in progress)
Goal: run on the retail Dachshund & Friends (E) cartridge alone (the demo ROM no longer needed). The retail
build is nearly the same code at other addresses. `extracted/retail/` holds its `dsd rom extract` (rom/) and
`dsd init` (config/) output. `port/tools/retail_match.py` pairs functions by their bytes with dsd's relocations
masked (unique exact matches as anchors, repeated code by order between anchors, the rest by order + similarity),
then data addresses through the literal-pool words of matched code; output `extracted/retail/match.txt` and
`data_map.txt` (git-ignored). Status: of 8,647 demo functions 6,533 exact + 2,012 same code placed by order,
56 changed (similar), 46 with no counterpart (41 main, 5 itcm), 27 retail-only in main; calls in matched code
agree (1 mismatch to check). Data: 3,386 addresses paired, no conflicts; of the 2,503 data symbols the C uses,
2,441 map directly, 33 lie within 0x100 of a mapped address, 26 others (some are constants, not addresses).
Data names are resolved per module (overlays share addresses): direct pairs, then the n-th loaded address of
changed functions with the same load count, then an offset within 0x100 of a pair (bytes checked), an address
inside a same-code function (jump tables), or the demo's bytes found once in retail with pointer-sized words
ignored. All 2,540 names resolve except 3 used only by the kiosk reset (func_02088e48), kept at their demo address.
`python port/tools/retail_match.py --rom <retail.nds>` (runs dsd if extracted/retail/config is missing) writes
`extracted/retail/symbols.txt` and `extracted/retail/host/` (loader tables, header, rom_path), making
extracted/retail a data folder for the retail build:
`python port/tools/host_compile.py ... --target retail --exe nintendogs_pc_retail.exe` links the same game objects
with retail data addresses (datasyms_retail.ld), a function table at retail addresses (gen_functab.py --map), the
retail loader tables for setup (gen_hosttabs.py <folder>) and port files built with NDOGS_RETAIL (setup from the
retail ROM alone; full-game mode without full.nds). Layout constants that differ per build come from modules.txt:
the overlay base and the arena starts (loader.c `ndogs_ov_base/arena_*`, used by func_020801d8); hw.c's thread
info goes through data_02155cf8. The code that runs is still the demo's C (with the full-game patches) on the
retail data, so the changed functions are where to look when something breaks.
Status: the retail exe runs from the Dachshund & Friends ROM alone (first-run setup included): door, kennel,
breed list (Husky), buying, Home, saving, as on the demo build. Fixed on the way: func_020a457c called the
trampoline func_0208ddac without the r0 it forwards (func_0208c3e0's result, the system.ini text): the demo build
only worked because the stack slot happened to hold it.
Sound: the retail sound archive drops demo sequence 58 (later sequences are one lower) and adds sound groups
(e.g. 80-84 / 85-89 variants by a value system.ini sets at data_02159010+4; other groups +8). Shared code takes
its numbers from tables in the (retail) data and is right; functions retail changed carry demo numbers as
constants. Those pick the retail value at run time: `NDOGS_R(retail, demo)` (host_compat.h; `ndogs_retail` is set
in the retail exe's main.c; ctx.h gives the DS build the demo value, so it stays identical) in func_0209af44,
func_0209b18c (+ retail's case 29), func_0209c5b0, func_0209d430, func_0209d5bc, func_0209e534, func_0209eae4,
func_020b42c0, func_020b9d98, func_ov001_0215c8fc. Found by diffing the immediates of every equal-size "similar"
function (objdump of both builds) and the sound calls with literal numbers in the changed ones. Check: the door
music is sample-identical to the demo build's; NDOGS_SNDLOG=2 logs notes and dumps each prepared sequence/bank.
Next: decompile the changed / retail-only functions (the save code among them); drop the demo gates in a retail
build.

## Raspberry Pi (32-bit ARM Linux)
Cross-built on Windows with Arm GNU Toolchain 12.2.rel1 (mingw-w64-i686 host, arm-none-linux-gnueabihf; glibc 2.36,
as Raspberry Pi OS Bookworm; kept in H:/nintendogs/tools/armgcc, outside the repo):
`python port/tools/host_compile.py --cc arm-none-linux-gnueabihf-gcc --link <objdir> --exe nintendogs_pc --target retail`.
An ARM compiler gets `-marm -fsigned-char` instead of `-m32` (ARM code for the register trap and frame-pointer
backtraces; char signed as on the DS) and ARM branches in the veneers. 32-bit is required (the game assumes 32-bit
pointers): 64-bit Pi OS needs armhf multiarch libraries, and a Pi 5 on its default 16 KB-page kernel can't run it.
- Register trap (hw_trap.c, `__arm__`): no user single-step on ARM, so on the fault the page is opened and the
  instruction after the faulting one (ARM or Thumb; 16/32-bit Thumb by the first halfword) is replaced by UDF;
  the access re-runs natively, SIGILL at the UDF restores it, finishes the write and closes the page. Code pages
  are made writable on demand. The write bit comes from the fault status (error_code bit 11).
- main.c's Linux crash/watchdog backtraces walk ARM frames (fp -> saved lr, fp[-1] -> caller's fp).
- mic.c: ALSA capture (dlopen, non-blocking, mono S16 at the game's rate).
- audio.c: audio_underruns/drops were defined only in the Windows branch (any Linux link failed).
Status: compiles and links (8,311 files) to an ELF32 hard-float binary needing only libc.so.6 (GLIBC_2.34);
not run yet (no ARM machine or emulator here; Docker Desktop doesn't start on this PC: AF_UNIX socket files
become inaccessible).

## Build
```
cmake -S . -B build -A Win32                 # Windows (Visual Studio)
cmake -S . -B build -DCMAKE_C_FLAGS=-m32     # Linux (needs gcc-multilib)
cmake --build build
```
The CMake target is a memory-map self-test. The game itself: `python port/tools/host_compile.py --link build/objs`
(CI artifacts `nintendogs-pc-linux` / `nintendogs-pc-windows`), run as `nintendogs_pc --data extracted`.
