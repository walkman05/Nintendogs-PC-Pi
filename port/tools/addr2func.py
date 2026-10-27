"""Name host addresses (crash / watchdog backtraces) using the linker map.

usage: python port/tools/addr2func.py <program.map> 0x00487e2b [0x...]   (or pipe a backtrace on stdin)
"""
import bisect, re, sys


def main():
    syms = []
    for line in open(sys.argv[1], errors='replace'):
        m = re.match(r'\s+0x([0-9a-f]{8})\s+([A-Za-z_]\w*)\s*$', line)
        if m:
            syms.append((int(m.group(1), 16), m.group(2)))
    syms.sort()
    keys = [a for a, _ in syms]
    addrs = sys.argv[2:] or re.findall(r'0x[0-9a-fA-F]{6,8}|\b[0-9A-F]{8}\b', sys.stdin.read())
    for a in addrs:
        v = int(a, 16)
        i = bisect.bisect_right(keys, v) - 1
        print('%s  %s+0x%x' % (a, syms[i][1], v - syms[i][0]) if i >= 0 else a)


if __name__ == '__main__':
    main()
