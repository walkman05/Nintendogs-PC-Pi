"""Milestone 2 check: compile every decompiled C file for the 32-bit host, one by one, and summarise the errors.
With --link, keep the objects and link the whole game, reporting undefined and duplicate symbols.

usage: python port/tools/host_compile.py [--cc gcc] [--jobs N] [--out report.txt] [--link objdir]
Exit status 0 only when every file compiles.
"""
import argparse, collections, os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
FLAGS = ['-m32', '-std=gnu99', '-g1', '-fno-omit-frame-pointer', '-fwrapv', '-fno-strict-aliasing', '-fpermissive', '-w', '-Wno-unknown-pragmas',
         '-DNDOGS_PC=1', '-include', os.path.join(ROOT, 'port', 'include', 'host_compat.h'),
         '-I', os.path.join(ROOT, 'include'), '-I', os.path.join(ROOT, 'port', 'include')]
DATA_RE = re.compile(r'\b(?:data|bss)_(?:ov[0-9]+_)?([0-9a-f]{8})(?:__\d+)?\b')
OBJDIR = None


def sources():
    for dirpath, _, files in os.walk(os.path.join(ROOT, 'src')):
        for f in sorted(files):
            if f.endswith('.c'):
                yield os.path.join(dirpath, f)


def obj_path(path):
    if not OBJDIR:
        return os.devnull
    rel = os.path.relpath(path, ROOT).replace(os.sep, '_')
    return os.path.join(OBJDIR, rel[:-2] + '.o')


def compile_one(cc, path):
    r = subprocess.run([cc] + FLAGS + ['-c', path, '-o', obj_path(path)], capture_output=True, text=True)
    return path, r.returncode, r.stderr


def windows_target(cc):
    r = subprocess.run([cc, '-dumpmachine'], capture_output=True, text=True)
    return 'mingw' in r.stdout or 'cygwin' in r.stdout


def link(cc, objs, out):
    """data_/bss_ symbols are absolute at the address their name gives (DS memory is mapped there at run time)."""
    win = windows_target(cc)
    us = '_' if win else ''   # 32-bit Windows decorates C symbols with a leading underscore
    names = set()
    hf = os.path.join(ROOT, 'port', 'hostfuncs')
    for p in list(sources()) + [os.path.join(hf, x) for x in os.listdir(hf) if x.endswith('.c')]:
        with open(p, encoding='utf-8', errors='replace') as f:
            names.update(m.group(0) for m in DATA_RE.finditer(f.read()))
    script = os.path.join(OBJDIR, 'datasyms.ld')
    with open(script, 'w') as f:
        for n in sorted(names):
            f.write('%s%s = 0x%s;\n' % (us, n, re.search(r'([0-9a-f]{8})(?:__\d+)?$', n).group(1)))
    # trampolines become x86 tail jumps (signature-agnostic; a missing target is reported like any other symbol)
    stub = os.path.join(OBJDIR, 'veneers.S')
    with open(os.path.join(ROOT, 'port', 'link', 'veneers.txt')) as v, open(stub, 'w') as f:
        f.write('.text\n')
        for line in v:
            if '=' in line and not line.startswith('#'):
                a, b = [x.strip() for x in line.split('=')]
                f.write('.globl %s%s\n%s%s:\n    jmp %s%s\n' % (us, a, us, a, us, b))
    subprocess.run([cc, '-m32', '-c', stub, '-o', stub[:-2] + '.o'], check=True)
    objs = list(objs) + [stub[:-2] + '.o']
    rsp = os.path.join(OBJDIR, 'objs.rsp')
    with open(rsp, 'w') as f:
        f.write('\n'.join(o.replace('\\', '/') for o in objs))
    flags = ['-m32', '-Wl,-Map=' + out + '.map'] + ([] if win else ['-no-pie'])
    r = subprocess.run([cc] + flags + ['@' + rsp, script, '-o', out], capture_output=True, text=True)
    undef = sorted(set(re.findall(r"undefined reference to `([^']+)'", r.stderr)))
    dup = sorted(set(re.findall(r"multiple definition of `([^']+)'", r.stderr)))
    return r.returncode, undef, dup, r.stderr


def category(msg):
    msg = re.sub(r"'[^']*'", "'X'", msg)
    msg = re.sub(r'\b\d+\b', 'N', msg)
    return msg.strip()


def main():
    global OBJDIR
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='gcc')
    ap.add_argument('--jobs', type=int, default=os.cpu_count() or 4)
    ap.add_argument('--out', default='host_compile_report.txt')
    ap.add_argument('--link', help='object directory: keep objects and try linking the whole game')
    ap.add_argument('--exe', default='nintendogs_pc', help='name of the linked program (in the object directory)')
    args = ap.parse_args()
    if args.link:
        OBJDIR = os.path.abspath(args.link)
        os.makedirs(OBJDIR, exist_ok=True)
    files = list(sources())
    with ThreadPoolExecutor(args.jobs) as ex:
        results = list(ex.map(lambda p: compile_one(args.cc, p), files))
    bad = [(p, e) for p, rc, e in results if rc != 0]
    cats = collections.Counter()
    examples = {}
    for p, err in bad:
        seen = set()
        for line in err.splitlines():
            m = re.search(r'error: (.*)', line)
            if m:
                c = category(m.group(1))
                if c not in seen:
                    seen.add(c)
                    cats[c] += 1
                    examples.setdefault(c, os.path.relpath(p, ROOT) + ': ' + line.strip()[:200])
    with open(args.out, 'w') as f:
        f.write('files: %d  ok: %d  failing: %d\n\n' % (len(files), len(files) - len(bad), len(bad)))
        for c, n in cats.most_common():
            f.write('%5d  %s\n       e.g. %s\n' % (n, c, examples[c]))
        f.write('\nfailing files:\n')
        for p, _ in bad:
            f.write(os.path.relpath(p, ROOT) + '\n')
        if args.link:
            ok = [obj_path(p) for p, rc, _ in results if rc == 0]
            gen = os.path.join(OBJDIR, 'functab.c')
            subprocess.run([sys.executable, os.path.join(ROOT, 'port', 'tools', 'gen_functab.py'), gen], check=True)
            extras = [os.path.join(ROOT, 'port', 'src', x) for x in
                      ('host_cpu.c', 'host_link.c', 'host_io.c', 'platform_mem.c', 'loader.c', 'main.c')] + [gen]
            hf = os.path.join(ROOT, 'port', 'hostfuncs')
            extras += [os.path.join(hf, x) for x in sorted(os.listdir(hf)) if x.endswith('.c')]
            for src in extras:
                o = os.path.join(OBJDIR, 'port_' + os.path.basename(src)[:-2] + '.o')
                r = subprocess.run([args.cc] + FLAGS + ['-c', src, '-o', o], capture_output=True, text=True)
                if r.returncode:
                    f.write('port source failed: %s\n%s\n' % (os.path.relpath(src, ROOT), r.stderr[:2000]))
                else:
                    ok.append(o)
            rc, undef, dup, err = link(args.cc, ok, os.path.join(OBJDIR, args.exe))
            f.write('\nlink (%d objects): %s  undefined: %d  multiply defined: %d\n'
                    % (len(ok), 'ok' if rc == 0 else 'failed', len(undef), len(dup)))
            for u in undef:
                f.write('  undefined %s\n' % u)
            for d in dup:
                f.write('  duplicate %s\n' % d)
            if rc and not undef and not dup:
                f.write(err[:4000])
    print(open(args.out).read()[:8000])
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
