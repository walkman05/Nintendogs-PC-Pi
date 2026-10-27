"""Prove a port-only edit leaves the DS (matching) build untouched: compile the committed and the working copy
of each file with its mwccarm build (functions.csv) and compare the object files byte for byte.

usage: python port/tools/check_ds_unchanged.py <src/...c> [...]   (needs ../tools/mwccarm next to the repo)
"""
import csv, os, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MW = os.path.join(os.path.dirname(ROOT), 'tools', 'mwccarm')
FLAGS = ['-proc', 'arm946e', '-O4,p', '-interworking', '-enum', 'int', '-char', 'signed', '-nostdinc']


def compiler_for(path):
    rel = os.path.relpath(os.path.abspath(path), ROOT).replace(os.sep, '/')
    with open(os.path.join(ROOT, 'functions.csv')) as f:
        for row in csv.DictReader(f):
            if row['file'] == rel:
                return row['compiler'].split('/')[1]
    return 'base'


def build(src_text, name, ver, tmp):
    os.makedirs(tmp, exist_ok=True)
    c = os.path.join(tmp, name)
    with open(c, 'w', newline='\n') as f:
        f.write(src_text)
    o = c + '.o'
    r = subprocess.run([os.path.join(MW, '1.2', ver, 'mwccarm.exe')] + FLAGS +
                       ['-i', os.path.join(ROOT, 'include'), '-c', c, '-o', o], capture_output=True, text=True)
    if r.returncode:
        return None, r.stdout + r.stderr
    return open(o, 'rb').read(), ''


def main():
    bad = 0
    tmp = tempfile.mkdtemp()
    for p in sys.argv[1:]:
        rel = os.path.relpath(os.path.abspath(p), ROOT).replace(os.sep, '/')
        old = subprocess.run(['git', 'show', 'HEAD:' + rel], cwd=ROOT, capture_output=True, text=True).stdout
        new = open(p, encoding='utf-8').read()
        ver = compiler_for(p)
        a, ea = build(old, os.path.basename(p), ver, os.path.join(tmp, 'old'))
        b, eb = build(new, os.path.basename(p), ver, os.path.join(tmp, 'new'))
        same = a is not None and b is not None and a == b
        if not same:
            bad += 1
        print('%-55s %s %s' % (rel, ver, 'unchanged' if same else 'CHANGED/FAILED ' + (eb or ea)[:300]))
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
