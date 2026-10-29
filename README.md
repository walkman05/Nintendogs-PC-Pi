# Nintendogs decompilation

A matching decompilation of the C code of **Nintendogs** (Nintendo DS, 2005), made from the European kiosk demo.

| Version | ROM (`.nds`) | MD5 | Status |
|---|---|---|---|
| `Nintendogs (Europe) (En,Fr,De,Es,It) (Demo) (Kiosk)` | the in-store demo | `a190eca6a61b3c69ce48f076d2445eb8` | 8,310 of 8,311 functions of C match |

The ROM is not included: bring your own.

## Credits

- **cheemz**: the original decompilation, which got this project to about 99%.

## Status

- **8,310 of 8,311 functions** compile to exactly the original instructions, in the main ARM9 program and the
  overlays. This includes the library code linked into the game (NitroSDK, NitroSystem and an SHA-1
  implementation, at 0x0205e400-0x020b0000).
- **1 function** is not matched yet, down to register allocation: listed in `remaining.txt`. Its file
  (`src/arm9/func_0200a648.c`) is equivalent non-matching C, marked `nonmatching` in `functions.csv`.
- **58 functions** are not C (hand-written assembly and the compiler's runtime routines) and are left out; also
  listed in `remaining.txt`.
- **66 matches** use a little inline assembly for instructions C can't produce (coprocessor and cache
  operations, and the like); `functions.csv` marks them.
- **Names:** functions keep their addresses (`func_02000afc`, `func_ov019_0215d3a0` for overlay 19); naming hasn't
  started.
- This tree is the C only: there is no linker setup yet, so it doesn't rebuild the ROM.

## Layout

| Path | What |
|---|---|
| `src/arm9/` | the main program, one file per function |
| `src/overlays/ovNNN/` | each overlay's functions |
| `include/ctx.h` | the basic types every file includes |
| `include/nitro.h` | NitroSDK and NitroSystem types, each layout checked against matched code |
| `functions.csv` | every function: file, module, address, compiler build, inline asm |
| `remaining.txt` | the functions not matched yet, and the ones that aren't C |

## Compiler

Metrowerks CodeWarrior for Nintendo DS (`mwccarm`), version 1.2 (base); 1,245 functions match only with build
1.2/b56 (`functions.csv` says which). Flags:

```
-proc arm946e -O4,p -interworking -enum int -char signed
```

Some files start with a `#pragma` the original build used for them (`#pragma avoid_byte strb`, optimizer toggles).
Each function was checked by compiling its file alone and comparing the result instruction by instruction with the
original, with references to other functions and data compared by symbol.

## PC port: building it yourself

The same C also builds as a native program for Windows and the Raspberry Pi (`port/`). Ready-made test builds are
on the Releases page; this is how to make your own. It runs the retail game, *Nintendogs: Dachshund & Friends*.

**You need:**
- two ROMs of your own: the **European kiosk demo** (MD5 above; the code was decompiled from it) and
  **Nintendogs: Dachshund & Friends (Europe)** (a full 64 MB dump);
- **Python 3**;
- **dsd 0.12.1**, the ds-decomp tool: download `dsd-windows-x86_64.exe` (or `dsd-linux-x86_64`) from
  https://github.com/AetiasHax/ds-decomp/releases/tag/v0.12.1.

### Windows

1. Install [MSYS2](https://www.msys2.org), open its **MINGW32** shell and install the compiler:
   `pacman -S mingw-w64-i686-gcc mingw-w64-i686-python`
2. In this folder, prepare the game data from your ROMs (it goes into `extracted/`, which git ignores):
   ```
   python port/tools/extract_rom.py --rom <demo.nds> --dsd <path to dsd.exe>
   python port/tools/retail_match.py --rom <dachshund.nds> --dsd <path to dsd.exe>
   ```
3. Build (a few minutes):
   ```
   python port/tools/host_compile.py --cc gcc --link build/objs --exe nintendogs_pc.exe --target retail
   ```
4. Run `build\objs\nintendogs_pc.exe` (double-click works).

### Raspberry Pi

The game needs a **32-bit** program: Raspberry Pi OS Bookworm on a Pi 2, 3 or 4 (or a Pi 5 running the 32-bit
OS). dsd doesn't run on the Pi, so do the preparation step on a PC.

1. On a Windows or Linux PC, do steps 1-2 above (on Linux: `python3` and `dsd-linux-x86_64`), then copy this
   whole folder, including `extracted/`, to the Pi.
2. On the Pi, install the compiler:
   - 32-bit Raspberry Pi OS: `sudo apt install gcc python3`
   - 64-bit Raspberry Pi OS: `sudo apt install gcc-arm-linux-gnueabihf libc6-dev-armhf-cross python3`, and for
     running it `sudo dpkg --add-architecture armhf && sudo apt update && sudo apt install libc6:armhf libx11-6:armhf libasound2:armhf`
3. Build (this takes a while on a Pi). On 64-bit Pi OS use `--cc arm-linux-gnueabihf-gcc` instead of `--cc gcc`:
   ```
   python3 port/tools/host_compile.py --cc gcc --link build/objs --exe nintendogs_pc --target retail
   ```
4. Put your Dachshund & Friends ROM in this folder and, from this folder, run `./build/objs/nintendogs_pc`. (The
   ROM's path recorded on the PC doesn't exist on the Pi; the program then uses the ROM it finds here.)

You can also build the Pi program on Windows with Arm's cross-compiler (Arm GNU Toolchain 12.2.rel1,
`arm-none-linux-gnueabihf`): step 3 with `--cc arm-none-linux-gnueabihf-gcc` and `--link build/objs_pi`.

`CLAUDE.md` has the details: how the port works, debugging switches, and what is still open.

Not affiliated with or endorsed by Nintendo. No game assets or ROM data are included.
