"""Milestone 1: extract the game's code, overlays, files and symbol/relocation data from the user's own ROM.

Runs AetiasHax/ds-decomp (`dsd`, MIT) and writes everything under extracted/ (git-ignored, never committed):
  extracted/rom/      dsd rom extract: arm9.bin, itcm/dtcm, arm7.bin, decompressed overlays, game files
  extracted/config/   dsd init: symbols.txt, relocs.txt, delinks.txt per module
  extracted/data_symbols.txt   every data_/bss_ symbol the decompiled C references, with its address and module

usage: python port/tools/extract_rom.py --rom <baserom.nds> [--dsd <path to dsd executable>]
"""
import argparse, hashlib, os, re, shutil, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXPECTED_MD5 = 'a190eca6a61b3c69ce48f076d2445eb8'  # Nintendogs (Europe) (Demo) (Kiosk)
SYM_RE = re.compile(r'\b(?:data|bss)_(?:ov[0-9]+_)?[0-9a-f]{8}\b')


def md5(path):
    h = hashlib.md5()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def find_dsd(explicit):
    for c in [explicit, os.environ.get('DSD'), shutil.which('dsd'), shutil.which('dsd.exe')]:
        if c and os.path.isfile(c):
            return c
    sys.exit('dsd not found: download it from https://github.com/AetiasHax/ds-decomp/releases and pass --dsd')


def run(cmd):
    print('+', ' '.join(cmd), flush=True)
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        sys.stderr.write(r.stdout + r.stderr)
        sys.exit('command failed: %s' % cmd[1])


def referenced_symbols():
    used = set()
    for dirpath, _, files in os.walk(os.path.join(ROOT, 'src')):
        for name in files:
            if name.endswith('.c'):
                with open(os.path.join(dirpath, name), encoding='utf-8', errors='replace') as f:
                    used.update(SYM_RE.findall(f.read()))
    return used


def dsd_symbols(config_dir):
    syms = {}
    for dirpath, _, files in os.walk(config_dir):
        if 'symbols.txt' in files:
            module = os.path.relpath(dirpath, config_dir).replace(os.sep, '/')
            with open(os.path.join(dirpath, 'symbols.txt')) as f:
                for line in f:
                    parts = line.split()
                    addr = next((p[5:] for p in parts if p.startswith('addr:')), None)
                    if parts and addr:
                        syms.setdefault(parts[0], (addr, module))
    return syms


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--rom', required=True)
    ap.add_argument('--dsd')
    ap.add_argument('--out', default=os.path.join(ROOT, 'extracted'))
    args = ap.parse_args()

    digest = md5(args.rom)
    if digest != EXPECTED_MD5:
        sys.exit('ROM MD5 is %s, expected %s (the European kiosk demo)' % (digest, EXPECTED_MD5))
    dsd = find_dsd(args.dsd)
    rom_dir, config_dir = os.path.join(args.out, 'rom'), os.path.join(args.out, 'config')
    run([dsd, 'rom', 'extract', '-r', args.rom, '-o', rom_dir])
    run([dsd, 'init', '-r', os.path.join(rom_dir, 'config.yaml'), '-o', config_dir,
         '-b', os.path.join(args.out, 'dsd-build')])

    used, syms = referenced_symbols(), dsd_symbols(config_dir)
    missing = sorted(s for s in used if s not in syms)
    with open(os.path.join(args.out, 'data_symbols.txt'), 'w') as f:
        for s in sorted(used):
            if s in syms:
                f.write('%s %s %s\n' % (s, syms[s][0], syms[s][1]))
            else:
                # dsd has no symbol there (hardware register, system area, mid-object label): the name is the address
                f.write('%s 0x%s from-name\n' % (s, s[-8:]))
    print('data symbols referenced by the C: %d, located: %d, from name only: %d' % (len(used), len(used) - len(missing),
                                                                             len(missing)))
    for s in missing:
        print('  from name:', s)


if __name__ == '__main__':
    main()
