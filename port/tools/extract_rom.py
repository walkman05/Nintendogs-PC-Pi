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


def sections(path):
    out = {}
    for line in open(path):
        m = re.match(r'\s*(\.\w+)\s+start:0x([0-9a-f]+)\s+end:0x([0-9a-f]+)', line)
        if m:
            out[m.group(1)] = (int(m.group(2), 16), int(m.group(3), 16))
        elif line.strip() and not line.startswith(' '):
            break
    return out


def write_host_tables(out, rom_dir, config_dir):
    """extracted/host/: what the PC loader needs.
    modules.txt  one line per module: name file base bss_start bss_end ctor_start ctor_end
    fix_<module>.txt  'from to target_module' for every word in a module's data that points at code
    (function-pointer tables), which the loader rewrites to the compiled C functions."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import yaml_lite
    host = os.path.join(out, 'host')
    os.makedirs(host, exist_ok=True)
    mods = []
    arm9 = yaml_lite.load(os.path.join(rom_dir, 'arm9', 'arm9.yaml'))
    mods.append(('main', 'arm9/arm9.bin', int(arm9['base_address']), int(arm9['bss_start']), int(arm9['bss_end']), 0, 0,
                 os.path.join(config_dir, 'arm9')))
    for tcm in ('itcm', 'dtcm'):
        y = yaml_lite.load(os.path.join(rom_dir, 'arm9', tcm + '.yaml'))
        mods.append((tcm, 'arm9/%s.bin' % tcm, int(y['base_address']), int(y.get('bss_start', 0) or 0),
                     int(y.get('bss_end', 0) or 0), 0, 0, os.path.join(config_dir, 'arm9', tcm)))
    for ov in yaml_lite.load_list(os.path.join(rom_dir, 'arm9_overlays', 'overlays.yaml'), 'overlays'):
        base, size, bss = int(ov['base_address']), int(ov['code_size']), int(ov['bss_size'])
        mods.append(('ov%03d' % int(ov['id']), 'arm9_overlays/' + ov['file_name'], base, base + size, base + size + bss,
                     int(ov['ctor_start']), int(ov['ctor_end']),
                     os.path.join(config_dir, 'arm9', 'overlays', 'ov%03d' % int(ov['id']))))
    with open(os.path.join(host, 'modules.txt'), 'w') as f:
        for name, fn, base, b0, b1, c0, c1, _ in mods:
            f.write('%s %s 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n' % (name, fn, base, b0, b1, c0, c1))
    total = 0
    for name, fn, base, b0, b1, c0, c1, cfg in mods:
        secs = sections(os.path.join(cfg, 'delinks.txt')) if os.path.exists(os.path.join(cfg, 'delinks.txt')) else {}
        text = secs.get('.text', (0, 0))
        n = 0
        with open(os.path.join(host, 'fix_%s.txt' % name), 'w') as f:
            rp = os.path.join(cfg, 'relocs.txt')
            if os.path.exists(rp):
                for line in open(rp):
                    m = re.match(r'from:0x([0-9a-f]+) kind:load to:0x([0-9a-f]+) module:(\S+)', line)
                    if not m:
                        continue
                    frm, to = int(m.group(1), 16), int(m.group(2), 16)
                    if text[0] <= frm < text[1]:
                        continue          # literal pools inside code: the C has its own references
                    f.write('0x%08x 0x%08x %s\n' % (frm, to, m.group(3)))
                    n += 1
        total += n
    print('host tables: %d modules, %d data pointers to fix up' % (len(mods), total))


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
    write_host_tables(args.out, rom_dir, config_dir)
    print('data symbols referenced by the C: %d, located: %d, from name only: %d' % (len(used), len(used) - len(missing),
                                                                             len(missing)))
    for s in missing:
        print('  from name:', s)


if __name__ == '__main__':
    main()
