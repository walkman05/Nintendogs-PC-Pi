/* A small pool of host threads for data-parallel work (the 3D rasteriser's scanline bands). workers_run() runs
 * fn(job, arg) for job = 0..n-1 spread over the pool and the calling thread, and returns when all are done. Workers
 * only ever touch host memory handed to them (never DS registers or game state that traps). */
#include <stdlib.h>
#include "ds_video.h"

#define MAX_WORKERS 15

static int nworkers = -1;
static void (*job_fn)(int, void *);
static void *job_arg;
static volatile long next_job, jobs_total, jobs_left;

#ifdef _WIN32
#include <windows.h>
static HANDLE start_sem, done_ev;

static void run_jobs(void)
{
    long j;
    while ((j = InterlockedIncrement(&next_job) - 1) < jobs_total) {
        job_fn((int)j, job_arg);
        if (InterlockedDecrement(&jobs_left) == 0)
            SetEvent(done_ev);
    }
}

static DWORD WINAPI worker(LPVOID unused)
{
    (void)unused;
    for (;;) {
        WaitForSingleObject(start_sem, INFINITE);
        run_jobs();
    }
    return 0;
}

static int cpu_count(void)
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (int)si.dwNumberOfProcessors;
}

static void start_threads(int n)
{
    int i;
    start_sem = CreateSemaphoreA(0, 0, MAX_WORKERS, 0);
    done_ev = CreateEventA(0, FALSE, FALSE, 0);
    for (i = 0; i < n; i++)
        CreateThread(0, 0, worker, 0, 0, 0);
}

void workers_run(int n, void (*fn)(int, void *), void *arg)
{
    if (nworkers < 0)
        workers_init();
    if (n <= 0)
        return;
    job_fn = fn;
    job_arg = arg;
    jobs_total = n;
    jobs_left = n;
    next_job = 0;
    MemoryBarrier();
    if (nworkers > 0) {
        ResetEvent(done_ev);
        ReleaseSemaphore(start_sem, nworkers < n - 1 ? nworkers : n - 1, 0);
    }
    run_jobs();
    if (nworkers > 0)
        while (jobs_left > 0)
            WaitForSingleObject(done_ev, 1);
}

#else
#include <pthread.h>
#include <unistd.h>
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t start_cv = PTHREAD_COND_INITIALIZER, done_cv = PTHREAD_COND_INITIALIZER;
static int generation;

static void run_jobs(void)
{
    long j;
    while ((j = __sync_fetch_and_add(&next_job, 1)) < jobs_total) {
        job_fn((int)j, job_arg);
        if (__sync_sub_and_fetch(&jobs_left, 1) == 0) {
            pthread_mutex_lock(&mu);
            pthread_cond_signal(&done_cv);
            pthread_mutex_unlock(&mu);
        }
    }
}

static void *worker(void *unused)
{
    int seen = 0;
    (void)unused;
    for (;;) {
        pthread_mutex_lock(&mu);
        while (generation == seen)
            pthread_cond_wait(&start_cv, &mu);
        seen = generation;
        pthread_mutex_unlock(&mu);
        run_jobs();
    }
    return 0;
}

static int cpu_count(void)
{
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (int)n : 1;
}

static void start_threads(int n)
{
    int i;
    for (i = 0; i < n; i++) {
        pthread_t t;
        pthread_create(&t, 0, worker, 0);
        pthread_detach(t);
    }
}

void workers_run(int n, void (*fn)(int, void *), void *arg)
{
    if (nworkers < 0)
        workers_init();
    if (n <= 0)
        return;
    job_fn = fn;
    job_arg = arg;
    jobs_total = n;
    jobs_left = n;
    next_job = 0;
    __sync_synchronize();
    if (nworkers > 0) {
        pthread_mutex_lock(&mu);
        generation++;
        pthread_cond_broadcast(&start_cv);
        pthread_mutex_unlock(&mu);
    }
    run_jobs();
    if (nworkers > 0) {
        pthread_mutex_lock(&mu);
        while (jobs_left > 0)
            pthread_cond_wait(&done_cv, &mu);
        pthread_mutex_unlock(&mu);
    }
}
#endif

/* NDOGS_THREADS=<n> overrides the worker count (default: CPUs - 1, at most 15) */
void workers_init(void)
{
    int n;
    if (nworkers >= 0)
        return;
    n = getenv("NDOGS_THREADS") ? atoi(getenv("NDOGS_THREADS")) : cpu_count() - 1;
    if (n < 0)
        n = 0;
    if (n > MAX_WORKERS)
        n = MAX_WORKERS;
    nworkers = n;
    if (n > 0)
        start_threads(n);
}

int workers_count(void)
{
    if (nworkers < 0)
        workers_init();
    return nworkers + 1;
}
