"""Second step of the narrow return-type sweep (see retscan_lib.py): callers that store a mismatched function's
result in a narrow local (or cast it narrow). On the DS the register keeps the callee's value unless the code
re-extends it right after the call; on x86 the narrow local truncates/extends it. For each such site this checks
the original ARM code after `bl <callee>` and reports whether the DS re-extended (then the narrow local is right)
or not (then the local should hold the full value: widen it under NDOGS_PC).

usage: python port/tools/retlocals.py [--apply]"""
import os, re, subprocess, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from retscan_lib import scan, norm, ROOT, NARROW  # noqa: E402

RANGE = {'s8': (-128, 127), 'u8': (0, 255), 's16': (-32768, 32767), 'u16': (0, 65535)}


def asm_of(fn):
    r = subprocess.run([sys.executable, os.path.join(ROOT, 'port', 'tools', 'showasm.py'), fn], capture_output=True,
                       text=True, cwd=ROOT)
    return r.stdout.splitlines()


def after_call(lines, callee):
    """for each `bl callee`: 'ext' (re-extended/masked), 'store' (only stored narrow), or 'raw' (used as is)"""
    out = []
    for i, l in enumerate(lines):
        if re.search(r'\bbl\s+' + callee + r'\b', l):
            nxt = ' '.join(x.strip() for x in lines[i + 1:i + 4])
            first = lines[i + 1].strip() if i + 1 < len(lines) else ''
            if re.search(r'r0, lsl #0x(10|18)|and r\d+, r0, #0xff\b|r0, asr #0x(10|18)', nxt):
                out.append('ext')
            elif re.match(r'str[bh]\w*\s+r0,', first):
                out.append('store')
            else:
                out.append('raw')
    return out


def main():
    apply = '--apply' in sys.argv
    widened = []
    for fn, def_raw, callers in scan():
        d = norm(def_raw)
        for path in callers:
            if not os.path.relpath(path, ROOT).startswith('src'):
                continue
            t = open(path, errors='replace').read()
            sites = [(m.group(1), m.group(2)) for m in
                     re.finditer(r'\b(\w+)\s*=\s*(\(\s*\w+\s*\)\s*)?' + fn + r'\s*\(', t)]
            if not sites:
                continue
            caller = os.path.basename(path)[:-2]
            kinds = after_call(asm_of(caller), fn)
            for var, cast in sites:
                m = re.search(r'^([ \t]*)((?:volatile\s+)?(s8|u8|s16|u16|char|short))\s+' + var + r'\s*;[^\n]*$',
                              t, re.M)
                vt = norm(m.group(2)) if m else None
                if vt not in NARROW or vt == d:
                    continue
                verdict = 'ext' if 'ext' in kinds else 'raw' if 'raw' in kinds else 'store' if kinds else '?'
                if def_raw == 'BOOL':
                    verdict = 'fits'  # 0/1 fits any local
                # other writes to the variable would lose their truncation if it were widened: review by hand
                others = len(re.findall(r'\b' + var + r'\s*(?:[-+*/&|^]?=(?!=)|\+\+|--)', t)) - len(
                    re.findall(r'\b' + var + r'\s*=\s*(?:\(\s*\w+\s*\)\s*)?func_\w+\s*\(', t))
                others += len(re.findall(r'(?:\+\+|--)\s*' + var + r'\b', t))
                if re.search(r'&\s*' + var + r'\b', t):
                    others += 1  # address taken: its size matters
                if verdict == 'raw' and others > 0:
                    verdict = 'review'
                print('%-40s %-14s %s %s <- %s (defined %s) asm after call: %s -> %s' % (
                    os.path.relpath(path, ROOT), fn, vt, var, cast or '', def_raw, ','.join(kinds),
                    {'raw': 'widen', 'review': 'REVIEW'}.get(verdict, 'keep (' + verdict + ')')))
                if apply and verdict == 'raw' and 'NDOGS_PC: widened' not in t:
                    wide = 'u32' if d in ('u8', 'u16', 'u32') else 's32'
                    new = ('#ifdef NDOGS_PC /* NDOGS_PC: widened: holds %s\'s value unchanged, as the DS register '
                           'does */\n%s%s %s;\n#else\n%s\n#endif') % (fn, m.group(1), wide, var, m.group(0))
                    t = t[:m.start()] + new + t[m.end():]
                    open(path, 'w', newline='\n').write(t)
                    widened.append(os.path.relpath(path, ROOT))
    if apply:
        print('\n'.join(['widened:'] + sorted(set(widened))))


if __name__ == '__main__':
    main()
