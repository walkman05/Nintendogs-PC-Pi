"""Run original ARM9 code from the extracted ROM under Unicorn, as the reference for differential tests of the
port's host routines (port/tools/difftest.py).

ARM9 main RAM (arm9.bin at 0x02000000), ITCM (0x01ff8000) and DTCM (0x027e0000) are loaded from extracted/rom; the
hardware divider and square root (0x04000280-0x040002bc) are modelled, since the runtime uses them. A call puts up
to four arguments in r0-r3, returns to a stop address in lr, and gives back r0 and r1."""
import os
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_MEM_WRITE, UC_HOOK_MEM_READ, UC_HOOK_CODE, UC_PROT_ALL
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_SP, \
    UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_CPSR, UC_ARM_REG_C1_C0_2

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ROM = os.path.join(ROOT, 'extracted', 'rom', 'arm9')
STOP = 0x0ff00000
REGS = [UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3]


def _s64(v):
    v &= (1 << 64) - 1
    return v - (1 << 64) if v >> 63 else v


def _s32(v):
    v &= 0xffffffff
    return v - (1 << 32) if v >> 31 else v


class Arm:
    def __init__(self):
        uc = self.uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        for base, size in ((0x01ff8000, 0x8000), (0x02000000, 0x400000), (0x027e0000, 0x20000),
                           (0x04000000, 0x2000), (STOP, 0x1000)):
            uc.mem_map(base, size, UC_PROT_ALL)
        for name, base in (('arm9.bin', 0x02000000), ('itcm.bin', 0x01ff8000), ('dtcm.bin', 0x027e0000)):
            uc.mem_write(base, open(os.path.join(ROM, name), 'rb').read())
        uc.mem_write(STOP, b'\xfe\xff\xff\xea')  # b . (never reached: emulation stops at STOP)
        uc.hook_add(UC_HOOK_MEM_WRITE, self._io_write, begin=0x04000280, end=0x040002bf)
        uc.hook_add(UC_HOOK_MEM_WRITE, self._dma_write, begin=0x040000b0, end=0x040000df)
        # a transfer already running when the replay starts (e.g. GX FIFO DMA) completes when its control is read
        uc.hook_add(UC_HOOK_MEM_READ, lambda uc, acc, a, size, v, u: self._w32(0x040000b8 + (a - 0x040000b0) // 12 * 12,
                    self._r32(0x040000b8 + (a - 0x040000b0) // 12 * 12) & 0x7fffffff), begin=0x040000b0, end=0x040000df)
        self._hook_unaligned()
        self.snapshot = uc.context_save()
        self.ram = bytes(uc.mem_read(0x02000000, 0x400000))

    def _hook_unaligned(self):
        """ARMv5 unaligned word access (Unicorn behaves like ARMv6+ whatever the CPU model): LDR reads the aligned
        word rotated right by 8 * (address & 3), STR writes the aligned word. Only the SDK string/printf code relies
        on it (its byte stores go through rotated words), so only functions using a register-amount rotate are
        hooked, and there each word LDR/STR is checked."""
        import capstone, glob, re
        from capstone import arm as csa
        md = self._cs = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_ARM)
        md.detail = True
        starts, rot = {}, set()
        self.thumb = set()  # addresses of Thumb functions (entered with bit 0 set)
        for line in open(os.path.join(ROOT, 'functions.csv')):
            f = line.split(',')
            if len(f) > 3 and f[3].startswith('0x'):
                starts[f[0]] = int(f[3], 16)
        for p in glob.glob(os.path.join(ROOT, 'extracted', 'asm_all', '**', '*.s'), recursive=True):
            cur = None
            for line in open(p, encoding='utf-8', errors='replace'):
                m = re.match(r'\s*(arm|thumb)_func_start (\S+)', line)
                if m:
                    cur = m.group(2) if m.group(1) == 'arm' else None
                    t = re.search(r'([0-9a-f]{8})$', m.group(2))
                    if m.group(1) == 'thumb' and t:
                        self.thumb.add(int(t.group(1), 16))
                elif cur and re.search(r', ror (r\d+|ip|lr)\s*$', line):
                    rot.add(cur)
        order = sorted(starts.values())
        for fn in rot:
            if fn in starts:
                a = starts[fn]
                end = next((x for x in order if x > a), a + 0x400)
                self.uc.hook_add(UC_HOOK_CODE, self._unaligned, None, a, end - 1)

    def _cond(self, cc):
        from capstone import arm as csa
        f = self.uc.reg_read(UC_ARM_REG_CPSR) >> 28
        n, z, c, v = f >> 3 & 1, f >> 2 & 1, f >> 1 & 1, f & 1
        return {csa.ARM_CC_AL: 1, csa.ARM_CC_INVALID: 1, csa.ARM_CC_EQ: z, csa.ARM_CC_NE: not z, csa.ARM_CC_HS: c,
                csa.ARM_CC_LO: not c, csa.ARM_CC_MI: n, csa.ARM_CC_PL: not n, csa.ARM_CC_VS: v, csa.ARM_CC_VC: not v,
                csa.ARM_CC_HI: c and not z, csa.ARM_CC_LS: not c or z, csa.ARM_CC_GE: n == v,
                csa.ARM_CC_LT: n != v, csa.ARM_CC_GT: not z and n == v, csa.ARM_CC_LE: z or n != v}[cc]

    def _unaligned(self, uc, addr, size, _):
        from capstone import arm as csa
        ins = next(self._cs.disasm(bytes(uc.mem_read(addr, 4)), addr), None)
        if ins is None or ins.id not in (csa.ARM_INS_LDR, csa.ARM_INS_STR) or not self._cond(ins.cc):
            return
        reg = lambda r: uc.reg_read(r) if r != csa.ARM_REG_PC else addr + 8
        ops = ins.operands
        mem = ops[1].mem
        base = reg(mem.base)
        if mem.index:
            off = reg(mem.index) << (ops[1].shift.value or 0)
        else:
            off = mem.disp
        if len(ops) == 3:  # post-indexed: [rn], #imm / [rn], rm
            off = ops[2].imm if ops[2].type == csa.ARM_OP_IMM else reg(ops[2].reg)
            if ops[2].subtracted:
                off = -off
            ea, wb = base, base + off
        else:
            if ops[1].subtracted:
                off = -off
            ea = base + off
            wb = ea if ins.writeback else None
        ea &= 0xffffffff
        if not ea & 3:
            return
        rt = ops[0].reg
        al = ea & ~3
        if ins.id == csa.ARM_INS_LDR:
            w = self._r32(al)
            k = (ea & 3) * 8
            uc.reg_write(rt, ((w >> k) | (w << (32 - k))) & 0xffffffff)
        else:
            self._w32(al, reg(rt))
        if wb is not None and not (ins.id == csa.ARM_INS_LDR and mem.base == rt):
            uc.reg_write(mem.base, wb & 0xffffffff)
        uc.reg_write(UC_ARM_REG_PC, addr + 4)

    def _r32(self, a):
        return int.from_bytes(self.uc.mem_read(a, 4), 'little')

    def _w32(self, a, v):
        self.uc.mem_write(a, (v & 0xffffffff).to_bytes(4, 'little'))

    def _io_write(self, uc, access, address, size, value, user):
        # the write lands after this hook: store it ourselves, then recompute
        uc.mem_write(address, (value & ((1 << (size * 8)) - 1)).to_bytes(size, 'little'))
        if 0x04000280 <= address < 0x040002a0:
            self._div()
        elif 0x040002b0 <= address < 0x040002c0:
            self._sqrt()

    def _dma_write(self, uc, access, address, size, value, user):
        """DMA channels 0-3: a write that sets the enable bit runs an immediate transfer at once; other start modes
        (VBlank, HBlank, display, card, GX FIFO) are treated as done (their targets are not modelled). Either way the
        enable bit clears, so the SDK's busy-wait loops end."""
        uc.mem_write(address, (value & ((1 << (size * 8)) - 1)).to_bytes(size, 'little'))
        ch = (address - 0x040000b0) // 12
        base = 0x040000b0 + ch * 12
        cnt = self._r32(base + 8)
        if not cnt >> 31:
            return
        src, dst = self._r32(base), self._r32(base + 4)
        n = cnt & 0x1fffff or 0x200000
        unit = 4 if cnt >> 26 & 1 else 2
        if cnt >> 27 & 7 == 0:
            step = {0: unit, 1: -unit, 2: 0, 3: unit}
            ds, ss = step[cnt >> 21 & 3], step[cnt >> 23 & 3]
            for i in range(min(n, 0x10000)):
                try:
                    uc.mem_write(dst + i * ds, bytes(uc.mem_read(src + i * ss, unit)))
                except Exception:
                    break  # an unmodelled target (VRAM banks, GX FIFO): nothing to compare there
        self._w32(base + 8, cnt & 0x7fffffff)

    def _div(self):
        mode = self._r32(0x04000280) & 3
        lo, hi, dlo, dhi = (self._r32(0x04000290), self._r32(0x04000294), self._r32(0x04000298),
                            self._r32(0x0400029c))
        if mode == 0:
            num, den = _s32(lo), _s32(dlo)
        elif mode == 1:
            num, den = _s64(lo | hi << 32), _s32(dlo)
        else:
            num, den = _s64(lo | hi << 32), _s64(dlo | dhi << 32)
        if den == 0:
            q, r = (1 if num < 0 else -1), num
        else:
            q = abs(num) // abs(den)
            if (num < 0) != (den < 0):
                q = -q
            r = num - q * den
        if mode == 0 and den != 0:
            q, r = _s32(q), _s32(r)
        q &= (1 << 64) - 1
        r &= (1 << 64) - 1
        self._w32(0x040002a0, q)
        self._w32(0x040002a4, q >> 32)
        self._w32(0x040002a8, r)
        self._w32(0x040002ac, r >> 32)
        self.uc.mem_write(0x04000280, (self._r32(0x04000280) & 0x7fff & 0xffff).to_bytes(2, 'little'))

    def _sqrt(self):
        v = self._r32(0x040002b8)
        if self._r32(0x040002b0) & 1:
            v |= self._r32(0x040002bc) << 32
        r = 0
        if v:
            import math
            r = math.isqrt(v)
        self._w32(0x040002b4, r)

    def call(self, addr, args=(), limit=2000000):
        """call the ARM function at addr with up to 4 u32 args; returns (r0, r1)"""
        uc = self.uc
        uc.context_restore(self.snapshot)
        uc.reg_write(UC_ARM_REG_CPSR, 0x1f)  # system mode, ARM state (first: SP is banked per mode)
        for i, a in enumerate(args[:4]):
            uc.reg_write(REGS[i], a & 0xffffffff)
        uc.reg_write(UC_ARM_REG_SP, 0x027fc000)
        uc.reg_write(UC_ARM_REG_LR, STOP)
        uc.emu_start(addr, STOP, count=limit)
        if uc.reg_read(UC_ARM_REG_PC) != STOP:
            raise RuntimeError('did not return (pc %08x)' % uc.reg_read(UC_ARM_REG_PC))
        return uc.reg_read(UC_ARM_REG_R0), uc.reg_read(UC_ARM_REG_R1)


if __name__ == '__main__':
    a = Arm()
    # FX_Div(3.0, 2.0) = 1.5 (0x1800); FX_Inv(2.0) = 0.5 (0x800)
    print('FX_Div(3, 2) = %08x' % a.call(0x01ff8958, (0x3000, 0x2000))[0])
    print('FX_Inv(2) = %08x' % a.call(0x01ff8930, (0x2000,))[0])
