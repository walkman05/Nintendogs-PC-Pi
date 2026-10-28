/* Host entry point: map DS memory, load the game from extracted/, then do what the ROM's startup code (Entry)
 * does before jumping to the game's main(). */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform_mem.h"
#include "loader.h"
#include "hw.h"
extern void hw_card_open(const char *data_dir);
extern void hw_card_open(const char *data_dir);

#ifdef _WIN32
#include <windows.h>
#endif

extern void func_0205e660(void);
extern void func_02000afc(void);
extern void func_02063c80(void);
extern void func_01ffdc94(void);
extern void ndogs_game_main(void);

static const char *stage = "start";
int ndogs_full = 1;  /* full-game mode (see host_compat.h); --demo keeps the kiosk demo's restrictions */

#ifdef _WIN32
static LONG WINAPI on_fault(EXCEPTION_POINTERS *e)
{
    EXCEPTION_RECORD *r = e->ExceptionRecord;
    fprintf(stderr, "\nfault 0x%08lx at eip=%p, address %p, during %s\n", r->ExceptionCode, r->ExceptionAddress,
            r->NumberParameters > 1 ? (void *)r->ExceptionInformation[1] : 0, stage);
    {
        unsigned int *fp = (unsigned int *)e->ContextRecord->Ebp, *sp = (unsigned int *)e->ContextRecord->Esp;
        int i;
        if (!IsBadReadPtr(sp, 16))
            fprintf(stderr, "  stack: %08x %08x %08x %08x\n", sp[0], sp[1], sp[2], sp[3]);
        if (fp && !IsBadReadPtr(fp, 24))
            fprintf(stderr, "  caller args: %08x %08x %08x %08x\n", fp[2], fp[3], fp[4], fp[5]);
        /* NDOGS_PEEK=addr[,addr...]: print words of DS memory at the crash (debugging aid) */
        if (getenv("NDOGS_PEEK")) {
            char *p = getenv("NDOGS_PEEK");
            while (*p) {
                unsigned int a = (unsigned int)strtoul(p, &p, 16);
                if (!IsBadReadPtr((void *)a, 16))
                    fprintf(stderr, "  peek %08x: %08x %08x %08x %08x\n", a, ((unsigned int *)a)[0],
                            ((unsigned int *)a)[1], ((unsigned int *)a)[2], ((unsigned int *)a)[3]);
                while (*p == ',' || *p == ' ')
                    p++;
            }
        }
        for (i = 0; i < 24 && fp && !IsBadReadPtr(fp, 8); i++) {
            fprintf(stderr, "  frame %2d return 0x%08x\n", i, fp[1]);
            fp = (unsigned int *)fp[0];
        }
    }
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
    fprintf(stderr, "\nwatchdog: main thread in %s, eip=0x%08lx (image base %p)\n", stage, (unsigned long)c.Eip,
            (void *)GetModuleHandleA(0));
    fp = (unsigned int *)c.Ebp;
    for (i = 0; i < 24 && fp && !IsBadReadPtr(fp, 8); i++) {
        fprintf(stderr, "  frame %2d return 0x%08x\n", i, fp[1]);
        fp = (unsigned int *)fp[0];
    }
    {
        extern void host_threads_dump(void);
        host_threads_dump();
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
    fprintf(stderr, "\nwatchdog: main thread in %s, eip=0x%08x\n", stage, (unsigned int)uc->uc_mcontext.gregs[REG_EIP]);
    for (i = 0; i < 24 && fp; i++) {
        fprintf(stderr, "  frame %2d return 0x%08x\n", i, fp[1]);
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

static int is_data_dir(const char *dir)
{
    char path[1024];
    FILE *f;
    snprintf(path, sizeof path, "%s/rom/header.yaml", dir);
    f = fopen(path, "rb");
    if (f)
        fclose(f);
    return f != 0;
}

/* Without --data (e.g. started by double-click, from the exe's folder): try extracted/ in the current folder, next
 * to the exe and in its parent folders, then the source tree the exe was built from. */
static const char *find_data_dir(void)
{
    static char buf[1024];
    char *slash;
    int up;
    if (is_data_dir("extracted"))
        return "extracted";
#ifdef _WIN32
    if (GetModuleFileNameA(0, buf, sizeof buf - 16) > 0) {
        for (up = 0; up < 4 && (slash = strrchr(buf, '\\')) != 0; up++) {
            strcpy(slash + 1, "extracted");
            if (is_data_dir(buf))
                return buf;
            *slash = 0;
        }
    }
#else
    {
        ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 16);
        if (n > 0) {
            buf[n] = 0;
            for (up = 0; up < 4 && (slash = strrchr(buf, '/')) != 0; up++) {
                strcpy(slash + 1, "extracted");
                if (is_data_dir(buf))
                    return buf;
                *slash = 0;
            }
        }
    }
#endif
#ifdef NDOGS_DEFAULT_DATA
    if (is_data_dir(NDOGS_DEFAULT_DATA))
        return NDOGS_DEFAULT_DATA;
#endif
    return "extracted";
}

static void fatal_box(const char *dir)
{
#ifdef _WIN32
    char msg[1400];
    snprintf(msg, sizeof msg, "Could not load the game data from \"%s\".\n\nRun extract_rom.py on the ROM first, or "
             "start the game as: nintendogs_pc --data <path to extracted>", dir);
    MessageBoxA(0, msg, "nintendogs_pc", MB_OK | MB_ICONERROR);
#else
    (void)dir;
#endif
}

int main(int argc, char **argv)
{
    const char *dir = 0;
    int i;
    for (i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--data") && i + 1 < argc)
            dir = argv[++i];
        else if (!strcmp(argv[i], "--demo"))
            ndogs_full = 0;
    if (!dir)
        dir = find_data_dir();
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
    if (platform_map_ds_memory() != 0) {
        int code = platform_relaunch_reserved();
        if (code >= 0)
            return code;
        fprintf(stderr, "could not map the DS memory\n");
        return 1;
    }
    hw_init();
    hw_trap_install();
    stage = "loading";
    if (ndogs_load_main(dir) != 0) {
        fatal_box(dir);
        return 1;
    }
    hw_card_open(dir);
    {
        extern void ndogs_save_set_dir(const char *data_dir);
        ndogs_save_set_dir(dir);
    }
    if (getenv("NDOGS_TRAPBENCH")) {  /* cost of one trapped register access */
        extern double now_ms(void);
        double t0 = now_ms();
        int k;
        for (k = 0; k < 100000; k++)
            (void)*(volatile unsigned short *)0x04000208u;
        printf("trap: %.2f us each\n", (now_ms() - t0) * 0.01);
        t0 = now_ms();
        for (k = 0; k < 100000; k++)
            (void)HostIoRead16(0x04000208u);
        printf("host accessor: %.2f us each\n", (now_ms() - t0) * 0.01);
    }
    {
        extern void video_init(void);
        video_init();
    }

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
