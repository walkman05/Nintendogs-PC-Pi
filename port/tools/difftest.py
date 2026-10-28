"""Differential test of the port's hand-written host routines (port/hostfuncs) against the original ARM code
(port/tools/armrun.py, Unicorn). For each pure routine (arguments in, value out) a harness built with the host
compiler calls the C version with the same register-level arguments; both run on random and edge-case inputs and
every mismatch is reported.

usage: python port/tools/difftest.py [--cc gcc] [--n 3000] [func_xxx ...]"""
import argparse, os, random, re, struct, subprocess, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from armrun import Arm  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HF = os.path.join(ROOT, 'port', 'hostfuncs')
WIDE = ('u64', 's64', 'double', 'f64')
BUILD = os.path.join(os.environ.get('NDOGS_TMP', os.path.join(ROOT, 'build')), 'difftest')


def symbols():
    addr = {}
    for p, _, fs in os.walk(os.path.join(ROOT, 'extracted', 'config')):
        for f in fs:
            if f == 'symbols.txt':
                for l in open(os.path.join(p, f)):
                    m = re.match(r'(func_\w+) kind:function\((arm|thumb)[^)]*\) addr:(0x[0-9a-f]+)', l)
                    if m:
                        addr[m.group(1)] = (int(m.group(3), 16), m.group(2))
    return addr


def pure_routines():
    """(name, file, return type, [arg types]) for routines that take values and return one"""
    out = []
    for f in sorted(os.listdir(HF)):
        if not f.endswith('.c'):
            continue
        t = open(os.path.join(HF, f)).read()
        for m in re.finditer(r'^([A-Za-z_][\w \*]*?)\b(func_\w+)\s*\(([^)]*)\)\s*\{', t, re.M):
            ret, name, args = m.group(1).strip(), m.group(2), m.group(3).strip()
            body = t[m.end():]
            if ret.startswith('static') or ret == 'void' or args in ('', 'void'):
                continue
            if name in ('func_01ffdb08', 'func_02080b48'):  # read game globals / CPU state
                continue
            if 'HostIo' in body or 'HostSwpb' in body or '*' in args or 'MEM' in body or 'Rd32' in body:
                continue
            types = [re.sub(r'\s+\w+$', '', a.strip()) for a in args.split(',')]
            if sum(2 if t_ in WIDE else 1 for t_ in types) > 4:
                continue
            out.append((name, f, ret, types))
    return out


def gen_harness(routines):
    lines = ['#include <stdio.h>', '#include <string.h>', '#include "ctx.h"', '#include "nitro.h"']
    lines.append('u32 func_02063cb8(void) { static u32 status; return (u32)&status; }  /* FP status word address */')
    for name, _, ret, types in routines:
        lines.append('%s %s(%s);' % (ret, name, ', '.join(types)))
    lines += ['int main(void)', '{', '    char name[64]; u32 r[4];',
              '    while (scanf("%63s %x %x %x %x", name, &r[0], &r[1], &r[2], &r[3]) == 5) {',
              '        u64 out = 0;']
    for name, _, ret, types in routines:
        decl, args, k = [], [], 0
        for i, t_ in enumerate(types):
            if t_ in WIDE:
                decl.append('%s a%d; {u64 w = r[%d] | (u64)r[%d] << 32; memcpy(&a%d, &w, 8);}' % (t_, i, k, k + 1, i))
                k += 2
            else:
                decl.append('%s a%d = (%s)r[%d];' % (t_, i, t_, k))
                k += 1
            args.append('a%d' % i)
        call = '%s(%s)' % (name, ', '.join(args))
        if ret in WIDE:
            body = '%s v = %s; memcpy(&out, &v, 8);' % (ret, call)
        else:
            body = 'out = (u32)%s;' % call
        lines.append('        if (!strcmp(name, "%s")) { %s %s }' % (name, ' '.join(decl), body))
    lines += ['        printf("%08x %08x\\n", (u32)out, (u32)(out >> 32));', '        fflush(stdout);', '    }',
              '    return 0;', '}']
    return '\n'.join(lines) + '\n'


EDGE32 = [0, 1, 2, 3, 0x7f, 0x80, 0xff, 0x100, 0x1000, 0x7fff, 0x8000, 0xffff, 0x10000, 0x7fffffff, 0x80000000,
          0x80000001, 0xfffffffe, 0xffffffff, 0xfffff000, 0x12345678]
DOUBLES = [0.0, -0.0, 1.0, -1.0, 0.5, 1.5, 2.0, 3.0, 1e-10, 1e10, 123.456, -987.654, 1e300, -1e300, 4294967296.0,
           2147483647.0, 2147483648.0, -2147483648.0, -2147483649.0, 1e-310, float('inf'), float('-inf'),
           float('nan')]


def gen_inputs(types, n, rng):
    """register-level argument tuples"""
    cases = []
    for i in range(n):
        regs = []
        for t_ in types:
            if t_ in ('double', 'f64'):
                d = DOUBLES[i % len(DOUBLES)] if i < len(DOUBLES) * 2 else rng.choice(
                    [rng.uniform(-1e6, 1e6), rng.uniform(-10, 10), rng.uniform(-1e18, 1e18)])
                w = struct.unpack('<Q', struct.pack('<d', d))[0]
                regs += [w & 0xffffffff, w >> 32]
            elif t_ in WIDE:
                w = rng.choice([rng.getrandbits(64), rng.getrandbits(33), rng.getrandbits(20),
                                (1 << 64) - rng.getrandbits(20), EDGE32[i % len(EDGE32)],
                                EDGE32[i % len(EDGE32)] << 32 | EDGE32[(i * 7) % len(EDGE32)]])
                regs += [w & 0xffffffff, w >> 32]
            else:
                regs.append(EDGE32[(i * (len(regs) + 3)) % len(EDGE32)] if i < len(EDGE32) * 3 else
                            rng.choice([rng.getrandbits(32), rng.getrandbits(16), rng.getrandbits(12),
                                        (1 << 32) - rng.getrandbits(16)]))
        cases.append(tuple(regs + [0] * (4 - len(regs))))
    return cases


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='gcc')
    ap.add_argument('--n', type=int, default=3000)
    ap.add_argument('names', nargs='*')
    args = ap.parse_args()
    routines = [r for r in pure_routines() if not args.names or r[0] in args.names]
    syms = symbols()
    for r in routines:  # entry points without a symbol of their own: the address is in the name (ARM code)
        m = re.match(r'func_([0-9a-f]{8})$', r[0])
        if r[0] not in syms and m:
            syms[r[0]] = (int(m.group(1), 16), 'arm')
    routines = [r for r in routines if r[0] in syms and syms[r[0]][1] == 'arm']
    os.makedirs(BUILD, exist_ok=True)
    harness = os.path.join(BUILD, 'harness.c')
    open(harness, 'w').write(gen_harness(routines))
    exe = os.path.join(BUILD, 'harness.exe')
    flags = ['-m32', '-std=gnu99', '-O0', '-fwrapv', '-fno-strict-aliasing', '-fpermissive', '-w', '-DNDOGS_PC=1',
             '-include', os.path.join(ROOT, 'port', 'include', 'host_compat.h'), '-I', os.path.join(ROOT, 'include'),
             '-I', os.path.join(ROOT, 'port', 'include')]
    srcs = sorted(set(os.path.join(HF, r[1]) for r in routines))
    r = subprocess.run([args.cc] + flags + [harness] + srcs + ['-o', exe], capture_output=True, text=True)
    if r.returncode:
        print(r.stderr[:3000])
        sys.exit(1)
    arm = Arm()
    rng = random.Random(1)
    proc = subprocess.Popen([exe], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
    total_bad = 0
    for name, f, ret, types in routines:
        wide = ret in WIDE
        cases = gen_inputs(types, args.n, rng)
        bad, shown, errors = 0, 0, 0
        for c in cases:
            try:
                a0, a1 = arm.call(syms[name][0], c)
            except Exception:
                errors += 1
                continue
            proc.stdin.write('%s %x %x %x %x\n' % ((name,) + c))
            proc.stdin.flush()
            h0, h1 = (int(x, 16) for x in proc.stdout.readline().split())
            if (a0, a1 if wide else 0) != (h0, h1 if wide else 0):
                bad += 1
                if shown < 4:
                    shown += 1
                    print('  %s(%s): arm %08x %08x, host %08x %08x' % (name, ' '.join('%08x' % x for x in c),
                                                                       a0, a1, h0, h1))
        total_bad += bad
        print('%-22s %-7s %-40s %s' % (name, ret, ','.join(types),
                                       'OK' if not bad else '%d of %d MISMATCH' % (bad, len(cases))) +
              (' (%d arm errors)' % errors if errors else ''))
    proc.stdin.close()
    print('done: %d routines, %d mismatching cases' % (len(routines), total_bad))


if __name__ == '__main__':
    main()
