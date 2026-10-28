/* DS register model (ARM9 side). Registers without special behaviour keep the last value written, in the
 * trapped page's own memory. The ARM7 is not emulated: what the ARM9 expects from it is answered here. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hw.h"
#include "platform_mem.h"

#define R8(a) (*(volatile u8 *)io_reg(a))
#define R16(a) (*(volatile u16 *)io_reg(a))
#define R32(a) (*(volatile u32 *)io_reg(a))

extern void hw_page_open(u32 addr, int open);
static int video_pending, vblank_pending;
extern void video_frame(void);
extern void video_vblank(void);
extern u32 input_keys(void);
static u32 current_line(void);

u32 hw_irq_count[32], hw_pxi_msgs;
extern unsigned hw_trap_count, hw_trap_gx;
extern u32 hw_card_reads;
int hw_status;

#define FIFO_DEPTH 16
typedef struct {
    u32 data[FIFO_DEPTH];
    int head, count;
} Fifo;

static struct {
    u32 vcount;
    u32 arm7_sync;          /* IPCSYNC bits 0-3 as the ARM7 shows them */
    Fifo to_arm7, to_arm9;  /* IPC FIFOs */
    u32 fifo_error;
    u32 timer[4];
} hw;

static int fifo_push(Fifo *f, u32 v)
{
    if (f->count == FIFO_DEPTH)
        return 0;
    f->data[(f->head + f->count++) % FIFO_DEPTH] = v;
    return 1;
}

static int fifo_pop(Fifo *f, u32 *v)
{
    if (!f->count)
        return 0;
    *v = f->data[f->head];
    f->head = (f->head + 1) % FIFO_DEPTH;
    f->count--;
    return 1;
}

void hw_raise_irq(u32 bits)
{
    R32(0x04000214) |= bits;
}

/* ---- ARM7 stand-in -------------------------------------------------------------------------------------- */

static void arm7_reply(u32 tag, u32 data)
{
    fifo_push(&hw.to_arm9, (data << 6) | (tag & 0x1f));
    if (R32(0x04000184) & 0x400)  /* receive-not-empty interrupt enabled */
        hw_raise_irq(1u << 18);
}

/* SPI devices behind the ARM7 (PXI channels 4 NVRAM/firmware, 5 RTC, 6 touch screen, 8 power management,
 * 9 microphone): a command is a start word (bit 25, command in bits 8-14) followed by data words, the last with
 * bit 24. The ARM7 answers with bit 24, the command number in bits 8-14 and a result byte (0 = success).
 * Data the commands produce lives in shared memory (e.g. touch samples at 0x027fffaa, all "not touched" here). */
static u32 spi_cmd[32];
static void arm7_spi(u32 tag, u32 data)
{
    if (data & 0x2000000u)
        spi_cmd[tag] = (data >> 8) & 0x7f;
    if (data & 0x1000000u)
        arm7_reply(tag, 0x1000000u | (spi_cmd[tag] << 8) | 0);
}

/* PXI channel 5, real-time clock: each word is a request (command in bits 8-14); the ARM7 puts date and time
 * in BCD at 0x027ffde8 (year, month, day, weekday) and 0x027ffdec (hour with bit 6 = PM, minute, second) and
 * answers with the command number and result 0. The host's local time is used. */
#include <time.h>
static u8 bcd(int v) { return (u8)(((v / 10) << 4) | (v % 10)); }
static void arm7_rtc(u32 data)
{
    time_t now = time(0);
    struct tm *t = localtime(&now);
    volatile u8 *d = (volatile u8 *)0x027ffde8u;
    d[0] = bcd(t->tm_year % 100);
    d[1] = bcd(t->tm_mon + 1);
    d[2] = bcd(t->tm_mday);
    d[3] = (u8)t->tm_wday;
    d[4] = (u8)(bcd(t->tm_hour) | (t->tm_hour >= 12 ? 0x40 : 0));
    d[5] = bcd(t->tm_min);
    d[6] = bcd(t->tm_sec);
    arm7_reply(5, data & 0x7f00);
}

/* PXI channel 7, sound: each message is the address of a linked list of commands ({next, id, args[4]}). The
 * ARM7 runs them and then advances finishCommandTag, the first word of the shared work area the ARM9 handed over
 * with command 29 (SHARED_WORK). No audio yet: commands are only acknowledged. */
static u32 snd_shared;
static void arm7_sound(u32 addr)
{
    u32 cmd, guard = 0;
    if (!addr)
        return;
    for (cmd = addr; cmd && guard < 4096; cmd = *(volatile u32 *)cmd, guard++)
        if (*(volatile u32 *)(cmd + 4) == 29)
            snd_shared = *(volatile u32 *)(cmd + 8);
    if (snd_shared)
        (*(volatile u32 *)snd_shared)++;
}

static void arm7_receive(u32 msg)
{
    hw_pxi_msgs++;
    u32 tag = msg & 0x1f, data = msg >> 6;
    if (hw_iolog)
        fprintf(stderr, "arm7: PXI message tag %u data 0x%07x%s\n", tag, data, (msg & 0x20) ? " (err)" : "");
    switch (tag) {
    case 4: case 6: case 8: case 9:
        arm7_spi(tag, data);
        break;
    case 5:
        arm7_rtc(data);
        break;
    case 7:
        arm7_sound(data);
        break;
    default:
        break;
    }
}

/* ---- divider / square root ------------------------------------------------------------------------------ */

static void do_div(void)
{
    u32 cnt = R16(0x04000280) & 3;
    s64 num, den, q, r;
    if (cnt == 0) {
        num = (s32)R32(0x04000290);
        den = (s32)R32(0x04000298);
    } else if (cnt == 1) {
        num = (s64)(R32(0x04000290) | (u64)R32(0x04000294) << 32);
        den = (s32)R32(0x04000298);
    } else {
        num = (s64)(R32(0x04000290) | (u64)R32(0x04000294) << 32);
        den = (s64)(R32(0x04000298) | (u64)R32(0x0400029c) << 32);
    }
    if (den == 0) {
        q = num < 0 ? 1 : -1;
        r = num;
        R16(0x04000280) |= 0x4000;  /* division by zero */
    } else if (num == (s64)0x8000000000000000ll && den == -1) {
        q = num;
        r = 0;
    } else {
        q = num / den;
        r = num % den;
        R16(0x04000280) &= ~0x4000;
    }
    if (cnt == 0 && den != 0) {
        /* 32/32: results are sign-extended 32-bit values */
        q = (s32)q;
        r = (s32)r;
    }
    R32(0x040002a0) = (u32)q;
    R32(0x040002a4) = (u32)((u64)q >> 32);
    R32(0x040002a8) = (u32)r;
    R32(0x040002ac) = (u32)((u64)r >> 32);
    R16(0x04000280) &= ~0x8000;  /* not busy */
}

static void do_sqrt(void)
{
    u64 v = (R16(0x040002b0) & 1) ? (R32(0x040002b8) | (u64)R32(0x040002bc) << 32) : R32(0x040002b8);
    u64 r = 0, bit = (u64)1 << 62;
    while (bit > v)
        bit >>= 2;
    while (bit) {
        if (v >= r + bit) {
            v -= r + bit;
            r = (r >> 1) + bit;
        } else
            r >>= 1;
        bit >>= 2;
    }
    R32(0x040002b4) = (u32)r;
    R16(0x040002b0) &= ~0x8000;
}

/* ---- DMA (immediate transfers only for now) ---------------------------------------------------------------- */

#include "platform_mem.h"

/* Addresses a DMA may touch on the host: the mapped DS regions (I/O excluded: register DMA goes elsewhere). */
static int dma_ok(u32 a)
{
    int i;
    for (i = 0; i < ds_region_count; i++)
        if (a - (u32)ds_regions[i].base < (u32)ds_regions[i].size && ds_regions[i].base != 0x04000000ul)
            return 1;
    return 0;
}

/* 3D geometry commands (FIFO 0x04000400 and the command ports 0x04000440-0x040005ff): counted for now, rendered
 * by the 3D engine in milestone 5. */
u32 hw_gx_words;
#include "ds_video.h"
void hw_gx_command(u32 port, u32 value)
{
    hw_gx_words++;
    gx_command(port, value);
}

extern void hw_card_start(void);
extern int hw_card_pending(void);
extern u32 hw_card_data(void);

/* Run DMA channel ch if it is enabled for <trigger> (0 immediate, 5 game card). */
static void do_dma(int ch, int trigger)
{
    u32 base = 0x040000b0 + ch * 12;
    u32 cnt = R32(base + 8);
    u32 src = R32(base), dst = R32(base + 4), n = cnt & 0x1fffff, i;
    int word = (cnt >> 26) & 1, timing = (cnt >> 27) & 7;
    int dctl = (cnt >> 21) & 3, sctl = (cnt >> 23) & 3;
    u32 step = word ? 4 : 2;
    if (!(cnt & 0x80000000u) || timing != trigger)
        return;
    if (n == 0)
        n = 0x200000;
    if (trigger == 7) {
        /* geometry command FIFO: the whole display list goes to the 3D command sink at once */
        for (i = 0; i < n; i++) {
            hw_gx_command(dst, dma_ok(src) ? *(volatile u32 *)src : 0);
            if (sctl == 0)
                src += 4;
            else if (sctl == 1)
                src -= 4;
        }
        R32(base + 8) = cnt & ~0x80000000u;
        if (cnt & 0x40000000u)
            hw_raise_irq(1u << (8 + ch));
        return;
    }
    if (trigger == 5) {
        /* game card: one word per "data ready", until the card transfer is over */
        while (hw_card_pending()) {
            u32 v = hw_card_data();
            if (dma_ok(dst))
                *(volatile u32 *)dst = v;
            if (dctl == 0 || dctl == 3)
                dst += 4;
        }
        if (!(cnt & 0x02000000u))
            R32(base + 8) = cnt & ~0x80000000u;
        if (cnt & 0x40000000u)
            hw_raise_irq(1u << (8 + ch));
        return;
    }
    for (i = 0; i < n; i++) {
        if (!dma_ok(src) || !dma_ok(dst))
            ;  /* e.g. the SDK's dummy 0 -> 0 transfer used to stop a channel */
        else if (word)
            *(volatile u32 *)dst = *(volatile u32 *)src;
        else
            *(volatile u16 *)dst = *(volatile u16 *)src;
        if (dctl == 0 || dctl == 3)
            dst += step;
        else if (dctl == 1)
            dst -= step;
        if (sctl == 0)
            src += step;
        else if (sctl == 1)
            src -= step;
    }
    R32(base + 8) = cnt & ~0x80000000u;
    if (cnt & 0x40000000u)
        hw_raise_irq(1u << (8 + ch));
}

/* ---- model entry points ------------------------------------------------------------------------------------ */

void hw_io_prepare_read(u32 addr)
{
    u32 a = addr & ~1u;
    if ((addr & ~3u) - 0x04000604u < 0xa0u) {  /* 3D results: RAM_COUNT, POS/VEC_RESULT, CLIPMTX/VECMTX_RESULT */
        extern u32 gx_read_result(u32 addr);
        R32(addr & ~3u) = gx_read_result(addr & ~3u);
        return;
    }
    switch (a & ~3u) {
    case 0x04000004: /* DISPSTAT / VCOUNT: from the frame clock */
        hw.vcount = current_line();
        R16(0x04000006) = (u16)hw.vcount;
        R16(0x04000004) = (u16)((R16(0x04000004) & ~7u) | (hw.vcount >= 192 && hw.vcount < 262 ? 1 : 0) | 2 |
                                (hw.vcount == (u32)(R16(0x04000004) >> 8) ? 4 : 0));
        break;
    case 0x04000100: case 0x04000104: case 0x04000108: case 0x0400010c: {
        int t = (int)((a - 0x04000100) >> 2);
        hw.timer[t] += 64;
        R16(0x04000100 + t * 4) = (u16)hw.timer[t];
        break;
    }
    case 0x04000130: /* KEYINPUT: from the window's keyboard */
        R16(0x04000130) = (u16)(input_keys() & 0x03ff);
        break;
    case 0x04000180: /* IPCSYNC */
        R32(0x04000180) = (R32(0x04000180) & ~0xfu) | hw.arm7_sync;
        break;
    case 0x04000184: /* IPCFIFOCNT */
        R32(0x04000184) = (R32(0x04000184) & 0x8404u) | (hw.to_arm7.count == 0 ? 1 : 0) |
                          (hw.to_arm7.count == FIFO_DEPTH ? 2 : 0) | (hw.to_arm9.count == 0 ? 0x100 : 0) |
                          (hw.to_arm9.count == FIFO_DEPTH ? 0x200 : 0) | hw.fifo_error;
        break;
    case 0x04100010: /* game card data */
        R32(0x04100010) = hw_card_data();
        break;
    case 0x04100000: { /* IPCFIFORECV */
        u32 v = 0;
        if (!fifo_pop(&hw.to_arm9, &v))
            hw.fifo_error = 0x4000;
        R32(0x04100000) = v;
        if (hw.to_arm9.count && (R32(0x04000184) & 0x400))
            hw_raise_irq(1u << 18);
        break;
    }
    case 0x04000600: /* GXSTAT: from the 3D engine (stack levels; FIFO empty, never busy) */
        R32(0x04000600) = (R32(0x04000600) & 0xc0000000u) | gx_status();
        break;
    default:
        break;
    }
}

void hw_io_written(u32 addr)
{
    u32 a = addr & ~3u;
    if (a - 0x04000400u < 0x200u) { /* geometry FIFO and command ports */
        hw_gx_command(a, R32(a));
        return;
    }
    switch (a) {
    case 0x04000180: { /* IPCSYNC: the ARM7 answers each value the ARM9 shows with the next one */
        u32 out = (R32(0x04000180) >> 8) & 0xf;
        hw.arm7_sync = (out + 1) & 0xf;
        break;
    }
    case 0x04000184: /* IPCFIFOCNT: bit 3 clears the send FIFO, bit 14 acknowledges an error */
        if (R32(0x04000184) & 8)
            hw.to_arm7.count = 0;
        if (R32(0x04000184) & 0x4000)
            hw.fifo_error = 0;
        R32(0x04000184) &= 0x8404u;
        if ((R32(0x04000184) & 0x400) && hw.to_arm9.count)
            hw_raise_irq(1u << 18);
        break;
    case 0x04000188: /* IPCFIFOSEND */
        if (!fifo_push(&hw.to_arm7, R32(0x04000188)))
            hw.fifo_error = 0x4000;
        else {
            u32 m;
            while (fifo_pop(&hw.to_arm7, &m))
                arm7_receive(m);
        }
        break;
    case 0x04000214: /* IF: writing 1 clears */
        break;
    case 0x04000280: case 0x04000290: case 0x04000294: case 0x04000298: case 0x0400029c:
        do_div();
        break;
    case 0x040002b0: case 0x040002b8: case 0x040002bc:
        do_sqrt();
        break;
    case 0x040000b8: case 0x040000c4: case 0x040000d0: case 0x040000dc:
        do_dma((int)((a - 0x040000b8) / 12), 0);
        do_dma((int)((a - 0x040000b8) / 12), 7);
        break;
    case 0x04000240: case 0x04000244: case 0x04000248: { /* VRAMCNT A-D, E-G (+WRAMCNT), H-I */
        static const u16 reg[9] = { 0x240, 0x241, 0x242, 0x243, 0x244, 0x245, 0x246, 0x248, 0x249 };
        int b;
        for (b = 0; b < 9; b++)
            vram_cnt_written(b, R8(0x04000000u + reg[b]));
        break;
    }
    case 0x040001a4: /* ROMCTRL: bit 31 starts a card command */
        if (R32(0x040001a4) & 0x80000000u) {
            int ch;
            hw_card_start();
            for (ch = 0; ch < 4; ch++)
                do_dma(ch, 5);
        }
        break;
    default:
        break;
    }
}

/* IF needs write-1-to-clear semantics, which the generic "page keeps the value" cannot give: remember the old
 * value before the write lands. Called by the trap before every write. */
static u32 if_before;
void hw_io_before_write(u32 addr)
{
    if ((addr & ~3u) == 0x04000214)
        if_before = R32(0x04000214);
}
void hw_io_after_if_write(void)
{
    u32 written = R32(0x04000214);
    R32(0x04000214) = if_before & ~written;
}

void hw_init(void)
{
    memset(&hw, 0, sizeof hw);
    {
        extern u32 watch_addr;
        extern u32 trap_prof_set(u32);
        trap_prof_set(getenv("NDOGS_TRAPPROF") ? (u32)atoi(getenv("NDOGS_TRAPPROF")) : 0);
        watch_addr = getenv("NDOGS_WATCH") ? (u32)strtoul(getenv("NDOGS_WATCH"), 0, 16) & ~3u : 0;
    }
    hw_iolog = getenv("NDOGS_IOLOG") ? atoi(getenv("NDOGS_IOLOG")) : 0;
    hw_status = getenv("NDOGS_STATUS") ? atoi(getenv("NDOGS_STATUS")) : 0;
    /* The ARM7 publishes the PXI channels it serves in shared memory (0x027ffc00 + 0x388 + 4 * ARM7); the ARM9
     * waits for them (PXI_IsCallbackReady). The stand-in serves every channel; messages are logged with
     * NDOGS_IOLOG until each gets a real answer. */
    *(volatile u32 *)(0x027ffc00u + 0x388u + 4u) = 0xffffffffu;
}

/* ---- interrupt delivery ----------------------------------------------------------------------------------------
 * Called where the hardware would take a pending interrupt: when the game unmasks IRQs in the CPSR, when it halts,
 * and after a register write. Runs the game's IRQ handler (pointer at the top of DTCM) with IRQs masked. */
extern u32 host_cpsr;
extern int hw_trap_busy(void);
extern int hw_in_exception(void);
static int in_irq;

/* What the ROM's IRQ exit (func_01ffdcec) does after a handler: threads waiting for the interrupts that fired
 * (bits collected at DTCM+0) become ready, and if a reschedule was requested the highest-priority ready thread
 * (first ready one in the priority-sorted list) is switched to. OSThreadInfo is data_02155cf8 (+0 reschedule
 * flag, +8 current, +0xc list, +0x10 switch callback); an OSThread starts with its context, state at +0x64, next
 * at +0x68, id at +0x6c. The switch is a host fiber switch, so it is only done outside exception handlers. */
extern void func_02066290(u32 *cp);
extern int host_ctx_save(u32 *ctx);
extern void host_ctx_load(u32 *ctx);

static void irq_exit_reschedule(void)
{
    volatile u16 *irqq = (volatile u16 *)0x027c0000u;
    u8 *info = (u8 *)0x02155cf8u;
    u8 *t, *cur;
    if (*irqq) {
        u32 bits = *irqq;
        *irqq = 0;
        *(volatile u16 *)info = 1;
        for (t = *(u8 **)(info + 0xc); t; t = *(u8 **)(t + 0x68))
            if (bits & (1u << *(u32 *)(t + 0x6c)))
                *(u32 *)(t + 0x64) = 1;
    }
    if (*(volatile u16 *)info == 0 || hw_in_exception())
        return;
    *(volatile u16 *)info = 0;
    for (t = *(u8 **)(info + 0xc); t && *(u16 *)(t + 0x64) != 1; t = *(u8 **)(t + 0x68))
        ;
    cur = *(u8 **)(info + 8);
    if (!t || t == cur)
        return;
    if (*(u32 *)(info + 0x10))
        ((void (*)(u8 *, u8 *))*(u32 *)(info + 0x10))(cur, t);
    *(u8 **)(info + 8) = t;
    /* save the interrupted thread (resumes here) and switch */
    func_02066290((u32 *)(cur + 0x48));
    *(u32 *)cur = host_cpsr;
    host_ctx_save((u32 *)cur);
    {
        extern void func_02066224(u32 *cp);
        func_02066224((u32 *)(t + 0x48));
    }
    host_cpsr = *(u32 *)t;
    host_ctx_load((u32 *)t);
}

void hw_irq_poll(void)
{
    int guard = 0;
    if (in_irq || hw_trap_busy())
        return;
    hw_time_check();
    if (hw_iolog > 1)
        fprintf(stderr, "irq poll: cpsr %02x IME %x IE %08x IF %08x\n", host_cpsr & 0xff, HostIoRead32(0x04000208),
                HostIoRead32(0x04000210), HostIoRead32(0x04000214));
    while (!(host_cpsr & 0x80) && (HostIoRead32(0x04000208) & 1) &&
           (HostIoRead32(0x04000210) & HostIoRead32(0x04000214)) && guard++ < 32) {
        u32 saved = host_cpsr, p = HostIoRead32(0x04000210) & HostIoRead32(0x04000214), b;
        for (b = 0; b < 32; b++)
            if (p & (1u << b)) {
                hw_irq_count[b]++;
                break;
            }
        in_irq = 1;
        host_cpsr = (host_cpsr & ~0x1fu) | 0x92;  /* IRQ mode, IRQs masked */
        ((void (*)(void)) * (volatile u32 *)0x027c3ffcu)();
        host_cpsr = saved;
        in_irq = 0;
        irq_exit_reschedule();
    }
    if (!in_irq && !hw_in_exception()) {
        if (vblank_pending) {
            vblank_pending = 0;
            video_vblank();
        }
        if (video_pending) {
            video_pending = 0;
            video_frame();
        }
        irq_exit_reschedule();
    }  /* a switch left pending by an interrupt taken inside a register access */
}

/* ---- time: frames -----------------------------------------------------------------------------------------------
 * The DS runs 60 frames a second (263 scanlines; VBlank from line 192). Without a cycle-accurate clock, time moves
 * when the game halts to wait for an interrupt: the host advances to the next VBlank, raises the VBlank interrupt
 * if DISPSTAT enables it, keeps real-time pace, and delivers it. */
#ifdef _WIN32
#include <windows.h>
double now_ms(void)
{
    static LARGE_INTEGER f;
    LARGE_INTEGER c;
    if (!f.QuadPart)
        QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / (double)f.QuadPart;
}
/* Precise wait: Sleep() is 15.6 ms granular unless the timer resolution is raised (winmm, loaded at run time like
 * user32); the last 2 ms are spun so a frame starts on time. */
static void sleep_ms(double ms)
{
    static int res_set;
    double end = now_ms() + ms;
    if (!res_set) {
        HMODULE w = LoadLibraryA("winmm.dll");
        FARPROC f = w ? GetProcAddress(w, "timeBeginPeriod") : 0;
        if (f)
            ((UINT(WINAPI *)(UINT))f)(1);
        res_set = 1;
    }
    if (ms > 2)
        Sleep((DWORD)(ms - 2));
    while (now_ms() < end)
        Sleep(0);
}
#else
#include <time.h>
double now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}
static void sleep_ms(double ms)
{
    struct timespec t;
    if (ms <= 0)
        return;
    t.tv_sec = (time_t)(ms / 1000);
    t.tv_nsec = (long)((ms - t.tv_sec * 1000.0) * 1e6);
    nanosleep(&t, 0);
}
#endif

u32 hw_frame;
static double next_vblank;  /* wall-clock time (ms) of the next VBlank */
#define FRAME_MS (1000.0 / 60.0)
/* The picture is drawn from line 0, after the VBlank lines (192-262): the game's VBlank handler sets up the
 * coming frame (screen swap, VRAM banks, capture) first, so the frame is rendered at that point, not at VBlank. */
#define LINE0_MS (FRAME_MS * 71 / 263)
static double display_due;

static void display_start(void)
{
    display_due = 0;
    if (hw_in_exception())
        video_pending = 1;  /* render at the next safe point */
    else
        video_frame();
}

static void vblank_event(void)
{
    hw_frame++;
    if (hw_in_exception())
        vblank_pending = 1;
    else
        video_vblank();
    display_due = now_ms() + LINE0_MS;
    if ((hw_iolog || hw_status) && hw_frame % 60 == 0)
        fprintf(stderr, "frame %u: IME %x IE %08x IF %08x DISPSTAT %04x | irqs vblank %u ipc %u card %u timer0 %u | "
                        "card reads %u, pxi msgs %u | traps %u (3D ports %u), 3D words %u\n", hw_frame,
                HostIoRead32(0x04000208), HostIoRead32(0x04000210), HostIoRead32(0x04000214),
                HostIoRead16(0x04000004), hw_irq_count[0], hw_irq_count[18], hw_irq_count[19], hw_irq_count[3],
                hw_card_reads, hw_pxi_msgs, hw_trap_count, hw_trap_gx, hw_gx_words);
    hw_page_open(0x04000004, 1);
    R16(0x04000004) |= 1;
    if (R16(0x04000004) & 8)
        hw_raise_irq(1);
    hw_page_open(0x04000004, 0);
}

/* Frame clock. A VBlank happens when the game halts waiting for an interrupt (its frame is done), never in the
 * middle of a frame (the game swaps screens and 3D views in step with VBlank and assumes each frame fits in one).
 * VBlanks follow a fixed 60 Hz schedule (the game's logic advances once per VBlank, as on the DS, so this is real
 * speed; each screen shows 30 fps since the game alternates them): a VBlank never comes early, and after a slow
 * frame the following ones come as soon as the game halts until it is back on schedule. If the game doesn't halt
 * for a while, a VBlank is raised anyway. */
#define STALL_MS 100.0
#define CATCHUP_MS 250.0

static void next_frame(double t)
{
    /* VBlanks keep to a fixed 60 Hz schedule, so game speed follows the clock: after a slow frame the next ones
     * start without waiting until the game has caught up. Only a long stall (loading) moves the schedule. */
    next_vblank += FRAME_MS;
    if (t - next_vblank > CATCHUP_MS)
        next_vblank = t + FRAME_MS;
    vblank_event();
}

/* checked at interrupt points */
void hw_time_check(void)
{
    double t = now_ms();
    if (next_vblank == 0)
        next_vblank = t + FRAME_MS;
    if (t >= next_vblank + STALL_MS)
        next_frame(t);
    if (display_due && t >= display_due)
        display_start();
}

/* Scanline (0-262) from the frame clock; VBlank lines are 192-262. */
static u32 current_line(void)
{
    double into;
    if (next_vblank == 0)
        return 0;
    into = now_ms() - (next_vblank - FRAME_MS);  /* time since the last VBlank started (line 192) */
    return (192 + (u32)(into / FRAME_MS * 263)) % 263;
}

/* Halt (wait for interrupt): finish drawing the current frame, wait for the next VBlank time, then take it. */
void hw_halt(void)
{
    double t = now_ms();
    if (next_vblank == 0)
        next_vblank = t + FRAME_MS;
    if (display_due)
        display_start();
    t = now_ms();
    if (hw_status > 1) {  /* NDOGS_STATUS=2: per-frame busy time (VBlank to halt, including drawing) and traps */
        static u32 last_traps;
        fprintf(stderr, "busy %.1fms traps %u\n", t - (next_vblank - FRAME_MS), hw_trap_count - last_traps);
        last_traps = hw_trap_count;
    }
    if (next_vblank > t)
        sleep_ms(next_vblank - t);
    next_frame(now_ms());
    hw_irq_poll();
}

/* ---- host accessors ------------------------------------------------------------------------------------------ */

extern void hw_page_open(u32 addr, int open);

u32 HostIoRead32(u32 addr)
{
    u32 v;
    hw_page_open(addr, 1);
    hw_io_prepare_read(addr);
    v = R32(addr);
    hw_page_open(addr, 0);
    return v;
}
u16 HostIoRead16(u32 addr) { return (u16)(HostIoRead32(addr & ~3u) >> ((addr & 2) * 8)); }
u8 HostIoRead8(u32 addr) { return (u8)(HostIoRead32(addr & ~3u) >> ((addr & 3) * 8)); }

static void host_write(u32 addr, u32 value, int size)
{
    hw_page_open(addr, 1);
    hw_io_before_write(addr);
    if (size == 4)
        R32(addr) = value;
    else if (size == 2)
        R16(addr) = (u16)value;
    else
        R8(addr) = (u8)value;
    if ((addr & ~3u) == 0x04000214)
        hw_io_after_if_write();
    hw_io_written(addr);
    hw_page_open(addr, 0);
}
void HostIoWrite32(u32 addr, u32 value) { host_write(addr, value, 4); }
void HostIoWrite16(u32 addr, u16 value) { host_write(addr, value, 2); }
void HostIoWrite8(u32 addr, u8 value) { host_write(addr, value, 1); }

/* Register values as last written (for the renderers). */
void hw_io_snapshot(u8 *io)
{
    hw_page_open(0x04000000, 1);
    hw_page_open(0x04001000, 1);
    memcpy(io, (const void *)io_reg(0x04000000), 0x2000);
    hw_page_open(0x04000000, 0);
    hw_page_open(0x04001000, 0);
}

void hw_capture_done(void)
{
    hw_page_open(0x04000000, 1);
    R32(0x04000064u) &= ~0x80000000u;
    hw_page_open(0x04000000, 0);
}

/* NDOGS_WATCH debugging: frame, scanline, whether inside the IRQ handler, and the writer's call chain */
void hw_watch_hit(u32 addr, u32 eip, u32 ebp)
{
    u32 *fp = (u32 *)ebp;
    int i;
    fprintf(stderr, "watch %08x: frame %u line %u irq %d eip %08x", addr, hw_frame, current_line(), in_irq, eip);
    for (i = 0; i < 4 && fp; i++, fp = (u32 *)fp[0])
        fprintf(stderr, " <%08x", fp[1]);
    fprintf(stderr, "\n");
}
