"""Print the original disassembly of one or more functions, from the dsd disassembly of the user's own ROM
(extracted/asm_all, made by `dsd dis -c extracted/config/arm9/config.yaml -a extracted/asm_all`).

usage: python port/tools/showasm.py func_0200a648 [more names...]
"""
import glob, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def load():
    blocks = {}
    for p in glob.glob(os.path.join(ROOT, 'extracted', 'asm_all', '**', '*.s'), recursive=True):
        cur, buf = None, []
        for line in open(p, encoding='utf-8', errors='replace'):
            m = re.match(r'\s*(?:arm|thumb)_func_start (\S+)', line)
            if m:
                cur, buf = m.group(1), [line]
                continue
            if cur:
                buf.append(line)
                if re.match(r'\s*(?:arm|thumb)_func_end', line):
                    blocks[cur] = ''.join(buf)
                    cur = None
    return blocks


def main():
    blocks = load()
    for name in sys.argv[1:]:
        print(blocks.get(name, '; %s: not found in extracted/asm_all\n' % name))


if __name__ == '__main__':
    main()
