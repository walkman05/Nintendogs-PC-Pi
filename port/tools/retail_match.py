"""Retail retarget, step 1: find each demo function's counterpart in the retail cartridge (Dachshund & Friends, E).

Both builds come from nearly the same source, so most functions are the same code at another address. Each
function's bytes are compared with the words dsd marks as relocations masked out (call offsets, loaded addresses),
then: exact masked matches that are unique on both sides become anchors; repeated code (small getters, stubs) is
placed by order between anchors; what's left between two anchors is paired by order when sizes are close, with a
byte similarity score. Calls inside matched pairs are then checked: a demo call to X should go to X's retail match.

Inputs (git-ignored, from the user's ROMs): extracted/config + extracted/rom (demo, extract_rom.py) and
extracted/retail/config + extracted/retail/rom (dsd rom extract + dsd init on the retail ROM, run here if missing).
Outputs (git-ignored), which make extracted/retail a data folder for the retail build (host_compile.py --target retail):
  match.txt    one line per demo function:
               <demo module> <demo addr> <retail addr|-> <exact|order|similar:<score>|none> <size> [<retail size>]
  symbols.txt  the retail address of every func_/data_/bss_ name the C links against
  host/        the loader tables (as extract_rom.py writes them), the cartridge header and the ROM's path

usage: python port/tools/retail_match.py --rom <Dachshund & Friends (E).nds> [--dsd <dsd>]
"""
import collections, difflib, os, re, struct, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EX = os.path.join(ROOT, 'extracted')
FUNC = re.compile(r'^(\S+) kind:function\((arm|thumb),size=0x([0-9a-f]+)\) addr:0x([0-9a-f]+)')
RELOC = re.compile(r'^from:0x([0-9a-f]+) kind:(\S+) to:0x([0-9a-f]+) module:(\S+)')


class Module:
    def __init__(self, side, name):
        rom = os.path.join(EX, 'rom') if side == 'demo' else os.path.join(EX, 'retail', 'rom')
        cfg = os.path.join(EX, 'config') if side == 'demo' else os.path.join(EX, 'retail', 'config')
        if name == 'main':
            binp, cdir = os.path.join(rom, 'arm9', 'arm9.bin'), os.path.join(cfg, 'arm9')
            yml = os.path.join(rom, 'arm9', 'arm9.yaml')
        elif name in ('itcm', 'dtcm'):
            binp, cdir = os.path.join(rom, 'arm9', name + '.bin'), os.path.join(cfg, 'arm9', name)
            yml = os.path.join(rom, 'arm9', name + '.yaml')
        else:
            binp, cdir = os.path.join(rom, 'arm9_overlays', name + '.bin'), os.path.join(cfg, 'arm9', 'overlays', name)
            yml = None
        self.name, self.data = name, open(binp, 'rb').read()
        if yml:
            self.base = int(re.search(r'base_address: (\d+)', open(yml).read()).group(1))
        else:
            ovs = open(os.path.join(rom, 'arm9_overlays', 'overlays.yaml')).read()
            m = re.search(r'id: %d\n\s+base_address: (\d+)' % int(name[2:]), ovs)
            self.base = int(m.group(1))
        self.funcs = []  # (addr, mode, size, name)
        sp = os.path.join(cdir, 'symbols.txt')
        if os.path.exists(sp):
            for line in open(sp):
                m = FUNC.match(line)
                if m:
                    self.funcs.append((int(m.group(4), 16), m.group(2), int(m.group(3), 16), m.group(1)))
        self.funcs.sort()
        self.relocs = {}
        rp = os.path.join(cdir, 'relocs.txt')
        if os.path.exists(rp):
            for line in open(rp):
                m = RELOC.match(line)
                if m:
                    self.relocs[int(m.group(1), 16)] = (m.group(2), int(m.group(3), 16), m.group(4))

    def body(self, f):
        o = f[0] - self.base
        return self.data[o:o + f[2]]

    def masked(self, f):
        """the function's bytes with relocated fields zeroed, plus its relocations as (offset, kind, target, module)"""
        b = bytearray(self.body(f))
        rel = []
        for a in range(f[0], f[0] + f[2], 2):
            r = self.relocs.get(a)
            if not r:
                continue
            o = a - f[0]
            kind = r[0]
            if kind == 'arm_call' or kind == 'arm_call_thumb':
                b[o:o + 3] = b'\0\0\0'        # BL/BLX offset (low 24 bits)
                if kind == 'arm_call_thumb':
                    b[o + 3] &= 0xfe            # BLX: the H bit is part of the offset
            elif kind.startswith('thumb_call'):
                b[o:o + 4] = bytes([0, b[o + 1] & 0xf8, 0, b[o + 3] & 0xf8])  # BL/BLX pair offsets
            else:
                b[o:o + 4] = b'\0\0\0\0'        # an address (literal pool or data)
            rel.append((o, kind, r[1], r[2]))
        return bytes(b), rel


def match_module(d, r):
    dsig = [(f, *d.masked(f)) for f in d.funcs]
    rsig = [(f, *r.masked(f)) for f in r.funcs]
    key = lambda s: (s[0][1], s[0][2], s[1])
    rby = collections.defaultdict(list)
    for i, s in enumerate(rsig):
        rby[key(s)].append(i)
    dby = collections.defaultdict(list)
    for i, s in enumerate(dsig):
        dby[key(s)].append(i)
    m = {}  # demo index -> (retail index, how)
    for k, di in dby.items():
        ri = rby.get(k, [])
        if len(di) == 1 and len(ri) == 1:
            m[di[0]] = (ri[0], 'exact')
    # order constraint: anchors must increase on both sides (drop the ones that cross)
    def monotone(pairs):
        pairs = sorted(pairs)
        # longest increasing subsequence on the retail index
        import bisect
        tails, prev, idx = [], [None] * len(pairs), []
        for n, (a, b) in enumerate(pairs):
            k = bisect.bisect_left([pairs[t][1] for t in tails], b)
            if k == len(tails):
                tails.append(n)
            else:
                tails[k] = n
            prev[n] = tails[k - 1] if k else None
        out, n = [], tails[-1] if tails else None
        while n is not None:
            out.append(pairs[n])
            n = prev[n]
        return set(out[::-1])
    keep = monotone([(a, b) for a, (b, _) in m.items()])
    m = {a: (b, h) for a, (b, h) in m.items() if (a, b) in keep}

    def fill(how_exact):
        anchors = sorted((a, b) for a, (b, _) in m.items())
        bounds = [(-1, -1)] + anchors + [(len(dsig), len(rsig))]
        for (a0, b0), (a1, b1) in zip(bounds, bounds[1:]):
            ds = [i for i in range(a0 + 1, a1) if i not in m]
            rs = list(range(b0 + 1, b1))
            used = set(b for b, _ in m.values())
            rs = [j for j in rs if j not in used]
            if not ds or not rs:
                continue
            if how_exact:
                # repeated code: same masked bytes, taken in order
                j = 0
                for i in ds:
                    while j < len(rs) and key(rsig[rs[j]]) != key(dsig[i]):
                        j += 1
                    if j < len(rs):
                        m[i] = (rs[j], 'order')
                        j += 1
            else:
                # different code: pair by order with a byte similarity score (equal counts), or best scores
                sm = difflib.SequenceMatcher(autojunk=False)
                if len(ds) == len(rs):
                    for i, j in zip(ds, rs):
                        sm.set_seqs(dsig[i][1], rsig[j][1])
                        m[i] = (j, 'similar:%.2f' % sm.ratio())
                else:
                    taken = set()
                    for i in ds:
                        best = None
                        for j in rs:
                            if j in taken or rsig[j][0][1] != dsig[i][0][1]:
                                continue
                            a, b = dsig[i][1], rsig[j][1]
                            if abs(len(a) - len(b)) > max(len(a), len(b)) // 2:
                                continue
                            sm.set_seqs(a, b)
                            s = sm.quick_ratio()
                            if s >= 0.6:
                                s = sm.ratio()
                                if s >= 0.6 and (best is None or s > best[0]):
                                    best = (s, j)
                        if best:
                            taken.add(best[1])
                            m[i] = (best[1], 'similar:%.2f' % best[0])
    fill(True)
    fill(False)
    return dsig, rsig, m


def main():
    import argparse
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from extract_rom import find_dsd, run, write_host_tables
    ap = argparse.ArgumentParser()
    ap.add_argument('--rom', required=True)
    ap.add_argument('--dsd')
    args = ap.parse_args()
    rdir = os.path.join(EX, 'retail')
    if not os.path.exists(os.path.join(rdir, 'config', 'arm9', 'symbols.txt')):
        dsd = find_dsd(args.dsd)
        run([dsd, 'rom', 'extract', '-r', args.rom, '-o', os.path.join(rdir, 'rom')])
        run([dsd, 'init', '-r', os.path.join(rdir, 'rom', 'config.yaml'), '-o', os.path.join(rdir, 'config'),
             '-b', os.path.join(rdir, 'dsd-build')])
    names = ['main', 'itcm'] + ['ov%03d' % i for i in range(30)]
    out = []
    totals = collections.Counter()
    addr_map = {}  # (demo module, demo addr) -> retail addr
    results = []
    for n in names:
        d, r = Module('demo', n), Module('retail', n)
        dsig, rsig, m = match_module(d, r)
        results.append((n, d, r, dsig, rsig, m))
        for i, s in enumerate(dsig):
            if i in m:
                addr_map[(n, s[0][0])] = rsig[m[i][0]][0][0]
    # call check: in each matched pair, calls to matched functions should go to the same retail function
    bad_calls = collections.Counter()
    for n, d, r, dsig, rsig, m in results:
        c = collections.Counter()
        for i, s in enumerate(dsig):
            how = m[i][1] if i in m else 'none'
            if i in m:
                rs = rsig[m[i][0]]
                rf = rs[0]
                rcalls = {o: (t, mod) for o, k, t, mod in rs[2] if 'call' in k}
                for o, k, t, mod in s[2]:
                    if 'call' not in k:
                        continue
                    want = addr_map.get(('main' if mod == 'main' else mod, t))
                    got = rcalls.get(o)
                    if want is not None and got is not None and got[0] != want and how.startswith('similar') is False:
                        bad_calls[n] += 1
                out.append('%s 0x%08x 0x%08x %s 0x%x 0x%x' % (n, s[0][0], rf[0], how, s[0][2], rf[2]))
            else:
                out.append('%s 0x%08x - none 0x%x' % (n, s[0][0], s[0][2]))
            c[how.split(':')[0]] += 1
        sim = [float(m[i][1].split(':')[1]) for i in m if m[i][1].startswith('similar')]
        print('%-5s demo %4d retail %4d  exact %4d  order %3d  similar %3d (>=0.9: %3d)  none %3d  call mismatches %d'
              % (n, len(dsig), len(rsig), c['exact'], c['order'], c['similar'], sum(x >= 0.9 for x in sim), c['none'],
                 bad_calls[n]))
        totals.update(c)
        totals['demo'] += len(dsig)
    print('total: %d demo functions: %d exact, %d order, %d similar, %d unmatched'
          % (totals['demo'], totals['exact'], totals['order'], totals['similar'], totals['none']))
    with open(os.path.join(EX, 'retail', 'match.txt'), 'w') as f:
        f.write('\n'.join(out) + '\n')
    symbol_map(results)
    # the loader's module list and pointer fix-ups for the retail images, the header and where the ROM is
    write_host_tables(rdir, os.path.join(rdir, 'rom'), os.path.join(rdir, 'config'))
    with open(args.rom, 'rb') as f, open(os.path.join(rdir, 'host', 'header.bin'), 'wb') as h:
        h.write(f.read(0x200))
    with open(os.path.join(rdir, 'host', 'rom_path.txt'), 'w') as f:
        f.write(os.path.abspath(args.rom) + '\n')


DATA_RE = re.compile(r'\b(?:data|bss)_(?:ov([0-9]+)_)?([0-9a-f]{8})(?:__\d+)?\b')


def reloc_module(mod):
    """dsd's target module ('main', 'itcm', 'dtcm', 'overlay(3)') as a module name, or None if ambiguous"""
    m = re.match(r'overlay\((\d+)\)$', mod)
    return 'ov%03d' % int(m.group(1)) if m else (mod if mod in ('main', 'itcm', 'dtcm') else None)


def symbol_map(results):
    """extracted/retail/symbols.txt: the retail address of every func_/data_/bss_ name the C links against.
    Functions from the matches; data from the loaded addresses (literal-pool words) of matching code, per module,
    or the same offset from the closest such address below (within 0x100, bytes checked where both are initialised
    data); hardware and system-area addresses are the same in both."""
    mods = {n: (d, r) for n, d, r, _, _, _ in results}
    pairs = collections.defaultdict(collections.Counter)  # (module, demo addr) -> retail addrs
    for n, d, r, dsig, rsig, m in results:
        for i, s in enumerate(dsig):
            if i not in m or m[i][1].startswith('similar'):
                continue
            rl = {o: (t, mod) for o, k, t, mod in rsig[m[i][0]][2] if k == 'load'}
            for o, k, t, mod in s[2]:
                if k == 'load' and o in rl:
                    dm, rm = reloc_module(mod), reloc_module(rl[o][1])
                    if dm and dm == rm:
                        pairs[(dm, t)][rl[o][0]] += 1
    conflicts = sum(len(c) > 1 for c in pairs.values())
    best = {k: c.most_common(1)[0][0] for k, c in pairs.items()}
    # changed functions: the n-th loaded address on each side, when both load the same number (lower priority)
    weak = collections.defaultdict(collections.Counter)
    for n, d, r, dsig, rsig, m in results:
        for i, s in enumerate(dsig):
            if i not in m or not m[i][1].startswith('similar'):
                continue
            dl = [(t, mod) for o, k, t, mod in s[2] if k == 'load']
            rl = [(t, mod) for o, k, t, mod in rsig[m[i][0]][2] if k == 'load']
            if len(dl) != len(rl):
                continue
            for (t, mod), (rt, rmod) in zip(dl, rl):
                dm, rm = reloc_module(mod), reloc_module(rmod)
                if dm and dm == rm and (dm, t) not in best:
                    weak[(dm, t)][rt] += 1
    for k, c in weak.items():
        if len(c) == 1:
            best[k] = next(iter(c))
    bymod = collections.defaultdict(list)
    for (mod, a) in best:
        bymod[mod].append(a)
    for v in bymod.values():
        v.sort()
    import bisect
    known = {}
    for line in open(os.path.join(EX, 'data_symbols.txt')):
        p = line.split()
        if len(p) >= 3 and p[2].startswith('arm9'):
            known[p[0]] = 'main' if p[2] == 'arm9' else p[2].split('/')[-1]

    fpairs = collections.defaultdict(list)  # module -> [(demo function, retail function)] with the same code
    for n, d, r, dsig, rsig, m in results:
        for i, s in enumerate(dsig):
            if i in m and not m[i][1].startswith('similar'):
                fpairs[n].append((s[0], rsig[m[i][0]][0]))
    masked_cache = {}

    def masked_data(md):
        """the module's bytes with every aligned word that looks like a DS RAM address zeroed"""
        if id(md) not in masked_cache:
            b = bytearray(md.data)
            for o in range(0, len(b) - 3, 4):
                w = struct.unpack_from('<I', b, o)[0]
                if 0x01ff8000 <= w < 0x02400000 or 0x027c0000 <= w < 0x02800000:
                    b[o:o + 4] = b'\0\0\0\0'
            masked_cache[id(md)] = bytes(b)
        return masked_cache[id(md)]

    def initialised(side, mod, a):
        mo = mods.get(mod)
        if not mo:
            return None
        md = mo[0] if side == 0 else mo[1]
        o = a - md.base
        return md.data[o:o + 8] if 0 <= o and o + 8 <= len(md.data) else None

    def resolve(name, ov, a):
        if a >= 0x027ff000 or a < 0x01ff8000:
            return a, 'same'
        if ov is not None:
            cands = ['ov%03d' % int(ov)]
        elif name in known:
            cands = [known[name]]
        elif a < 0x02000000:
            cands = ['itcm']
        elif 0x027c0000 <= a < 0x027c4000:
            cands = ['dtcm']
        else:
            cands = ['main'] + ['ov%03d' % i for i in range(30)]
        found = set()
        for mod in cands:
            if (mod, a) in best:
                found.add((best[(mod, a)], 'direct'))
                continue
            v = bymod.get(mod, [])
            k = bisect.bisect_right(v, a) - 1
            if k >= 0 and a - v[k] < 0x100:
                ra = best[(mod, v[k])] + (a - v[k])
                db, rb = initialised(0, mod, a), initialised(1, mod, ra)
                if db is None or rb is None or db == rb:
                    found.add((ra, 'near'))
        if len(set(f[0] for f in found)) == 1:
            return found.pop()
        if found:
            return None, 'ambiguous'
        for mod in cands:
            mo = mods.get(mod)
            if not mo:
                continue
            if a == mo[0].base:
                return mo[1].base, 'base'
            # inside a function with the same code (a jump table or literal pool): the same offset in its match
            for f, rf in fpairs.get(mod, ()):
                if f[0] <= a < f[0] + f[2] and f[2] == rf[2]:
                    return rf[0] + (a - f[0]), 'in-function'
        # constant data that no matching code loads directly (tables, strings): the demo's bytes from that address,
        # found once in the retail module (the shortest window that is unique, aligned like the original), with
        # pointer-sized words left out of the comparison (their targets moved)
        for mod in cands:
            mo = mods.get(mod)
            if not mo:
                continue
            o = a - mo[0].base
            dm, rm = masked_data(mo[0]), masked_data(mo[1])
            for n in (16, 32, 64, 128):
                w = dm[o:o + n]
                if len(w) < n or w.count(w[:1]) == n:
                    break
                hits = [h for h in re.finditer(re.escape(w), rm) if h.start() % 4 == o % 4]
                if len(hits) == 1:
                    return mo[1].base + hits[0].start(), 'bytes'
                if not hits:
                    break
        return None, 'none'

    names = set()
    for base in ('src', os.path.join('port', 'hostfuncs')):
        for dp, _, fs in os.walk(os.path.join(ROOT, base)):
            for fn in fs:
                if fn.endswith('.c'):
                    names.update(m.group(0) for m in DATA_RE.finditer(open(os.path.join(dp, fn), errors='replace').read()))
    out, how, missing = [], collections.Counter(), []
    for name in sorted(names):
        m = DATA_RE.match(name)
        ra, h = resolve(name, m.group(1), int(m.group(2), 16))
        how[h] += 1
        if ra is None:  # only demo-only code uses these (e.g. the kiosk reset): keep the demo address
            missing.append(name)
            out.append('%s 0x%s unresolved' % (name, m.group(2)))
        else:
            out.append('%s 0x%08x' % (name, ra))
    # functions: every matched demo function under the name the C gives it
    nf = 0
    for n, d, r, dsig, rsig, m in results:
        for i, s in enumerate(dsig):
            if i in m:
                a = s[0][0]
                name = 'func_%s%08x' % ('' if n in ('main', 'itcm') else n + '_', a)
                out.append('%s 0x%08x' % (name, rsig[m[i][0]][0][0]))
                nf += 1
    print('data: %d (module, address) pairs, %d conflicting; %d data names: %s; unresolved: %s'
          % (len(best), conflicts, len(names), dict(how), ' '.join(missing) or 'none'))
    print('functions: %d names with a retail address' % nf)
    with open(os.path.join(EX, 'retail', 'symbols.txt'), 'w') as f:
        f.write('\n'.join(out) + '\n')


if __name__ == '__main__':
    main()
