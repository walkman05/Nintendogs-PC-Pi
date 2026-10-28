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
# game code optimisation (NDOGS_OPT, default -O0: -O1 exposes undefined behaviour in the decompiled C); the renderers always build at -O2
GAME_OPT = os.environ.get('NDOGS_OPT', '-O0')
FAST_SRC = ('gpu2d.c', 'gpu3d.c', 'video.c', 'vram.c', 'window.c', 'audio.c', 'snd.c', 'workers.c')
FLAGS += os.environ.get('NDOGS_CFLAGS', '').split()  # extra flags, e.g. sanitizer checks for a test build
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


HEADERS_MTIME = None


def headers_mtime():
    global HEADERS_MTIME
    if HEADERS_MTIME is None:
        hs = [os.path.join(d, f) for base in ('include', os.path.join('port', 'include'))
              for d, _, fs in os.walk(os.path.join(ROOT, base)) for f in fs]
        HEADERS_MTIME = max([os.path.getmtime(h) for h in hs] + [os.path.getmtime(os.path.abspath(__file__))])
    return HEADERS_MTIME


def compile_one(cc, path):
    o = obj_path(path)
    # incremental: reuse an object newer than its source and every header (and this script's flags)
    if OBJDIR and os.path.exists(o) and os.path.getmtime(o) > max(os.path.getmtime(path), headers_mtime()):
        return path, 0, ''
    r = subprocess.run([cc] + FLAGS + [GAME_OPT, '-c', path, '-o', o], capture_output=True, text=True)
    return path, r.returncode, r.stderr


def windows_target(cc):
    r = subprocess.run([cc, '-dumpmachine'], capture_output=True, text=True)
    return 'mingw' in r.stdout or 'cygwin' in r.stdout


def capture_wrappers(names):
    """C for --wrap wrappers: NDOGS_CAPTURE=<name>@<call>[,...] dumps DS memory and the arguments before and after
    that call of that routine to cap_<name>_<call>.bin (read by port/tools/capdiff.py)"""
    c = ['#include <stdio.h>', '#include <stdlib.h>', '#include <string.h>',
         'typedef unsigned int u32; typedef unsigned long long u64;',
         'static const u32 regions[][2] = { {0x01ff8000u, 0x8000u}, {0x02000000u, 0x400000u}, '
         '{0x027c0000u, 0x10000u}, {0x027f0000u, 0x10000u}, {0x037f0000u, 0x10000u} };',
         'static int cap_want(const char *name, int n)',
         '{',
         '    const char *p = getenv("NDOGS_CAPTURE");',
         '    size_t l = strlen(name);',
         '    while (p && (p = strstr(p, name))) {',
         '        if (p[l] == 64 && atoi(p + l + 1) == n) return 1;',
         '        p += l;',
         '    }',
         '    return 0;',
         '}',
         'static FILE *cap_file, *cap_trace;',
         'static int cap_depth;  /* > 0 while a captured call runs: wrapped calls are traced */',
         'static u32 cap_obj;    /* first argument of the captured call: dumped (0xa00 bytes) at each traced call */',
         'static int cap_obj_ok(void) { return cap_obj >= 0x01ff8000u && cap_obj <= 0x02400000u - 0xa00u; }  /* a pointer into DS RAM (not a number) */',
         'static void cap_obj_dump(void) { unsigned i; for (i = 0; i < 0xa00; i++) fprintf(cap_trace, "%02x", cap_obj_ok() ? ((unsigned char *)cap_obj)[i] : 0); fputc(10, cap_trace); }',
         'static void cap_dump(const char *name, int n, const u32 *a, int after, u64 ret)',
         '{',
         '    unsigned i;',
         '    u32 hdr[16];',
         '    if (!after) {',
         '        char fn[96];',
         '        snprintf(fn, sizeof fn, "cap_%s_%d.bin", name, n);',
         '        cap_file = fopen(fn, "wb");',
         '    }',
         '    if (!cap_file) return;',
         '    hdr[0] = 0x50414344u; hdr[1] = (u32)after; hdr[2] = (u32)ret; hdr[3] = (u32)(ret >> 32);',
         '    memcpy(hdr + 4, a, 48);',
         '    fwrite(hdr, 4, 16, cap_file);',
         '    for (i = 0; i < sizeof regions / sizeof regions[0]; i++) fwrite((void *)regions[i][0], 1, regions[i][1], cap_file);',
         '    { /* the caller frames on the host stack (structures passed by pointer live there) */',
         '        u32 win[2]; win[0] = (u32)a; win[1] = 0x2000;',
         '        fwrite(win, 4, 2, cap_file); fwrite((void *)win[0], 1, win[1], cap_file); }',
         '    { /* the hardware registers as the game sees them (POWCNT, timers, ...) */',
         '        extern void hw_io_snapshot(unsigned char *io); static unsigned char io[0x2000]; u32 win[2];',
         '        hw_io_snapshot(io); win[0] = 0x04000000u; win[1] = 0x2000;',
         '        fwrite(win, 4, 2, cap_file); fwrite(io, 1, 0x2000, cap_file); }',
         '    if (after) { fclose(cap_file); cap_file = 0; fprintf(stderr, "captured %s call %d%c", name, n, 10); }',
         '}']
    for n in names:
        c += ['extern u64 __real_%s(u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32);' % n,
              'u64 __wrap_%s(u32 a0, u32 a1, u32 a2, u32 a3, u32 a4, u32 a5, u32 a6, u32 a7, u32 a8, u32 a9, u32 a10, u32 a11)' % n,  # forwards up to 12 arguments
              '{',
              '    static int calls;',
              '    u32 *a = &a0;  /* the incoming arguments, in the caller frame */',
              '    int hit = cap_want("%s", ++calls) && !cap_depth;  /* not inside another captured call */' % n,
              '    u64 r;',
              '    if (cap_depth && cap_trace) { fprintf(cap_trace, "%s %%08x %%08x %%08x %%08x ", a0, a1, a2, a3); cap_obj_dump(); }' % n,
              '    if (hit) { char fn[96]; snprintf(fn, sizeof fn, "cap_%s_%%d.trace", calls); cap_trace = fopen(fn, "w"); cap_obj = a0; cap_dump("%s", calls, a, 0, 0); cap_depth++; }' % (n, n),
              '    r = __real_%s(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11);' % n,
              '    if (hit) { cap_depth--; cap_dump("%s", calls, a, 1, r); if (cap_trace) fclose(cap_trace); cap_trace = 0; }' % n,
              '    return r;',
              '}']
    return '\n'.join(c) + '\n'


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
    wrap = [x for x in os.environ.get('NDOGS_WRAP', '').split(',') if x]
    if wrap:  # capture build for port/tools/capdiff.py: wrap these routines (see capture_wrappers)
        wsrc = os.path.join(OBJDIR, 'capture_wraps.c')
        open(wsrc, 'w').write(capture_wrappers(wrap))
        subprocess.run([cc] + FLAGS + ['-O0', '-c', wsrc, '-o', wsrc[:-2] + '.o'], check=True)
        objs.append(wsrc[:-2] + '.o')
    rsp = os.path.join(OBJDIR, 'objs.rsp')
    with open(rsp, 'w') as f:
        f.write('\n'.join(o.replace('\\', '/') for o in objs))
    # fixed load address (no ASLR): keeps the program clear of the DS address range and makes the map usable
    flags = ['-m32', '-Wl,-Map=' + out + '.map'] + ['-Wl,--wrap=' + w for w in wrap] + (['-Wl,--disable-dynamicbase', '-Wl,--image-base=0x10000000'] if win else ['-no-pie', '-Wl,-Ttext-segment=0x10000000'])
    libs = [] if win else ['-ldl', '-lpthread']  # dlopen (ALSA, X11), worker threads
    r = subprocess.run([cc] + flags + ['@' + rsp, script, '-o', out] + libs,
                       capture_output=True, text=True)
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
                      ('host_cpu.c', 'host_link.c', 'hw.c', 'hw_trap.c', 'hw_card.c', 'host_threads.c', 'vram.c', 'video.c', 'window.c', 'audio.c', 'snd.c', 'workers.c', 'gpu2d.c', 'gpu3d.c', 'platform_mem.c', 'loader.c', 'main.c')] + [gen]
            extras = [x for x in extras if os.path.exists(x)] + [x for x in os.environ.get('NDOGS_EXTRA_SRC', '').split(';') if x]
            hf = os.path.join(ROOT, 'port', 'hostfuncs')
            extras += [os.path.join(hf, x) for x in sorted(os.listdir(hf)) if x.endswith('.c')]
            for src in extras:
                o = os.path.join(OBJDIR, 'port_' + os.path.basename(src)[:-2] + '.o')
                opt = '-O2' if os.path.basename(src) in FAST_SRC else GAME_OPT
                r = subprocess.run([args.cc] + FLAGS + [opt, '-c', src, '-o', o], capture_output=True, text=True)
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
