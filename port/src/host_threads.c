/* SDK threads on the host. The SDK switches threads with OS_SaveContext / OS_LoadContext (setjmp/longjmp-like
 * over a register context). Each SDK thread context gets a host fiber (Windows) or ucontext (POSIX) with its own
 * host stack: saving records which fiber owns the context, loading switches to it. A context that has never been
 * saved is a new thread (set up by OS_InitContext): its fiber starts at the entry point in the context (pc+4 in
 * word 16), with r0 (word 1) as the argument, and calls lr (word 15, OS_ExitThread) if the entry returns. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ctx.h"

#define MAX_CTX 64
#define HOST_STACK (512 * 1024)

#ifdef _WIN32
#include <windows.h>
typedef void *Fiber;
#else
#include <ucontext.h>
typedef ucontext_t *Fiber;
#endif

typedef struct {
    u32 *ctx;
    Fiber fiber;
    u32 where[10];  /* return addresses when the thread last switched away (debugging) */
} Entry;

static Entry table[MAX_CTX];
static int count;
static Fiber current;

static Entry *find(u32 *ctx)
{
    int i;
    for (i = 0; i < count; i++)
        if (table[i].ctx == ctx)
            return &table[i];
    return 0;
}

static Entry *add(u32 *ctx)
{
    Entry *e = find(ctx);
    if (e)
        return e;
    if (count == MAX_CTX) {
        fprintf(stderr, "host_threads: too many thread contexts\n");
        abort();
    }
    e = &table[count++];
    e->ctx = ctx;
    e->fiber = 0;
    return e;
}

/* OS_InitContext: the context is (re)initialised for a new thread; forget any fiber it had. */
void host_ctx_forget(u32 *ctx)
{
    Entry *e = find(ctx);
    if (e)
        e->fiber = 0;
}

static void ensure_main(void)
{
#ifdef _WIN32
    if (!current)
        current = ConvertThreadToFiber(0);
#else
    if (!current) {
        current = calloc(1, sizeof(ucontext_t));
        getcontext(current);
    }
#endif
}

int host_ctx_save(u32 *ctx)
{
    Entry *e;
    u32 *fp = (u32 *)__builtin_frame_address(0);
    int i;
    ensure_main();
    e = add(ctx);
    e->fiber = current;
    for (i = 0; i < 10; i++) {
        e->where[i] = fp ? fp[1] : 0;
        fp = fp && fp[0] > (u32)fp && fp[0] - (u32)fp < 0x100000 ? (u32 *)fp[0] : 0;
    }
    return 0;
}

/* For the watchdog: where every known thread context last switched away. */
void host_threads_dump(void)
{
    int i, k;
    for (i = 0; i < count; i++) {
        fprintf(stderr, "thread ctx %p%s:", (void *)table[i].ctx, table[i].fiber == current ? " (running)" : "");
        for (k = 0; k < 10 && table[i].where[k]; k++)
            fprintf(stderr, " 0x%08x", table[i].where[k]);
        fprintf(stderr, "\n");
    }
}

static void thread_body(u32 *ctx)
{
    void (*entry)(u32) = (void (*)(u32))(ctx[16] - 4);
    void (*on_exit)(void) = (void (*)(void))ctx[15];
    entry(ctx[1]);
    if (on_exit)
        on_exit();
    fprintf(stderr, "host_threads: thread %p returned with nowhere to go\n", (void *)ctx);
    for (;;)
        ;
}

#ifdef _WIN32
static void WINAPI fiber_main(void *p) { thread_body((u32 *)p); }
#else
static void uc_main(unsigned lo, unsigned hi)
{
    (void)hi;
    thread_body((u32 *)(size_t)lo);
}
#endif

void host_ctx_load(u32 *ctx)
{
    Entry *e;
    Fiber prev;
    ensure_main();
    e = add(ctx);
    if (!e->fiber) {
#ifdef _WIN32
        e->fiber = CreateFiber(HOST_STACK, fiber_main, ctx);
#else
        ucontext_t *uc = calloc(1, sizeof(ucontext_t));
        getcontext(uc);
        uc->uc_stack.ss_sp = malloc(HOST_STACK);
        uc->uc_stack.ss_size = HOST_STACK;
        uc->uc_link = 0;
        makecontext(uc, (void (*)(void))uc_main, 2, (unsigned)(size_t)ctx, 0u);
        e->fiber = uc;
#endif
        if (!e->fiber) {
            fprintf(stderr, "host_threads: cannot create a fiber\n");
            abort();
        }
    }
    if (e->fiber == current)
        return;
    prev = current;
    current = e->fiber;
#ifdef _WIN32
    (void)prev;
    SwitchToFiber(current);
#else
    swapcontext(prev, current);
#endif
}
