"""Narrow parameters: a function defined with an s8/u8/s16/u16 parameter that some caller declares (and so passes)
as a 32-bit value. On the DS the callee receives the whole register; whether it narrows it depends on its code.
On x86 the callee always narrows to its declared type. For each such parameter (r0-r3 only) this checks the
original ARM code of the callee for a narrowing of that register before it is otherwise used; if there is none,
the DS callee works with the full value and the parameter should be widened on the PC.

usage: python port/tools/paramscan.py [--apply]"""
import os, re, subprocess, sys, collections

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from retscan_lib import norm, ROOT, NARROW, STATEMENTS  # noqa: E402

WIDE = ('s32', 'u32')


def split_params(s):
    s = s.strip()
    if s in ('', 'void'):
        return []
    out, depth, cur = [], 0, ''
    for ch in s:
        if ch == ',' and depth == 0:
            out.append(cur)
            cur = ''
            continue
        depth += ch == '('
        depth -= ch == ')'
        cur += ch
    out.append(cur)
    return [p.strip() for p in out]


def ptype(p):
    """type of one parameter declaration (drop the name)"""
    p = re.sub(r'\b(const|volatile)\b', '', p).strip()
    if '(' in p or '...' in p:
        return '?'
    m = re.match(r'^(.*?)(\b[A-Za-z_]\w*)?$', p)
    t = m.group(1).strip() if m and m.group(1).strip() else p
    if re.match(r'^(s8|u8|s16|u16|s32|u32|int|BOOL|fx32|fx16|char|short|unsigned)$', p.split()[0]) and len(p.split()) == 1:
        t = p
    return norm(t)


def narrows(asm, reg):
    """how the callee treats register `reg` (before overwriting it): True if it narrows it or only stores it as a
    byte/halfword (same on both machines), False if it uses the full value (arithmetic, compare, logic, addressing,
    a word store, or passing it on)"""
    for l in asm[2:60]:
        l = l.strip()
        if not l or l.endswith(':') or l.startswith('.') or l.startswith('arm_func'):
            continue
        ops = re.split(r'[,{}' + chr(92) + r'[' + chr(92) + r']! ]+', l)
        op, args = ops[0], [x for x in ops[1:] if x]
        if reg not in args and not (op.startswith(('stm', 'push')) and reg in l):
            if op.startswith(('bl', 'blx')):
                return True   # a call clobbers r0-r3 without having used it
            if re.match(r'^b(x|eq|ne|lt|gt|le|ge|cs|cc|hi|ls|mi|pl)?$', op):
                return True   # control flow without using it: treat as unused
            continue
        if re.search(r'lsl #0x(10|18)\b', l) or (re.search(r'#0xff\b', l) and op.startswith('and')) \
                or re.match(r'(smul|smla)(bb|tb|bt|wb)', op):
            return True
        if re.match(r'str(h|b)', op) and args and args[0] == reg:
            continue      # stored narrow: same bits either way
        if args and args[0] == reg and not op.startswith(('str', 'cmp', 'cmn', 'tst', 'teq', 'stm')):
            if reg in args[1:]:
                return False   # read and written by the same instruction: a full-width use
            return True        # overwritten before any full-width use
        return False           # any other use reads the full register
    return True


def main():
    apply = '--apply' in sys.argv
    defs, files, decls = {}, {}, collections.defaultdict(list)
    for root in ('src',):
        for p, _, fs in os.walk(os.path.join(ROOT, root)):
            for f in fs:
                if not f.endswith('.c'):
                    continue
                path = os.path.join(p, f)
                t = open(path, errors='replace').read()
                name = f[:-2]
                m = re.search(r'^[A-Za-z_][\w \*]*?\b' + name + r'\s*\(([^;{]*)\)\s*\{', t, re.M)
                if m:
                    defs[name] = split_params(m.group(1))
                    files[name] = path
                for m in re.finditer(r'^\s*(?:extern\s+)?([A-Za-z_][\w \*]*?)\b(func_\w+)\s*\(([^;{]*)\)\s*;', t, re.M):
                    if m.group(1).split()[0] in STATEMENTS or m.group(2) == name:
                        continue
                    decls[m.group(2)].append((split_params(m.group(3)), path))
    hits = collections.defaultdict(set)
    for fn, params in defs.items():
        for i, prm in enumerate(params[:4]):
            dt = ptype(prm)
            if dt not in NARROW:
                continue
            wide_callers = [f for ps, f in decls.get(fn, []) if i < len(ps) and ptype(ps[i]) in WIDE + ('ptr',)]
            if wide_callers:
                hits[fn].add((i, prm, len(wide_callers)))
    edited = []
    for fn in sorted(hits):
        asm = subprocess.run([sys.executable, os.path.join(ROOT, 'port', 'tools', 'showasm.py'), fn],
                             capture_output=True, text=True, cwd=ROOT).stdout.splitlines()
        if not asm:
            continue
        for i, prm, n in sorted(hits[fn]):
            nar = narrows(asm, 'r%d' % i)
            print('%-24s param %d %-22s %d wide caller decl(s): callee %s -> %s' % (
                fn, i, prm, n, 'narrows' if nar else 'uses the full register', 'keep' if nar else 'WIDEN'))
            if apply and not nar:
                path = files[fn]
                t = open(path).read()
                m = re.search(r'^([A-Za-z_][\w \*]*?\b' + fn + r'\s*\()([^;{]*)(\)\s*\{)', t, re.M)
                ps = split_params(m.group(2))
                if i >= len(ps) or ps[i] != prm:
                    continue
                wide_t = 'u32' if ptype(prm) in ('u8', 'u16') else 's32'
                pname = prm.split()[-1]
                ps_pc = list(ps)
                ps_pc[i] = '%s %s' % (wide_t, pname)
                sig_pc = m.group(1) + ', '.join(ps_pc) + m.group(3)
                new = ('#ifdef NDOGS_PC /* NDOGS_PC: widened parameter %d: the DS callee uses the whole register */\n'
                       '%s\n#else\n%s\n#endif') % (i, sig_pc, m.group(0))
                t = t[:m.start()] + new + t[m.end():]
                open(path, 'w', newline='\n').write(t)
                edited.append(os.path.relpath(path, ROOT))
    if apply:
        print('\n'.join(['edited:'] + sorted(set(edited))))


if __name__ == '__main__':
    main()
