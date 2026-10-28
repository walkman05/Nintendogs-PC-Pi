"""Shared by retscan.py / retfix.py: functions whose callers declare a different narrow return type than the
definition. On the DS the callee's code decides how the value in r0 is extended (callers use r0 as it is); on
x86 the caller's prototype does, so such mismatches change values (e.g. a 0..0xffff angle read as negative)."""
import os, re, collections

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TYPES = {'s8': 's8', 'u8': 'u8', 's16': 's16', 'u16': 'u16', 'char': 's8', 'short': 's16', 'BOOL': 's32',
         's32': 's32', 'u32': 'u32', 'int': 's32', 'fx32': 's32', 'fx16': 's16', 'unsigned': 'u32', 'void': 'void',
         'u64': 'u64', 's64': 's64', 'fx64': 's64', 'GXRgb': 'u16'}
NARROW = ('s8', 'u8', 's16', 'u16')
STATEMENTS = ('return', 'else', 'case', 'goto', 'if', 'do')


def norm(t):
    t = re.sub(r'\b(extern|static|inline|const|volatile)\b', '', t).strip()
    if '*' in t:
        return 'ptr'
    words = t.split()
    return TYPES.get(words[-1], words[-1]) if words else t


def scan():
    """[(function, definition's return type as written, [caller files with a mismatching declaration])]"""
    defs, decls = {}, collections.defaultdict(list)
    for root in ('src', os.path.join('port', 'hostfuncs')):
        for p, _, fs in os.walk(os.path.join(ROOT, root)):
            for f in fs:
                if not f.endswith('.c'):
                    continue
                path = os.path.join(p, f)
                t = open(path, errors='replace').read()
                name = f[:-2]
                m = re.search(r'^([A-Za-z_][\w \*]*?)\b' + name + r'\s*\([^;{]*\)\s*\{', t, re.M)
                if m:
                    defs[name] = re.sub(r'\b(static|inline)\b', '', m.group(1)).strip()
                for m in re.finditer(r'^\s*(?:extern\s+)?([A-Za-z_][\w \*]*?)\b(func_\w+)\s*\([^;{]*\)\s*;', t, re.M):
                    if m.group(1).split()[0] in STATEMENTS or m.group(2) == name:
                        continue
                    decls[m.group(2)].append((norm(m.group(1)), path))
    out = []
    for fn, raw in sorted(defs.items()):
        d = norm(raw)
        if d in ('void', 'ptr'):
            continue
        bad = sorted(set(f for t, f in decls.get(fn, []) if t != d and (t in NARROW or d in NARROW)
                         and t not in ('void', 'ptr')))
        if bad:
            out.append((fn, raw, bad))
    return out
