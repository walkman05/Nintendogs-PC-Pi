/* Microphone (PXI channel 9, the ARM7's MIC library). Auto sampling (command 0x41: type, buffer, size, rate in
 * 33.514 MHz cycles) fills a ring buffer in main RAM in real time and keeps the address of the newest sample at
 * 0x027fff90; one-shot sampling (0x30) leaves one sample at 0x027fff94. Type bits: 0 = 12-bit (16-bit words, left
 * aligned), 1 = signed, 2 = filter off, 4 = loop. The host feeds samples once per VBlank, rate/60 of them (game
 * time), from the PC's default recording device (Windows: waveIn, loaded at run time; elsewhere silence for now),
 * or from a WAV file for tests: NDOGS_MICWAV=<16-bit mono PCM .wav>,<frame> plays it into the microphone from that
 * frame on. NDOGS_NOMIC=1 opens no device. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hw.h"

extern u32 hw_frame;

#define FIFO 16384  /* host samples waiting (about 2 s at 8 kHz) */
static s16 fifo[FIFO];
static unsigned fifo_r, fifo_w;

static struct {
    int on, type;
    u32 buf, size, pos;
    double rate, acc;
} auto_s;

static s16 *wav_data;
static unsigned wav_len, wav_pos;
static int wav_frame = -1;

static void fifo_put(const s16 *s, unsigned n)
{
    while (n--) {
        fifo[fifo_w++ % FIFO] = *s++;
        if (fifo_w - fifo_r > FIFO)
            fifo_r = fifo_w - FIFO;
    }
}

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#define NBUF 8
#define BUFN 256
static struct {
    MMRESULT (WINAPI *Open)(LPHWAVEIN, UINT, LPCWAVEFORMATEX, DWORD_PTR, DWORD_PTR, DWORD);
    MMRESULT (WINAPI *Prepare)(HWAVEIN, LPWAVEHDR, UINT);
    MMRESULT (WINAPI *Add)(HWAVEIN, LPWAVEHDR, UINT);
    MMRESULT (WINAPI *Start)(HWAVEIN);
    MMRESULT (WINAPI *Stop)(HWAVEIN);
    MMRESULT (WINAPI *Reset)(HWAVEIN);
    MMRESULT (WINAPI *Unprepare)(HWAVEIN, LPWAVEHDR, UINT);
    MMRESULT (WINAPI *Close)(HWAVEIN);
} W;
static HWAVEIN dev;
static WAVEHDR hdr[NBUF];
static s16 bufs[NBUF][BUFN];

static void dev_close(void)
{
    int i;
    if (!dev)
        return;
    W.Reset(dev);
    for (i = 0; i < NBUF; i++)
        W.Unprepare(dev, &hdr[i], sizeof hdr[i]);
    W.Close(dev);
    dev = 0;
}

static void dev_open(int rate)
{
    static HMODULE w;
    WAVEFORMATEX f;
    int i;
    dev_close();
    if (!w && !(w = LoadLibraryA("winmm.dll")))
        return;
    *(FARPROC *)&W.Open = GetProcAddress(w, "waveInOpen");
    *(FARPROC *)&W.Prepare = GetProcAddress(w, "waveInPrepareHeader");
    *(FARPROC *)&W.Add = GetProcAddress(w, "waveInAddBuffer");
    *(FARPROC *)&W.Start = GetProcAddress(w, "waveInStart");
    *(FARPROC *)&W.Stop = GetProcAddress(w, "waveInStop");
    *(FARPROC *)&W.Reset = GetProcAddress(w, "waveInReset");
    *(FARPROC *)&W.Unprepare = GetProcAddress(w, "waveInUnprepareHeader");
    *(FARPROC *)&W.Close = GetProcAddress(w, "waveInClose");
    memset(&f, 0, sizeof f);
    f.wFormatTag = WAVE_FORMAT_PCM;
    f.nChannels = 1;
    f.nSamplesPerSec = (DWORD)rate;
    f.wBitsPerSample = 16;
    f.nBlockAlign = 2;
    f.nAvgBytesPerSec = (DWORD)rate * 2;
    if (W.Open(&dev, WAVE_MAPPER, &f, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        fprintf(stderr, "mic: no recording device at %d Hz\n", rate);
        dev = 0;
        return;
    }
    for (i = 0; i < NBUF; i++) {
        memset(&hdr[i], 0, sizeof hdr[i]);
        hdr[i].lpData = (LPSTR)bufs[i];
        hdr[i].dwBufferLength = sizeof bufs[i];
        W.Prepare(dev, &hdr[i], sizeof hdr[i]);
        W.Add(dev, &hdr[i], sizeof hdr[i]);
    }
    W.Start(dev);
}

static void dev_poll(void)
{
    int i;
    if (!dev)
        return;
    for (i = 0; i < NBUF; i++)
        if (hdr[i].dwFlags & WHDR_DONE) {
            fifo_put(bufs[i], hdr[i].dwBytesRecorded / 2);
            hdr[i].dwFlags &= ~WHDR_DONE;
            W.Add(dev, &hdr[i], sizeof hdr[i]);
        }
}
#else
/* ALSA capture through dlopen (no build dependency; silence if libasound or a capture device is missing) */
#include <dlfcn.h>
static void *cap;
static struct {
    int (*open)(void **, const char *, int, int);
    int (*set_params)(void *, int, int, unsigned, unsigned, int, unsigned);
    long (*readi)(void *, void *, unsigned long);
    int (*prepare)(void *);
    int (*close)(void *);
} A;

static void dev_close(void)
{
    if (cap)
        A.close(cap);
    cap = 0;
}

static void dev_open(int rate)
{
    static void *lib;
    dev_close();
    if (!lib && !(lib = dlopen("libasound.so.2", RTLD_NOW)))
        return;
    A.open = (int (*)(void **, const char *, int, int))dlsym(lib, "snd_pcm_open");
    A.set_params = (int (*)(void *, int, int, unsigned, unsigned, int, unsigned))dlsym(lib, "snd_pcm_set_params");
    A.readi = (long (*)(void *, void *, unsigned long))dlsym(lib, "snd_pcm_readi");
    A.prepare = (int (*)(void *))dlsym(lib, "snd_pcm_prepare");
    A.close = (int (*)(void *))dlsym(lib, "snd_pcm_close");
    if (!A.open || !A.set_params || !A.readi || !A.prepare || !A.close)
        return;
    /* capture (1), non-blocking (1); S16_LE (2), interleaved (3), mono, 100 ms latency */
    if (A.open(&cap, "default", 1, 1) < 0) {
        fprintf(stderr, "mic: no recording device\n");
        cap = 0;
        return;
    }
    if (A.set_params(cap, 2, 3, 1, (unsigned)rate, 1, 100000) < 0) {
        fprintf(stderr, "mic: the recording device can't do %d Hz mono\n", rate);
        dev_close();
    }
}

static void dev_poll(void)
{
    s16 buf[512];
    long n;
    if (!cap)
        return;
    while ((n = A.readi(cap, buf, 512)) > 0)
        fifo_put(buf, (unsigned)n);
    if (n < 0 && n != -11)  /* -EAGAIN: nothing yet; anything else (overrun): recover */
        A.prepare(cap);
}
#endif

static void wav_load(void)
{
    static int done;
    const char *e = getenv("NDOGS_MICWAV");
    char path[400];
    FILE *f;
    u8 h[44];
    const char *comma;
    if (done || !e)
        return;
    done = 1;
    comma = strrchr(e, ',');
    snprintf(path, sizeof path, "%.*s", comma ? (int)(comma - e) : (int)strlen(e), e);
    wav_frame = comma ? atoi(comma + 1) : 0;
    f = fopen(path, "rb");
    if (!f || fread(h, 1, 44, f) != 44) {
        fprintf(stderr, "mic: cannot read %s\n", path);
        if (f)
            fclose(f);
        return;
    }
    fseek(f, 0, SEEK_END);
    wav_len = (unsigned)(ftell(f) - 44) / 2;
    wav_data = malloc(wav_len * 2 + 2);
    fseek(f, 44, SEEK_SET);
    wav_len = (unsigned)fread(wav_data, 2, wav_len, f);
    fclose(f);
}

/* one sample in the requested format */
static u16 convert(int type, s16 s)
{
    if (type & 1)
        return (u16)((type & 2 ? s : s + 0x8000) & 0xfff0);
    return (u8)((type & 2 ? s : s + 0x8000) >> 8);
}

static void put(u16 v)
{
    if (auto_s.type & 1) {
        *(volatile u16 *)(auto_s.buf + auto_s.pos) = v;
        *(volatile u32 *)0x027fff90u = auto_s.buf + auto_s.pos;
        auto_s.pos += 2;
    } else {
        *(volatile u8 *)(auto_s.buf + auto_s.pos) = (u8)v;
        *(volatile u32 *)0x027fff90u = auto_s.buf + auto_s.pos;
        auto_s.pos += 1;
    }
    if (auto_s.pos >= auto_s.size) {
        auto_s.pos = 0;
        if (!(auto_s.type & 0x10))
            auto_s.on = 0;
    }
}

static s16 next_sample(void)
{
    if (wav_data && wav_frame >= 0 && (int)hw_frame >= wav_frame && wav_pos < wav_len)
        return wav_data[wav_pos++];
    if (fifo_r != fifo_w)
        return fifo[fifo_r++ % FIFO];
    return 0;
}

/* each VBlank: the samples the ARM7 would have taken in 1/60 s */
void mic_frame(void)
{
    int n;
    dev_poll();
    if (!auto_s.on)
        return;
    if (fifo_w - fifo_r > (unsigned)(auto_s.rate / 4))  /* keep the delay under 0.25 s */
        fifo_r = fifo_w - (unsigned)(auto_s.rate / 8);
    auto_s.acc += auto_s.rate / 60.0;
    n = (int)auto_s.acc;
    auto_s.acc -= n;
    while (n-- > 0 && auto_s.on)
        put(convert(auto_s.type, next_sample()));
}

/* a complete command from the ARM9: w[0] the start word's low byte, w[1..] the data words' 16-bit payloads */
void mic_command(u32 cmd, const u16 *w)
{
    wav_load();
    switch (cmd) {
    case 0x30: /* one sample */
        dev_poll();
        *(volatile u16 *)0x027fff94u = convert(w[0], next_sample());
        break;
    case 0x41: /* auto sampling start */
        auto_s.type = w[0];
        auto_s.buf = (u32)w[1] << 16 | w[2];
        auto_s.size = (u32)w[3] << 16 | w[4];
        auto_s.rate = 33513982.0 / (double)((u32)w[5] << 16 | w[6]);
        auto_s.pos = 0;
        auto_s.acc = 0;
        auto_s.on = 1;
        fifo_r = fifo_w;
        if (!getenv("NDOGS_NOMIC") && !getenv("NDOGS_HEADLESS"))
            dev_open((int)(auto_s.rate + 0.5));
        break;
    case 0x42: /* auto sampling stop */
        auto_s.on = 0;
        dev_close();
        break;
    }
}
