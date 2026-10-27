"""Milestone 2 check: compile every decompiled C file for the 32-bit host, one by one, and summarise the errors.

usage: python port/tools/host_compile.py [--cc gcc] [--jobs N] [--out report.txt]
Exit status 0 only when every file compiles.
"""
import argparse, collections, os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
FLAGS = ['-m32', '-std=gnu99', '-fwrapv', '-fno-strict-aliasing', '-w', '-Wno-unknown-pragmas',
         '-DNDOGS_PC=1', '-include', os.path.join(ROOT, 'port', 'include', 'host_compat.h'),
         '-I', os.path.join(ROOT, 'include'), '-I', os.path.join(ROOT, 'port', 'include')]


def sources():
    for dirpath, _, files in os.walk(os.path.join(ROOT, 'src')):
        for f in sorted(files):
            if f.endswith('.c'):
                yield os.path.join(dirpath, f)


def compile_one(cc, path):
    r = subprocess.run([cc] + FLAGS + ['-c', path, '-o', os.devnull], capture_output=True, text=True)
    return path, r.returncode, r.stderr


def category(msg):
    msg = re.sub(r"'[^']*'", "'X'", msg)
    msg = re.sub(r'\b\d+\b', 'N', msg)
    return msg.strip()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='gcc')
    ap.add_argument('--jobs', type=int, default=os.cpu_count() or 4)
    ap.add_argument('--out', default='host_compile_report.txt')
    args = ap.parse_args()
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
    print(open(args.out).read()[:6000])
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
