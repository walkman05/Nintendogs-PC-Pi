/* Register access trap. The pages holding DS I/O registers are kept inaccessible; a fault there tells us the
 * address and whether it is a read or a write. For a read the model first refreshes the register's backing
 * bytes. The page is then opened and the faulting x86 instruction re-executed natively under the single-step
 * flag (so every instruction form works, flags included); on the single-step trap a write is handed to the
 * model and the page is closed again. The game runs on one thread, so the short open window is safe. */
#include <stdio.h>
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

/* During an access every register page is open, so the model can touch registers on other pages. */
static void protect_all(int open)
{
    unsigned i;
    u32 p;
    for (i = 0; i < sizeof ranges / sizeof ranges[0]; i++)
        for (p = ranges[i].base; p < ranges[i].base + ranges[i].size; p += 0x1000)
            protect(p, open);
}

/* For host accessors: open/close the page holding a trapped register (no-op for other addresses). */
void hw_page_open(u32 addr, int open)
{
    if (trapped(addr))
        protect(page_of(addr), open);
}

#ifdef _WIN32
static void protect(u32 page, int open)
{
    DWORD old;
    VirtualProtect((void *)page, 0x1000, open ? PAGE_READWRITE : PAGE_NOACCESS, &old);
}

static LONG CALLBACK on_exception(EXCEPTION_POINTERS *e)
{
    EXCEPTION_RECORD *r = e->ExceptionRecord;
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
        protect_all(1);
        hw_io_prepare_read(addr);  /* also for writes: read-modify-write instructions read first */
        if (pending.write)
            hw_io_before_write(addr);
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
        protect_all(0);
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
    protect_all(1);
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
    protect_all(0);
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
