/* Register access trap. The pages holding DS I/O registers are kept inaccessible; a fault there tells us the
 * address and whether it is a read or a write. For a read the model first refreshes the register's backing
 * bytes. The page is then opened and the faulting x86 instruction re-executed natively under the single-step
 * flag (so every instruction form works, flags included); on the single-step trap a write is handed to the
 * model and the page is closed again. The game runs on one thread, so the short open window is safe. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hw.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#include <sys/mman.h>
#include <ucontext.h>
#endif

typedef struct {
    u32 base, size;
} TrapRange;

/* main register block, sub engine registers, IPC receive FIFO / card data */
static const TrapRange ranges[] = {
    { 0x04000000u, 0x2000u },
    { 0x04100000u, 0x1000u },
};

static struct {
    int active;
    u32 addr, page;
    int write;
} pending;

int hw_iolog;
u32 watch_addr;
/* NDOGS_TRAPPROF=<n>: count traps per faulting instruction, print the busiest after n traps */
static u32 trap_prof, prof_eip[4096], prof_n[4096];
u32 trap_prof_set(u32 n) { return trap_prof = n; }
static void prof_hit(u32 eip)
{
    static u32 total;
    extern u32 hw_frame;
    static int from = -1;
    u32 h = (eip * 2654435761u) >> 20, i;
    while (prof_eip[h] && prof_eip[h] != eip)
        h = (h + 1) & 4095;
    if (from < 0)
        from = getenv("NDOGS_PROFFROM") ? atoi(getenv("NDOGS_PROFFROM")) : 0;
    if (hw_frame < (u32)from)
        return;
    prof_eip[h] = eip;
    prof_n[h]++;
    if (++total == trap_prof)
        for (i = 0; i < 4096; i++)
            if (prof_n[i] > trap_prof / 200)
                fprintf(stderr, "trapprof %08x %u\n", prof_eip[i], prof_n[i]);
}
extern void hw_watch_hit(u32 addr, u32 eip, u32 ebp);
unsigned hw_trap_count, hw_trap_gx;

extern void hw_io_before_write(u32 addr);
extern void hw_io_after_if_write(void);
extern void hw_irq_poll(void);

static u32 page_of(u32 addr) { return addr & ~0xfffu; }

static int trapped(u32 addr)
{
    unsigned i;
    for (i = 0; i < sizeof ranges / sizeof ranges[0]; i++)
        if (addr - ranges[i].base < ranges[i].size)
            return 1;
    return 0;
}

static void protect(u32 page, int open);

int hw_trap_busy(void) { return pending.active; }

static int in_exception;
int hw_in_exception(void) { return in_exception; }


/* The register model and host accessors reach registers through the always-accessible alias (platform_mem.h), so
 * pages never need opening for them; only the trapped instruction's own page is opened while it runs. */
void hw_page_open(u32 addr, int open)
{
    (void)addr;
    (void)open;
}

#ifdef _WIN32
static void protect(u32 page, int open)
{
    DWORD old;
    VirtualProtect((void *)page, 0x1000, open ? PAGE_READWRITE : PAGE_NOACCESS, &old);
}

/* NDOGS_HWWATCH=<addr>: CPU data watchpoint (debug register 0) on a 4-byte word; each write is logged with the
 * writer's call chain (debugging aid for memory the game corrupts). */
static u32 hwwatch;
static int hwwatch_hit(EXCEPTION_POINTERS *e)
{
    CONTEXT *c = e->ContextRecord;
    u32 *fp = (u32 *)c->Ebp;
    int k;
    if (!(c->Dr6 & 1))
        return 0;
    c->Dr6 = 0;
    fprintf(stderr, "hwwatch %08x = %08x at eip %08x <", hwwatch, *(u32 *)hwwatch, (u32)c->Eip);
    for (k = 0; k < 10 && fp && !IsBadReadPtr(fp, 8); k++, fp = (u32 *)fp[0])
        fprintf(stderr, " %08x", fp[1]);
    fputc(10, stderr);
    return 1;
}

static DWORD WINAPI hwwatch_arm(LPVOID thread)
{
    CONTEXT c;
    SuspendThread((HANDLE)thread);
    memset(&c, 0, sizeof c);
    c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    GetThreadContext((HANDLE)thread, &c);
    c.Dr0 = hwwatch;
    c.Dr7 = 1 | (1u << 16) | (3u << 18);  /* local enable, break on write, 4 bytes */
    SetThreadContext((HANDLE)thread, &c);
    ResumeThread((HANDLE)thread);
    return 0;
}

/* Builds with NDOGS_CFLAGS="-fsanitize=shift-exponent,integer-divide-by-zero -fsanitize-trap=all" put a ud2 where
 * C would shift by >= the type width or divide by zero (x86 and ARM disagree there). Each such site is logged
 * once with its caller, and execution continues after the ud2. */
static int ub_hit(EXCEPTION_POINTERS *e)
{
    static u32 seen[512];
    static int nseen;
    u32 eip = (u32)e->ContextRecord->Eip, *fp = (u32 *)e->ContextRecord->Ebp;
    int i;
    if (IsBadReadPtr((void *)eip, 2) || ((u8 *)eip)[0] != 0x0f || ((u8 *)eip)[1] != 0x0b)
        return 0;
    for (i = 0; i < nseen && seen[i] != eip; i++)
        ;
    if (i == nseen && nseen < 512) {
        seen[nseen++] = eip;
        fprintf(stderr, "ub at %08x <%08x\n", eip, fp && !IsBadReadPtr(fp, 8) ? fp[1] : 0);
    }
    e->ContextRecord->Eip += 2;
    return 1;
}

static LONG CALLBACK on_exception(EXCEPTION_POINTERS *e)
{
    EXCEPTION_RECORD *r = e->ExceptionRecord;
    if (r->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION && ub_hit(e))
        return EXCEPTION_CONTINUE_EXECUTION;
    if (r->ExceptionCode == EXCEPTION_SINGLE_STEP && hwwatch && hwwatch_hit(e) && !pending.active)
        return EXCEPTION_CONTINUE_EXECUTION;
    if (r->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && r->NumberParameters >= 2) {
        u32 addr = (u32)r->ExceptionInformation[1];
        if (!trapped(addr) || pending.active)
            return EXCEPTION_CONTINUE_SEARCH;
        pending.active = 1;
        hw_trap_count++;
        if (addr - 0x04000400u < 0x200u)
            hw_trap_gx++;
        pending.addr = addr;
        pending.page = page_of(addr);
        pending.write = r->ExceptionInformation[0] == 1;
        protect(pending.page, 1);
        hw_io_prepare_read(addr);  /* also for writes: read-modify-write instructions read first */
        if (pending.write)
            hw_io_before_write(addr);
        if (trap_prof)
            prof_hit((u32)e->ContextRecord->Eip);
        if (pending.write && watch_addr && (addr & ~3u) == watch_addr)  /* NDOGS_WATCH=<reg>: who writes it, when */
            hw_watch_hit(addr, (u32)e->ContextRecord->Eip, (u32)e->ContextRecord->Ebp);
        e->ContextRecord->EFlags |= 0x100;  /* single-step the access */
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (r->ExceptionCode == EXCEPTION_SINGLE_STEP && pending.active) {
        pending.active = 0;
        if (pending.write) {
            if ((pending.addr & ~3u) == 0x04000214)
                hw_io_after_if_write();
            hw_io_written(pending.addr);
        }
        if (hw_iolog)
            fprintf(stderr, "io %s 0x%08x = 0x%08x\n", pending.write ? "W" : "R", pending.addr,
                    *(volatile u32 *)(pending.addr & ~3u));
        protect(pending.page, 0);
        if (pending.write) {
            in_exception = 1;
            hw_irq_poll();  /* a write may have made an interrupt pending (IME/IE/IF, FIFO) */
            in_exception = 0;
        }
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

int hw_trap_install(void)
{
    unsigned i;
    u32 p;
    AddVectoredExceptionHandler(1, on_exception);
    if (getenv("NDOGS_HWWATCH")) {
        HANDLE self, t;
        hwwatch = (u32)strtoul(getenv("NDOGS_HWWATCH"), 0, 16) & ~3u;
        DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &self, 0, FALSE,
                        DUPLICATE_SAME_ACCESS);
        t = CreateThread(0, 0, hwwatch_arm, self, 0, 0);
        WaitForSingleObject(t, INFINITE);
    }
    for (i = 0; i < sizeof ranges / sizeof ranges[0]; i++)
        for (p = ranges[i].base; p < ranges[i].base + ranges[i].size; p += 0x1000)
            protect(p, 0);
    return 0;
}
#elif defined(__arm__)
/* 32-bit ARM Linux (Raspberry Pi): no single-step flag for user programs. Instead the instruction after the
 * faulting one is replaced by an undefined instruction while the page is open; the access re-runs natively (any
 * load/store form, ARM or Thumb) and the undefined instruction brings us back to finish and restore it. */
#include <unistd.h>
static void protect(u32 page, int open)
{
    mprotect((void *)page, 0x1000, open ? PROT_READ | PROT_WRITE : PROT_NONE);
}

static struct {
    u32 at;        /* where the undefined instruction is */
    u32 saved;     /* what it replaced */
    int thumb;
} stop;

static void code_writable(u32 addr, int n)
{
    long pg = sysconf(_SC_PAGESIZE);
    u32 lo = addr & ~(u32)(pg - 1), hi = (addr + n + pg - 1) & ~(u32)(pg - 1);
    mprotect((void *)lo, hi - lo, PROT_READ | PROT_WRITE | PROT_EXEC);
}

static void on_segv(int sig, siginfo_t *si, void *ucv)
{
    ucontext_t *uc = ucv;
    u32 addr = (u32)(size_t)si->si_addr, pc = uc->uc_mcontext.arm_pc, next;
    int thumb = (uc->uc_mcontext.arm_cpsr & 0x20) != 0;
    (void)sig;
    if (!trapped(addr) || pending.active) {
        signal(SIGSEGV, SIG_DFL);
        return;
    }
    pending.active = 1;
    hw_trap_count++;
    if (addr - 0x04000400u < 0x200u)
        hw_trap_gx++;
    pending.addr = addr;
    pending.page = page_of(addr);
    /* ARM: bit 11 of the fault status (WnR) says write */
    pending.write = (uc->uc_mcontext.error_code & (1u << 11)) != 0;
    protect(pending.page, 1);
    hw_io_prepare_read(addr);
    if (pending.write)
        hw_io_before_write(addr);
    if (trap_prof)
        prof_hit(pc);
    if (thumb) {
        u16 h = *(u16 *)pc;
        next = pc + ((h >> 11) >= 0x1d ? 4 : 2);  /* 32-bit Thumb-2 instructions start with 0b11101/11110/11111 */
        code_writable(next, 2);
        stop.saved = *(u16 *)next;
        *(u16 *)next = 0xde00;                     /* UDF #0 (Thumb) */
        __builtin___clear_cache((char *)next, (char *)next + 2);
    } else {
        next = pc + 4;
        code_writable(next, 4);
        stop.saved = *(u32 *)next;
        *(u32 *)next = 0xe7f000f0;                 /* UDF #0 (ARM) */
        __builtin___clear_cache((char *)next, (char *)next + 4);
    }
    stop.at = next;
    stop.thumb = thumb;
}

static void on_ill(int sig, siginfo_t *si, void *ucv)
{
    ucontext_t *uc = ucv;
    (void)sig;
    (void)si;
    if (!pending.active || uc->uc_mcontext.arm_pc != stop.at) {
        signal(SIGILL, SIG_DFL);
        return;
    }
    if (stop.thumb)
        *(u16 *)stop.at = (u16)stop.saved;
    else
        *(u32 *)stop.at = stop.saved;
    __builtin___clear_cache((char *)stop.at, (char *)stop.at + 4);
    pending.active = 0;
    if (pending.write) {
        if ((pending.addr & ~3u) == 0x04000214)
            hw_io_after_if_write();
        hw_io_written(pending.addr);
    }
    if (hw_iolog)
        fprintf(stderr, "io %s 0x%08x = 0x%08x\n", pending.write ? "W" : "R", pending.addr,
                *(volatile u32 *)(pending.addr & ~3u));
    protect(pending.page, 0);
    if (pending.write) {
        in_exception = 1;
        hw_irq_poll();
        in_exception = 0;
    }
}

int hw_trap_install(void)
{
    struct sigaction sa;
    unsigned i;
    u32 p;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = on_segv;
    sa.sa_flags = SA_SIGINFO | SA_NODEFER;
    sigaction(SIGSEGV, &sa, 0);
    sa.sa_sigaction = on_ill;
    sigaction(SIGILL, &sa, 0);
    for (i = 0; i < sizeof ranges / sizeof ranges[0]; i++)
        for (p = ranges[i].base; p < ranges[i].base + ranges[i].size; p += 0x1000)
            protect(p, 0);
    return 0;
}
#else
static void protect(u32 page, int open)
{
    mprotect((void *)page, 0x1000, open ? PROT_READ | PROT_WRITE : PROT_NONE);
}

static void on_segv(int sig, siginfo_t *si, void *ucv)
{
    ucontext_t *uc = ucv;
    u32 addr = (u32)(size_t)si->si_addr;
    (void)sig;
    if (!trapped(addr) || pending.active) {
        signal(SIGSEGV, SIG_DFL);
        return;
    }
    pending.active = 1;
    pending.addr = addr;
    pending.page = page_of(addr);
    pending.write = (uc->uc_mcontext.gregs[REG_ERR] & 2) != 0;
    protect(pending.page, 1);
    hw_io_prepare_read(addr);
    if (pending.write)
        hw_io_before_write(addr);
    uc->uc_mcontext.gregs[REG_EFL] |= 0x100;
}

static void on_trap(int sig, siginfo_t *si, void *ucv)
{
    ucontext_t *uc = ucv;
    (void)sig;
    (void)si;
    if (!pending.active)
        return;
    pending.active = 0;
    uc->uc_mcontext.gregs[REG_EFL] &= ~0x100;
    if (pending.write) {
        if ((pending.addr & ~3u) == 0x04000214)
            hw_io_after_if_write();
        hw_io_written(pending.addr);
    }
    if (hw_iolog)
        fprintf(stderr, "io %s 0x%08x = 0x%08x\n", pending.write ? "W" : "R", pending.addr,
                *(volatile u32 *)(pending.addr & ~3u));
    protect(pending.page, 0);
    if (pending.write) {
        in_exception = 1;
        hw_irq_poll();
        in_exception = 0;
    }
}

int hw_trap_install(void)
{
    struct sigaction sa;
    unsigned i;
    u32 p;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = on_segv;
    sa.sa_flags = SA_SIGINFO | SA_NODEFER;
    sigaction(SIGSEGV, &sa, 0);
    sa.sa_sigaction = on_trap;
    sigaction(SIGTRAP, &sa, 0);
    for (i = 0; i < sizeof ranges / sizeof ranges[0]; i++)
        for (p = ranges[i].base; p < ranges[i].base + ranges[i].size; p += 0x1000)
            protect(p, 0);
    return 0;
}
#endif
