"""Run original ARM9 code from the extracted ROM under Unicorn, as the reference for differential tests of the
port's host routines (port/tools/difftest.py).

ARM9 main RAM (arm9.bin at 0x02000000), ITCM (0x01ff8000) and DTCM (0x027e0000) are loaded from extracted/rom; the
hardware divider and square root (0x04000280-0x040002bc) are modelled, since the runtime uses them. A call puts up
to four arguments in r0-r3, returns to a stop address in lr, and gives back r0 and r1."""
import os
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_MEM_WRITE, UC_PROT_ALL
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
        self.snapshot = uc.context_save()
        self.ram = bytes(uc.mem_read(0x02000000, 0x400000))

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
