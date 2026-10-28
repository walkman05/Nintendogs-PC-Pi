#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform_mem.h"

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#include <sys/syscall.h>
#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif
#endif

const DsRegion ds_regions[] = {
    { 0x01ff0000ul, 0x00010000ul, "ITCM" },          /* 0x01ff8000, rounded to 64 KiB for VirtualAlloc */
    { 0x02000000ul, 0x00400000ul, "main RAM" },
    { 0x027c0000ul, 0x00010000ul, "DTCM" },
    { 0x027f0000ul, 0x00010000ul, "system area" },   /* 0x027ff000-0x027fffff: SDK shared work area */
    { 0x037f0000ul, 0x00010000ul, "shared WRAM" },
    { 0x04000000ul, 0x01000000ul, "I/O (as RAM)" },   /* milestone 3 placeholder: the hardware layer replaces it */
    { 0x05000000ul, 0x00010000ul, "palette" },
    { 0x06000000ul, 0x00900000ul, "VRAM" },
    { 0x07000000ul, 0x00010000ul, "OAM" },
};
const int ds_region_count = sizeof(ds_regions) / sizeof(ds_regions[0]);

static void *map_fixed(unsigned long base, unsigned long size)
{
#ifdef _WIN32
    return VirtualAlloc((void *)base, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void *p = mmap((void *)base, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    return p == MAP_FAILED ? NULL : p;
#endif
}

static void unmap(unsigned long base, unsigned long size)
{
#ifdef _WIN32
    (void)size;
    VirtualFree((void *)base, 0, MEM_RELEASE);
#else
    munmap((void *)base, size);
#endif
}

unsigned char *platform_io_alias[2];
static const unsigned long io_windows[2] = { 0x04000000ul, 0x04100000ul };

/* one I/O window: shared memory at its DS address plus an alias view */
static int map_io_window(int w)
{
    void *at = (void *)io_windows[w], *v, *alias;
#ifdef _WIN32
    HANDLE h = CreateFileMappingA(INVALID_HANDLE_VALUE, 0, PAGE_READWRITE, 0, IO_WINDOW, 0);
    if (!h)
        return 0;
    v = MapViewOfFileEx(h, FILE_MAP_ALL_ACCESS, 0, 0, IO_WINDOW, at);
    alias = MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, IO_WINDOW);
#else
    int fd = (int)syscall(SYS_memfd_create, "ndogs_io", 0);
    if (fd < 0 || ftruncate(fd, IO_WINDOW) != 0)
        return 0;
    v = mmap(at, IO_WINDOW, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED_NOREPLACE, fd, 0);
    alias = mmap(0, IO_WINDOW, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (alias == MAP_FAILED)
        alias = 0;
#endif
    platform_io_alias[w] = alias;
    return v == at && alias;
}

/* the I/O region: the two register windows, the rest plain memory */
static int map_io(unsigned long base, unsigned long size)
{
    return map_io_window(0) && map_io_window(1) &&
           map_fixed(base + IO_WINDOW, io_windows[1] - base - IO_WINDOW) == (void *)(base + IO_WINDOW) &&
           map_fixed(io_windows[1] + IO_WINDOW, base + size - io_windows[1] - IO_WINDOW) ==
               (void *)(io_windows[1] + IO_WINDOW);
}

#ifdef _WIN32
/* Loading user32 maps a ~20 MB system section at a random low address, which often covers the DS map (every
 * launch from Explorer or PowerShell; launches from MSYS bash happened to miss it). The address space is still free
 * while a new process is suspended, so on failure the exe starts itself again suspended with the DS ranges reserved
 * (NDOGS_RESERVED set), and the new process releases those reservations right before mapping them. Returns the new
 * process's exit code, or -1 when it could not be started. */
int platform_relaunch_reserved(void)
{
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    char exe[MAX_PATH];
    DWORD code = 1;
    int i;
    if (getenv("NDOGS_RESERVED") || !GetModuleFileNameA(0, exe, sizeof exe))
        return -1;
    SetEnvironmentVariableA("NDOGS_RESERVED", "1");
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    if (!CreateProcessA(exe, GetCommandLineA(), 0, 0, TRUE, CREATE_SUSPENDED, 0, 0, &si, &pi))
        return -1;
    for (i = 0; i < ds_region_count; i++)
        VirtualAllocEx(pi.hProcess, (void *)ds_regions[i].base, ds_regions[i].size, MEM_RESERVE, PAGE_NOACCESS);
    ResumeThread(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)code;
}

/* drop the reservations the parent made (only whole private reservations at exactly a region's base) */
static void release_parent_reservations(void)
{
    int i;
    if (!getenv("NDOGS_RESERVED"))
        return;
    for (i = 0; i < ds_region_count; i++) {
        MEMORY_BASIC_INFORMATION m;
        void *base = (void *)ds_regions[i].base;
        if (VirtualQuery(base, &m, sizeof m) && m.AllocationBase == base && m.State == MEM_RESERVE &&
            m.Type == MEM_PRIVATE)
            VirtualFree(base, 0, MEM_RELEASE);
    }
}
#else
int platform_relaunch_reserved(void)
{
    return -1;
}
#endif

int platform_map_ds_memory(void)
{
    int i, bad = 0;
#ifdef _WIN32
    release_parent_reservations();
#endif
    for (i = 0; i < ds_region_count; i++) {
        void *p;
        if (ds_regions[i].base == io_windows[0]) {
            if (!map_io(ds_regions[i].base, ds_regions[i].size)) {
                fprintf(stderr, "could not map the I/O region\n");
                bad = 1;
            }
            continue;
        }
        p = map_fixed(ds_regions[i].base, ds_regions[i].size);
        if (p != (void *)ds_regions[i].base) {
#ifdef _WIN32
            if (!getenv("NDOGS_RESERVED"))
                return 1;  /* quietly: main relaunches with the ranges reserved */
#endif
            fprintf(stderr, "could not map %s at 0x%08lx\n", ds_regions[i].name, ds_regions[i].base);
#ifdef _WIN32
            {
                MEMORY_BASIC_INFORMATION m;
                char name[MAX_PATH] = "";
                if (VirtualQuery((void *)ds_regions[i].base, &m, sizeof m)) {
                    MEMORY_BASIC_INFORMATION e;
                    char *q = (char *)m.AllocationBase;
                    while (VirtualQuery(q, &e, sizeof e) && e.AllocationBase == m.AllocationBase)
                        q += e.RegionSize;
                    K32GetMappedFileNameA(GetCurrentProcess(), m.AllocationBase, name, sizeof name);
                    fprintf(stderr, "  in use: %p-%p, state %lx, type %lx %s\n", m.AllocationBase, q,
                            m.State, m.Type, name);
                }
            }
#endif
            bad = 1;
        }
    }
    return bad;
}

void platform_unmap_ds_memory(void)
{
    int i;
    for (i = 0; i < ds_region_count; i++)
        unmap(ds_regions[i].base, ds_regions[i].size);
}
