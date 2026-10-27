/* Host entry point: map DS memory, load the game from extracted/, then do what the ROM's startup code (Entry)
 * does before jumping to the game's main(). */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform_mem.h"
#include "loader.h"

#ifdef _WIN32
#include <windows.h>
#endif

extern void func_0205e660(void);
extern void func_02000afc(void);
extern void func_02063c80(void);
extern void func_01ffdc94(void);
extern void ndogs_game_main(void);

static const char *stage = "start";

#ifdef _WIN32
static LONG WINAPI on_fault(EXCEPTION_POINTERS *e)
{
    EXCEPTION_RECORD *r = e->ExceptionRecord;
    fprintf(stderr, "\nfault 0x%08lx at eip=%p, address %p, during %s\n", r->ExceptionCode, r->ExceptionAddress,
            r->NumberParameters > 1 ? (void *)r->ExceptionInformation[1] : 0, stage);
    fflush(stderr);
    return EXCEPTION_EXECUTE_HANDLER;
}
#else
static void on_fault(int sig, siginfo_t *si, void *uc)
{
    (void)uc;
    fprintf(stderr, "\nsignal %d, address %p, during %s\n", sig, si->si_addr, stage);
    fflush(stderr);
    _exit(128 + sig);
}
#endif

/* NDOGS_WATCHDOG=<seconds>: after that long, print the main thread's instruction pointer and frame-pointer
 * backtrace (the build uses -fno-omit-frame-pointer); match addresses against the linker map. */
#ifdef _WIN32
static HANDLE main_thread;
static DWORD WINAPI watchdog(LPVOID arg)
{
    CONTEXT c;
    int i;
    unsigned int *fp;
    Sleep((DWORD)(int)(size_t)arg * 1000);
    SuspendThread(main_thread);
    memset(&c, 0, sizeof c);
    c.ContextFlags = CONTEXT_FULL;
    GetThreadContext(main_thread, &c);
    fprintf(stderr, "
watchdog: main thread in %s, eip=0x%08lx
", stage, (unsigned long)c.Eip);
    fp = (unsigned int *)c.Ebp;
    for (i = 0; i < 24 && fp && !IsBadReadPtr(fp, 8); i++) {
        fprintf(stderr, "  frame %2d return 0x%08x
", i, fp[1]);
        fp = (unsigned int *)fp[0];
    }
    fflush(stderr);
    ExitProcess(3);
    return 0;
}
static void start_watchdog(int secs)
{
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &main_thread, 0, FALSE,
                    DUPLICATE_SAME_ACCESS);
    CreateThread(0, 0, watchdog, (LPVOID)(size_t)secs, 0, 0);
}
#else
#include <ucontext.h>
#include <unistd.h>
static void on_alarm(int sig, siginfo_t *si, void *ucv)
{
    ucontext_t *uc = ucv;
    unsigned int *fp = (unsigned int *)uc->uc_mcontext.gregs[REG_EBP];
    int i;
    (void)sig;
    (void)si;
    fprintf(stderr, "
watchdog: main thread in %s, eip=0x%08x
", stage, (unsigned int)uc->uc_mcontext.gregs[REG_EIP]);
    for (i = 0; i < 24 && fp; i++) {
        fprintf(stderr, "  frame %2d return 0x%08x
", i, fp[1]);
        fp = (unsigned int *)fp[0];
    }
    _exit(3);
}
static void start_watchdog(int secs)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = on_alarm;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGALRM, &sa, 0);
    alarm((unsigned)secs);
}
#endif

static void install_fault_handler(void)
{
#ifdef _WIN32
    SetUnhandledExceptionFilter(on_fault);
#else
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = on_fault;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, 0);
    sigaction(SIGBUS, &sa, 0);
    sigaction(SIGILL, &sa, 0);
    sigaction(SIGFPE, &sa, 0);
#endif
}

int main(int argc, char **argv)
{
    const char *dir = "extracted";
    int i;
    for (i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--data") && i + 1 < argc)
            dir = argv[++i];
    setvbuf(stdout, 0, _IONBF, 0);
    printf("nintendogs_pc: %d-bit build\n", (int)(sizeof(void *) * 8));
    if (sizeof(void *) != 4) {
        fprintf(stderr, "a 32-bit build is required\n");
        return 1;
    }
    install_fault_handler();
    if (getenv("NDOGS_WATCHDOG"))
        start_watchdog(atoi(getenv("NDOGS_WATCHDOG")));
    stage = "memory map";
    if (platform_map_ds_memory() != 0)
        return 1;
    stage = "loading";
    if (ndogs_load_main(dir) != 0)
        return 1;

    /* Entry: SDK work-area word, IRQ handler pointer at the top of DTCM, then the startup calls and main() */
    *(volatile unsigned int *)0x027fff9cu = 0;
    *(volatile unsigned int *)(0x027c0000u + 0x3ffcu) = (unsigned int)func_01ffdc94;
    stage = "startup (func_0205e660)";
    func_0205e660();
    stage = "startup (func_02000afc)";
    func_02000afc();
    stage = "static constructors (func_02063c80)";
    func_02063c80();
    stage = "main";
    printf("entering main()\n");
    ndogs_game_main();
    printf("main() returned\n");
    return 0;
}
