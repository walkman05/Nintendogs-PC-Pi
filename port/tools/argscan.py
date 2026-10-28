"""Find decompiled functions that take more register arguments on the DS than their C definition declares.

A function that forwards r1-r3 untouched (e.g. a tail call) can match on the DS with too few parameters, but on the
PC the callee then gets garbage. For every function in src/ this reads the original asm (showasm.py) and works out
how many argument registers it really uses (read before written, or passed on to a callee that uses them, found
recursively from the callees' own asm). Prints functions whose C definition declares fewer.
Caveats: 64-bit/double parameters use two registers (count them by hand); a linear scan, so review each hit.
Usage: python port/tools/argscan.py"""
import re, subprocess, sys, os
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
rows = []
for p, _, fs in os.walk(os.path.join(ROOT, 'src')):
    for f in fs:
        if f.endswith('.c') and f.startswith('func_'):
            t = open(os.path.join(p, f), errors='replace').read()
            m = re.search(r'^[^\n;#]*\b' + f[:-2] + r'\s*\(([^)]*)\)\s*\{', t, re.M)
            if m:
                a = m.group(1).strip()
                n = 0 if a in ('', 'void') else a.count(',') + 1
                if n < 4 and '...' not in a:
                    rows.append([f[:-2], 'def=%d' % n])
names = [r[0] for r in rows]
asm = ''.join(subprocess.run([sys.executable, ROOT + '/port/tools/showasm.py'] + names[i:i + 400],
                             capture_output=True, text=True, cwd=ROOT).stdout for i in range(0, len(names), 400))
blocks = dict(re.findall(r'arm_func_start (\w+)\n(.*?)arm_func_end', asm, re.S))

# definitions' param counts, for callees
defs = {}
for r in rows:
    defs[r[0]] = int(r[1].split('=')[1])
REG = re.compile(r'\b(r[0-9]|r1[0-2]|ip|lr|sp|pc)\b')
STORE = ('str', 'cmp', 'cmn', 'tst', 'teq')

def regs(s):
    return [x for x in REG.findall(s)]

def scan(body, first):
    """registers r<first>..r3 read before written (linear scan); and calls/tail calls they are live into"""
    defined = set('r%d' % i for i in range(first))
    reads, live_calls = set(), []
    lit = dict(re.findall(r'(\.L_\w+): \.word (\w+)', body))
    iplit = None
    for line in body.splitlines():
        line = line.strip()
        if not line or line.endswith(':') or line.startswith('.') or line.startswith(';') or ':' in line.split()[0]:
            continue
        op, _, rest = line.partition(' ')
        base = re.sub(r'(eq|ne|cs|cc|mi|pl|vs|vc|hi|ls|ge|lt|gt|le|al)?s?$', '', op)
        if op.startswith('bl') and not op.startswith('blx') or op == 'blx':
            tgt = rest.strip()
            live = [r for r in ('r1', 'r2', 'r3') if r not in defined and int(r[1]) >= first]
            if live:
                live_calls.append((tgt, live))
            defined |= {'r0', 'r1', 'r2', 'r3', 'ip'}
            continue
        if op in ('bx', 'b') or op.startswith('b') and op[1:] in ('', 'eq', 'ne', 'lt', 'gt', 'le', 'ge', 'hi', 'ls', 'cc', 'cs', 'mi', 'pl'):
            tgt = rest.strip()
            if tgt == 'lr':
                continue
            if tgt == 'ip':
                tgt = iplit or 'ip'
            if tgt.startswith('func_'):
                live = [r for r in ('r1', 'r2', 'r3') if r not in defined and int(r[1]) >= first]
                if live:
                    live_calls.append(('tail ' + tgt, live))
            continue
        rs = regs(rest)
        if base.startswith('stm') or base.startswith('push'):
            for r in regs(rest.split('{', 1)[1]) if '{' in rest else []:
                if r in ('r1', 'r2', 'r3') and r not in defined and int(r[1]) >= first:
                    reads.add(r + '(push)')
            continue
        if base.startswith('ldm') or base.startswith('pop'):
            if '{' in rest:
                defined |= set(regs(rest.split('{', 1)[1]))
            continue
        if base.startswith(STORE) or base in STORE:
            srcs, dst = rs, []
        else:
            dst, srcs = rs[:1], rs[1:]
            if base.startswith('ldr') and 'ip' in dst:
                m = re.search(r'(\.L_\w+)', rest)
                iplit = lit.get(m.group(1)) if m else None
            if base.startswith(('umull', 'smull')):
                dst, srcs = rs[:2], rs[2:]
            elif base.startswith(('umlal', 'smlal')):
                dst, srcs = rs[:2], rs
        for r in srcs:
            if r in ('r1', 'r2', 'r3') and r not in defined and int(r[1]) >= first:
                reads.add(r)
        defined |= set(dst)
    return reads, live_calls

out = []
for r in []:
    name, d = r[0], int(r[1].split('=')[1])
    body = blocks.get(name)
    if body is None:
        out.append((name, d, 'NO ASM'))
        continue
    reads, calls = scan(body, d)
    if reads or calls:
        out.append((name, d, 'reads %s calls %s' % (sorted(reads), calls[:3])))


# ---- refine: does the callee take the live registers?
allp = {}
for d in ('src', 'port/hostfuncs'):
    for p, _, fs in os.walk(os.path.join(ROOT, d)):
        for f in fs:
            if f.endswith('.c') and f.startswith('func_'):
                t = open(os.path.join(p, f), errors='replace').read()
                m = re.search(r'^[^\n;#]*\b' + f[:-2] + r'\s*\(([^)]*)\)\s*\{', t, re.M)
                if m:
                    s = m.group(1).strip()
                    allp[f[:-2]] = 0 if s in ('', 'void') else s.count(',') + 1
# ---- second refinement: callee argument counts from the callee's own asm (recursively)
need_names = set()
for r in rows:
    b = blocks.get(r[0])
    if b:
        for tgt, live in scan(b, 0)[1]:
            need_names.add(tgt.replace('tail ', ''))
extra = sorted(n for n in need_names if n.startswith('func_') and n not in blocks)
for i in range(0, len(extra), 400):
    more = subprocess.run([sys.executable, ROOT + '/port/tools/showasm.py'] + extra[i:i + 400], capture_output=True,
                          text=True, cwd=ROOT).stdout
    blocks.update(dict(re.findall(r'arm_func_start (\w+)\n(.*?)arm_func_end', more, re.S)))
memo = {}
def asm_params(f, depth=0):
    if f in memo:
        return memo[f]
    b = blocks.get(f)
    if b is None or depth > 6:
        return allp.get(f, 4)
    memo[f] = 0
    reads, calls = scan(b, 0)
    n = 0
    for x in reads:
        if '(push)' not in x:
            n = max(n, int(x[1]) + 1)
    for tgt, live in calls:
        c = tgt.replace('tail ', '')
        if c.startswith('func_'):
            k = asm_params(c, depth + 1)
            for x in live:
                if int(x[1]) < k:
                    n = max(n, int(x[1]) + 1)
    memo[f] = n
    return n
final = []
for r in rows:
    name, d = r[0], int(r[1].split('=')[1])
    if name not in blocks:
        continue
    n = asm_params(name)
    if n > d:
        final.append('%s def=%d asm=%d' % (name, d, n))
print('\n'.join(final))

