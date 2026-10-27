"""Find functions whose return type differs between their definition and the externs other files declare.

On the DS a narrower return (u8/s16/BOOL...) read as a wider one is usually harmless, because the callee leaves a
clean 32-bit register; on x86 the upper bits of eax are not guaranteed, and a void read as a value is garbage.
Reports, per function: the defined return type and the conflicting declarations (file: type).

usage: python port/tools/proto_check.py [--all]   (default: only mismatches that can change a value on x86)
"""
import os, re, sys
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
NAME = r'((?:func|Func)_\w+)'
DECL = re.compile(r'^\s*(?:extern\s+)?((?:const\s+|unsigned\s+|signed\s+|struct\s+\w+\s*)*[A-Za-z_]\w*(?:\s*\*+)?)\s*\**\s*'
                  + NAME + r'\s*\(([^;{]*)\)\s*;', re.M)
DEFN = re.compile(r'^(?:static\s+)?(?:inline\s+)?((?:const\s+|unsigned\s+|signed\s+|struct\s+\w+\s*)*[A-Za-z_]\w*(?:\s*\*+)?)\s*\**\s*'
                  + NAME + r'\s*\(([^;{]*)\)\s*\{', re.M)
NARROW = {'u8', 's8', 'u16', 's16', 'char', 'short', 'BOOL8', 'unsigned char', 'unsigned short', 'signed char'}
WIDE64 = {'u64', 's64', 'fx64', 'long long', 'unsigned long long'}


def norm(t):
    return re.sub(r'\s+', ' ', t.replace('extern', '').strip())


def kind(t):
    t = norm(t)
    if t == 'void':
        return 'void'
    if '*' in t:
        return 'ptr'
    if t in NARROW:
        return 'narrow:' + t
    if t in WIDE64:
        return '64'
    return 'int'


def main():
    show_all = '--all' in sys.argv
    defs, decls = {}, defaultdict(list)
    for base in ('src', os.path.join('port', 'hostfuncs')):
        for dirpath, _, files in os.walk(os.path.join(ROOT, base)):
            for fn in files:
                if not fn.endswith('.c'):
                    continue
                p = os.path.join(dirpath, fn)
                text = open(p, encoding='utf-8', errors='replace').read()
                rel = os.path.relpath(p, ROOT)
                for m in DEFN.finditer(text):
                    defs.setdefault(m.group(2), (norm(m.group(1)), rel))
                for m in DECL.finditer(text):
                    decls[m.group(2)].append((norm(m.group(1)), rel))
    risky = 0
    for name in sorted(defs):
        dtype, dfile = defs[name]
        dk = kind(dtype)
        bad = []
        for t, f in decls.get(name, []):
            k = kind(t)
            if k == dk:
                continue
            # value-changing on x86: caller reads more than the callee defines
            change = (dk == 'void' and k != 'void') or (dk.startswith('narrow') and k in ('int', 'ptr', '64')) or \
                     (k == '64' and dk != '64') or (dk.startswith('narrow') and k.startswith('narrow'))
            if change or show_all:
                bad.append('%s (%s)' % (f, t))
        if bad:
            risky += 1
            print('%s: defined %s in %s' % (name, dtype, dfile))
            for b in bad[:8]:
                print('    declared ' + b)
    print('\n%d functions with risky return-type mismatches' % risky)


if __name__ == '__main__':
    main()
