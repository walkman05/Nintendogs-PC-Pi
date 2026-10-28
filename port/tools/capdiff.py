"""Replay a captured call (cap_<name>_<n>.bin from a capture build: NDOGS_WRAP + NDOGS_CAPTURE, see host_compile.py)
on the original ARM code under Unicorn, from the same memory, and compare everything it writes with what the host
routine wrote.

usage: python port/tools/capdiff.py cap_func_xxx_N.bin [...]"""
import os, re, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from armrun import Arm, STOP  # noqa: E402
from unicorn import UC_PROT_ALL, UC_HOOK_MEM_UNMAPPED, UC_HOOK_CODE  # noqa: E402
from unicorn.arm_const import (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_SP,  # noqa
                               UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_CPSR)

REGIONS = [(0x01ff8000, 0x8000), (0x02000000, 0x400000), (0x027c0000, 0x10000), (0x027f0000, 0x10000),
           (0x037f0000, 0x10000)]
STACK = 0x0e000000
# written asynchronously while the host routine runs (interrupts, the ARM7 stand-in): not the routine's doing
ASYNC = [(0x02155fdc, 1, 'OS tick counter'), (0x027fffa8, 8, 'ARM7 touch/keys')]
ARGS = [UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3]


def load(path):
    """[(header, {base: bytes}), ...] for before and after; the last region is a window of the host stack"""
    d = open(path, 'rb').read()
    out, off = [], 0
    for k in range(2):
        hdr = struct.unpack('<12I', d[off:off + 48])
        off += 48
        mem = {}
        for base, s in REGIONS:
            mem[base] = d[off:off + s]
            off += s
        while off + 8 <= len(d) and struct.unpack('<I', d[off:off + 4])[0] != 0x50414344:  # extra windows
            wbase, wsize = struct.unpack('<2I', d[off:off + 8])
            off += 8
            mem[wbase] = d[off:off + wsize]
            off += wsize
        out.append((hdr, mem))
    return out


def host_functions(mapfile):
    """host address -> ARM address of every func_ symbol in the linker map"""
    out = {}
    for l in open(mapfile, errors='replace'):
        m = re.match(r'\s+0x([0-9a-f]+)\s+_?(?:__wrap_)?func_(?:ov\d+_)?([0-9a-f]{8})\s*$', l)
        if m:
            out[int(m.group(1), 16)] = int(m.group(2), 16)
    return out


def addr_of(name):
    m = re.match(r'func_(?:ov\d+_)?([0-9a-f]{8})$', name)
    return int(m.group(1), 16)


def ranges(a, b, base):
    """differing byte ranges between two buffers"""
    out, i, n = [], 0, len(a)
    while i < n:
        if a[i] != b[i]:
            j = i
            while j < n and (a[j] != b[j] or (j + 1 < n and a[j + 1] != b[j + 1]) or (j + 2 < n and a[j + 2] != b[j + 2])):
                j += 1
            out.append((base + i, j - i))
            i = j
        else:
            i += 1
    return out


def replay(path, arm):
    name = re.match(r'cap_(func_\w+?)_(\d+)\.bin$', os.path.basename(path)).group(1)
    (bh, before), (ah, after) = load(path)
    uc = arm.uc
    uc.context_restore(arm.snapshot)
    for base, data in before.items():
        page = base & ~0xfff
        try:
            uc.mem_map(page, ((base + len(data) + 0xfff) & ~0xfff) - page, UC_PROT_ALL)
        except Exception:
            pass  # already mapped
        uc.mem_write(base, data)
    uc.reg_write(UC_ARM_REG_CPSR, 0x1f)
    args = bh[4:12]
    for i in range(4):
        uc.reg_write(ARGS[i], args[i])
    sp = STACK + 0x8000
    uc.mem_write(sp, struct.pack('<4I', *args[4:8]))  # arguments 5-8 on the stack
    uc.reg_write(UC_ARM_REG_SP, sp)
    uc.reg_write(UC_ARM_REG_LR, STOP)
    try:
        pc = addr_of(name)
        for _ in range(100000):
            arm.redirect = None
            uc.emu_start(pc, STOP, count=50000000)
            if arm.redirect is None:
                break
            pc = arm.redirect  # entered a host function (code pointer in game data): continue at its original
        if uc.reg_read(UC_ARM_REG_PC) != STOP:
            return name, 'ARM did not return (pc %08x)' % uc.reg_read(UC_ARM_REG_PC), None
    except Exception as e:
        from unicorn.arm_const import UC_ARM_REG_R4, UC_ARM_REG_R12
        regs = [uc.reg_read(r) for r in ARGS] + [uc.reg_read(r) for r in range(UC_ARM_REG_R4, UC_ARM_REG_R12 + 1)]
        return name, 'ARM error %s at pc %08x (lr %08x, r0-r12 %s)' % (
            e, uc.reg_read(UC_ARM_REG_PC), uc.reg_read(UC_ARM_REG_LR), ' '.join('%08x' % x for x in regs)), None
    diffs = []
    for base, data in after.items():
        if base not in before or base == 0x04000000:  # registers: input only (FIFO writes would be noise)
            continue
        arm_mem = bytes(uc.mem_read(base, len(data)))
        for a, n in ranges(arm_mem, data, base):
            diffs.append((a, n, arm_mem[a - base:a - base + min(n, 32)], data[a - base:a - base + min(n, 32)]))
    # what the routine wrote at all (host): context for the diffs
    wrote = sum(n for base, data in after.items() if base in before for a, n in ranges(before[base], data, base))
    r0 = uc.reg_read(UC_ARM_REG_R0)
    return name, 'args %s, host wrote %d bytes, r0 arm %08x host %08x' % (
        ' '.join('%08x' % x for x in args[:6]), wrote, r0, ah[2]), diffs


def arm_trace(arm, path, names):
    """replay like replay(), recording calls of the named routines: [(name, r0, r1, r2, r3)]"""
    calls, hooks = [], []
    for n in names:
        addr = addr_of(n)
        hooks.append(arm.uc.hook_add(UC_HOOK_CODE, lambda uc, a, size, nm: calls.append(
            (nm,) + tuple(uc.reg_read(r) for r in ARGS) + (bytes(uc.mem_read(arm.obj, 0xa00)),)), n, begin=addr, end=addr))
    result = replay(path, arm)
    for h in hooks:
        arm.uc.hook_del(h)
    return calls, result


def compare_traces(arm, path):
    tpath = path[:-4] + '.trace'
    host = [l.split() for l in open(tpath)]
    host = [(h[0],) + tuple(int(x, 16) for x in h[1:5]) + (bytes.fromhex(h[5]),) for h in host]
    arm.obj = load(path)[0][0][4]
    names = sorted(set(h[0] for h in host) | set(n for n in open(os.environ.get('NDOGS_TRACE_NAMES', 'callees.txt')).read().split(',') if n.strip()))
    names = [n.strip() for n in names if re.match(r'func_(?:ov\d+_)?[0-9a-f]{8}$', n.strip())]
    # FX_Div / FX_Inv inner routines: the host's guarded versions compute directly instead of calling them
    names = [n for n in names if n not in ('func_02067898', 'func_0206787c')]
    host = [h for h in host if h[0] not in ('func_02067898', 'func_0206787c')]
    arm_calls, _ = arm_trace(arm, path, names)
    top = re.match(r'cap_(func_\w+?)_\d+\.bin$', os.path.basename(path)).group(1)
    if arm_calls and arm_calls[0][0] == top:
        arm_calls = arm_calls[1:]  # the captured routine's own entry
    print('%s: host %d calls, arm %d calls' % (os.path.basename(path), len(host), len(arm_calls)))
    # align: the ARM trace may hold extra calls (recursion and helpers that the host routines make directly, which
    # the linker wrappers can't see); each host call is matched with the next ARM call of the same name
    fmt = lambda c: '%s(%s)' % (c[0], ' '.join('%08x' % x for x in c[1:5])) if c else '-'
    j = 0
    for i, h in enumerate(host):
        k = j
        while k < len(arm_calls) and k < j + 40 and arm_calls[k][0] != h[0]:
            k += 1
        if k >= len(arm_calls) or arm_calls[k][0] != h[0]:
            print('  !! host call %d %s has no ARM counterpart (ARM next: %s)' % (i, fmt(h), fmt(arm_calls[j] if j < len(arm_calls) else None)))
            return
        a = arm_calls[k]
        if a[5] != h[5]:
            offs = [x for x in range(0xa00) if a[5][x] != h[5][x]]
            print('  object differs at host call %d %s (ARM call %d): %s' % (i, fmt(h), k, ' '.join(
                '+%x:%02x/%02x' % (x, a[5][x], h[5][x]) for x in offs[:24])))
            if i:
                print('     previous call: %s' % fmt(host[i - 1]))
            return
        j = k + 1
    print('  traces agree; object identical at every call')


def main():
    arm = Arm()
    arm.uc.mem_map(0x027c0000, 0x10000, UC_PROT_ALL)
    arm.uc.mem_map(0x037f0000, 0x10000, UC_PROT_ALL)
    arm.uc.mem_map(STACK, 0x10000, UC_PROT_ALL)
    for base, size in ((0x04002000, 0xffe000), (0x05000000, 0x10000), (0x06000000, 0x900000), (0x07000000, 0x10000)):
        arm.uc.mem_map(base, size, UC_PROT_ALL)  # I/O, palettes, VRAM, OAM: present but not modelled
    # code pointers in game data point at the host's compiled C: entering one continues at the original (a code
    # hook over the host range; host addresses need not be word aligned, ARM fetches round them down)
    mapfile = os.environ.get('NDOGS_MAP', 'objs_cap/nintendogs_cap.exe.map')
    funcs = host_functions(mapfile)
    lo, hi = min(funcs) & ~0xfff, (max(funcs) + 0x1fff) & ~0xfff
    arm.uc.mem_map(lo, hi - lo, UC_PROT_ALL)

    def enter_host(uc, addr, size, _):
        for a in (addr, addr + 1, addr + 2, addr + 3):
            if a in funcs:
                arm.redirect = funcs[a]
                uc.emu_stop()
                return
        arm.redirect = None
        uc.emu_stop()  # not a function entry: stops with the pc in host code (reported as not returning)
    arm.uc.hook_add(UC_HOOK_CODE, enter_host, None, lo, hi - 1)
    arm.snapshot = arm.uc.context_save()
    if sys.argv[1] == '--trace':
        for path in sys.argv[2:]:
            compare_traces(arm, path)
        return
    for path in sys.argv[1:]:
        name, info, diffs = replay(path, arm)
        if diffs:
            asyn = [d for d in diffs if any(b <= d[0] < b + n for b, n, _ in ASYNC)]
            diffs = [d for d in diffs if d not in asyn]
            info += ' (%d async ranges ignored)' % len(asyn) if asyn else ''
        verdict = 'FAILED TO RUN' if diffs is None else 'MATCH' if not diffs else '%d differing ranges' % len(diffs)
        print('%s: %s -> %s' % (os.path.basename(path), info, verdict))
        diffs = diffs or []
        for a, n, x, y in diffs[:12]:
            print('   %08x +%d: arm %s | host %s' % (a, n, x.hex(), y.hex()))


if __name__ == '__main__':
    main()
