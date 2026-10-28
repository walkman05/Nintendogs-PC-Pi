"""Give callers the callee's actual return type on the PC (see retscan.py): each caller declaration whose narrow
return type differs from the definition's becomes #ifdef NDOGS_PC <definition's type> #else <original> #endif.
usage: python port/tools/retfix.py   (prints the edited files)"""
import os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from retscan_lib import scan, norm, STATEMENTS  # noqa: E402

edited = set()
for fn, def_raw, callers in scan():
    for path in callers:
        t = open(path, errors='replace').read()
        if 'NDOGS_PC: return type of ' + fn in t:
            continue
        pat = re.compile(r'^([ \t]*)((?:extern\s+)?)([A-Za-z_][\w \*]*?)\b(' + fn + r')(\s*\([^;{]*\)\s*;[^\n]*)$', re.M)

        def sub(m):
            if m.group(3).split()[0] in STATEMENTS:
                return m.group(0)
            if norm(m.group(3)) == norm(def_raw):
                return m.group(0)
            pc = '%s%s%s %s%s' % (m.group(1), m.group(2), def_raw, m.group(4), m.group(5))
            return ('#ifdef NDOGS_PC /* NDOGS_PC: return type of %s as defined (x86 extends by the caller\'s '
                    'prototype, the DS by the callee) */\n%s\n#else\n%s\n#endif' % (fn, pc, m.group(0)))
        n = pat.sub(sub, t)
        if n != t:
            open(path, 'w', newline='\n').write(n)
            edited.add(os.path.relpath(path, ROOT))
for e in sorted(edited):
    print(e)
