"""Full-game data from the user's own retail cartridge (Nintendogs: Dachshund & Friends, Europe).

The kiosk demo's file system has the full game's files, except that most text was blanked out (main menu, shop,
item and NPC-dog messages) and a few files were left out (some dog icons, items, accessories). This builds
extracted/host/full.nds: the demo ROM with
  - every message file rebuilt in the demo's layout (same message IDs and order) with the retail text, by ID;
  - the files only the retail cartridge has added to the file system;
plus extracted/host/full_header.bin. The port uses them in full-game mode (port/src/main.c). Nothing here is
committed: both inputs are the user's ROMs and the output is derived from them.

usage: python port/tools/make_full_rom.py --demo baserom.nds --retail <retail.nds> [--out extracted]
"""
import argparse, os, struct


def read_fs(rom):
    """{path: bytes} of the NitroFS files, plus the overlay file IDs (FAT entries below the first named file)."""
    fnt, _, fat, _ = struct.unpack_from('<IIII', rom, 0x40)
    files = {}
    first_named = [None]

    def walk(dir_id, path):
        sub_off, fid, _ = struct.unpack_from('<IHH', rom, fnt + (dir_id & 0xfff) * 8)
        tree[path] = ([], [], dir_id)  # subdirectories, files (in table order), directory ID
        if first_named[0] is None or fid < first_named[0]:
            first_named[0] = fid
        p = fnt + sub_off
        while True:
            b = rom[p]
            p += 1
            if b == 0:
                break
            name = rom[p:p + (b & 0x7f)].decode('latin1')
            p += b & 0x7f
            if b & 0x80:
                tree[path][0].append(name)
                walk(struct.unpack_from('<H', rom, p)[0], path + name + '/')
                p += 2
            else:
                s, e = struct.unpack_from('<II', rom, fat + fid * 8)
                files[path + name] = bytes(rom[s:e])
                where[path + name] = (s, e)
                tree[path][1].append(name)
                fid += 1

    where, tree = {}, {}
    walk(0xf000, '')
    read_fs.tree = tree
    overlays = [struct.unpack_from('<II', rom, fat + i * 8) for i in range(first_named[0])]
    read_fs.where = where  # each file's (start, end) in the ROM
    return files, overlays


def bmg_sections(d):
    secs, o = {}, 0x20
    for _ in range(struct.unpack_from('<I', d, 12)[0]):
        if o + 8 > len(d):  # some files count a section they don't have
            break
        mag, sz = d[o:o + 4], struct.unpack_from('<I', d, o + 4)[0]
        secs[mag] = (o, sz)
        o += sz
    return secs


def bmg_messages(d):
    """{message id: raw UTF-16 bytes incl. control codes, without the terminator}"""
    secs = bmg_sections(d)
    io, _ = secs[b'INF1']
    n, esz = struct.unpack_from('<HH', d, io + 8)
    do, dsz = secs[b'DAT1']
    dat = d[do + 8:do + dsz]
    mo, _ = secs[b'MID1']
    ids = struct.unpack_from('<%dI' % n, d, mo + 16)
    out = {}
    for i in range(n):
        off = e = struct.unpack_from('<I', d, io + 16 + i * esz)[0]
        while e + 2 <= len(dat):
            c = struct.unpack_from('<H', dat, e)[0]
            if c == 0:
                break
            # control code: 0x1a, then a byte with the code's whole size (it may contain zero halfwords)
            e += max(dat[e + 2], 4) if c == 0x1a else 2
        out[ids[i]] = dat[off:e]
    return out


def merge_bmg(demo, retail):
    """the demo file's layout (INF1 attributes, MID1, order) with each message taken from the retail file by ID"""
    r = bmg_messages(retail)
    secs = bmg_sections(demo)
    io, isz = secs[b'INF1']
    n, esz = struct.unpack_from('<HH', demo, io + 8)
    mo, _ = secs[b'MID1']
    ids = struct.unpack_from('<%dI' % n, demo, mo + 16)
    d = bmg_messages(demo)
    inf = bytearray(demo[io:io + isz])
    dat = bytearray(b'\0\0')  # offset 0: the empty message
    for i, mid in enumerate(ids):
        s = r.get(mid) or d.get(mid) or b''
        off = 0
        if s:
            off = len(dat)
            dat += s + b'\0\0'
        struct.pack_into('<I', inf, 16 + i * esz, off)
    dat1 = b'DAT1' + struct.pack('<I', 0) + bytes(dat)
    dat1 += b'\0' * (-len(dat1) % 32)
    dat1 = dat1[:4] + struct.pack('<I', len(dat1)) + dat1[8:]
    body = bytes(inf) + dat1
    for mag, (o, sz) in sorted(secs.items(), key=lambda kv: kv[1][0]):
        if mag not in (b'INF1', b'DAT1'):
            body += demo[o:o + sz]
    head = bytearray(demo[:0x20])
    struct.pack_into('<I', head, 8, 0x20 + len(body))
    return bytes(head) + body


def build_fnt(paths, first_id, base):
    """FNT for the file paths, keeping the directory and file order of `base` (the demo's tree from read_fs; the
    game relies on it) and appending new files to their directories; returns (fnt bytes, [paths in file ID order])"""
    tree = {d: (list(t[0]), list(t[1])) for d, t in base.items()}  # dir path -> (subdirs, files)
    dirs = sorted(base, key=lambda d: base[d][2])
    for p in paths:
        parts = p.split('/')
        d = ''
        for name in parts[:-1]:
            sub = d + name + '/'
            if sub not in tree:
                tree[sub] = ([], [])
                tree[d][0].append(name)
                dirs.append(sub)
            d = sub
        if parts[-1] not in tree[d][1]:
            tree[d][1].append(parts[-1])
    dir_id = {d: 0xf000 + i for i, d in enumerate(dirs)}
    parent = {d + s + '/': d for d in dirs for s in tree[d][0]}
    order, subs = [], []
    fid = first_id
    for d in dirs:
        t = bytearray()
        first = fid
        for name in tree[d][1]:  # files first, then subdirectories
            nb = name.encode('latin1')
            t += bytes([len(nb)]) + nb
            order.append(d + name)
            fid += 1
        for name in tree[d][0]:
            nb = name.encode('latin1')
            t += bytes([0x80 | len(nb)]) + nb + struct.pack('<H', dir_id[d + name + '/'])
        t += b'\0'
        subs.append((first, t))
    main = bytearray()
    off = len(dirs) * 8
    blob = bytearray()
    for i, (d, (first, t)) in enumerate(zip(dirs, subs)):
        third = len(dirs) if i == 0 else dir_id[parent[d]]
        main += struct.pack('<IHH', off + len(blob), first, third)
        blob += t
    return bytes(main + blob), order


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--demo', required=True)
    ap.add_argument('--retail', required=True)
    ap.add_argument('--out', default='extracted')
    args = ap.parse_args()
    demo = bytearray(open(args.demo, 'rb').read())
    retail = open(args.retail, 'rb').read()
    dfiles, overlays = read_fs(demo)
    where, demo_tree = read_fs.where, read_fs.tree
    rfiles, _ = read_fs(retail)

    files = dict(dfiles)
    merged = added = 0
    for p, data in rfiles.items():
        if p in files:
            if p.endswith('.bmg') and data[:8] == b'MESGbmg1' and files[p] != data:
                files[p] = merge_bmg(files[p], data)
                merged += 1
        else:
            files[p] = data
            added += 1
    # the demo keeps untranslated copies at Message/*.bmg; give them the English text too
    for p in list(files):
        if p.startswith('Message/') and p.count('/') == 1 and 'Message/Eng/' + p[8:] in rfiles:
            files[p] = merge_bmg(dfiles[p], rfiles['Message/Eng/' + p[8:]])

    paths = sorted(files, key=lambda p: [s.lower() for s in p.split('/')])
    fnt, order = build_fnt(paths, len(overlays), demo_tree)
    # new tables and new or changed files go after the demo's data (within the 64 MB card); unchanged files stay put
    out = demo[:struct.unpack_from('<I', demo, 0x80)[0]]
    out += b'\xff' * (-len(out) % 0x200)
    fnt_off = len(out)
    out += fnt
    out += b'\xff' * (-len(out) % 0x200)
    fat_off = len(out)
    fat = bytearray()
    for s, e in overlays:  # overlays stay where they are
        fat += struct.pack('<II', s, e)
    out += b'\0' * (8 * (len(overlays) + len(order)))
    out += b'\xff' * (-len(out) % 0x200)
    for p in order:
        data = files[p]
        if p in dfiles and data is dfiles[p]:
            fat += struct.pack('<II', *where[p])
            continue
        s = len(out)
        out += data
        fat += struct.pack('<II', s, s + len(data))
        out += b'\xff' * (-len(out) % 0x200)
    out[fat_off:fat_off + len(fat)] = fat
    struct.pack_into('<IIII', out, 0x40, fnt_off, len(fnt), fat_off, len(fat))
    struct.pack_into('<I', out, 0x80, len(out))
    if len(out) > 64 << 20:
        raise SystemExit('the full image does not fit in a 64 MB card (%d bytes)' % len(out))
    out += b'\xff' * ((64 << 20) - len(out))
    host = os.path.join(args.out, 'host')
    os.makedirs(host, exist_ok=True)
    with open(os.path.join(host, 'full.nds'), 'wb') as f:
        f.write(out)
    with open(os.path.join(host, 'full_header.bin'), 'wb') as f:
        f.write(out[:0x170])
    print('full.nds: %d files (%d added from the retail cartridge, %d message files merged), %.1f MB'
          % (len(order), added, merged, len(out) / 1e6))


if __name__ == '__main__':
    main()
