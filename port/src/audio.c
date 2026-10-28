/* Host audio output: 16-bit stereo at the sound driver's rate, pushed once per VBlank from the game thread.
 * Windows: waveOut (winmm, loaded at run time like user32, after the DS memory map is reserved). Linux: ALSA through
 * dlopen (no build dependency; silent if libasound is missing). NDOGS_WAV=<file> also records everything to a WAV
 * file (for headless tests); NDOGS_MUTE=1 opens no device. Output gain: the driver's levels follow the SDK's decibel
 * mix, which with the game's own faders comes out quiet, so the host adds +6 dB by default (loud gameplay
 * effects then peak just below full scale; NDOGS_VOLUME=<percent of that>, clamped to s16). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ds_video.h"

static FILE *wav;
static u32 wav_bytes;
static int wav_rate;

static void wav_header(void)
{
    u32 h[11];
    h[0] = 0x46464952;  /* RIFF */
    h[1] = 36 + wav_bytes;
    h[2] = 0x45564157;  /* WAVE */
    h[3] = 0x20746d66;  /* fmt */
    h[4] = 16;
    h[5] = 1 | (2 << 16);
    h[6] = (u32)wav_rate;
    h[7] = (u32)wav_rate * 4;
    h[8] = 4 | (16 << 16);
    h[9] = 0x61746164;  /* data */
    h[10] = wav_bytes;
    fseek(wav, 0, SEEK_SET);
    fwrite(h, 4, 11, wav);
    fseek(wav, 0, SEEK_END);
    fflush(wav);
}

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>

#define NBUF 8
#define BUF_FRAMES 2048
static struct {
    MMRESULT (WINAPI *Open)(LPHWAVEOUT, UINT, LPCWAVEFORMATEX, DWORD_PTR, DWORD_PTR, DWORD);
    MMRESULT (WINAPI *Prepare)(HWAVEOUT, LPWAVEHDR, UINT);
    MMRESULT (WINAPI *Write)(HWAVEOUT, LPWAVEHDR, UINT);
} A;
static HWAVEOUT dev;
unsigned audio_underruns, audio_drops;
static WAVEHDR hdr[NBUF];
static s16 bufs[NBUF][BUF_FRAMES * 2];

static int dev_open(int rate)
{
    HMODULE w = LoadLibraryA("winmm.dll");
    WAVEFORMATEX f;
    int i;
    if (!w)
        return 0;
    *(FARPROC *)&A.Open = GetProcAddress(w, "waveOutOpen");
    *(FARPROC *)&A.Prepare = GetProcAddress(w, "waveOutPrepareHeader");
    *(FARPROC *)&A.Write = GetProcAddress(w, "waveOutWrite");
    if (!A.Open || !A.Prepare || !A.Write)
        return 0;
    memset(&f, 0, sizeof f);
    f.wFormatTag = WAVE_FORMAT_PCM;
    f.nChannels = 2;
    f.nSamplesPerSec = (DWORD)rate;
    f.wBitsPerSample = 16;
    f.nBlockAlign = 4;
    f.nAvgBytesPerSec = (DWORD)rate * 4;
    if (A.Open(&dev, WAVE_MAPPER, &f, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
        return 0;
    for (i = 0; i < NBUF; i++) {
        hdr[i].lpData = (LPSTR)bufs[i];
        hdr[i].dwBufferLength = sizeof bufs[i];
        A.Prepare(dev, &hdr[i], sizeof hdr[i]);
        hdr[i].dwFlags |= WHDR_DONE;
    }
    return 1;
}

/* Each push becomes one buffer. About 3 frames are kept queued (~50 ms): when the queue runs dry a frame of silence
 * is queued first, and when it's full (the game ran catch-up frames) the push is dropped. */
static void dev_push(const s16 *s, int frames)
{
    int i, free_n = 0, slot = -1;
    if (!dev || frames > BUF_FRAMES)
        return;
    for (i = 0; i < NBUF; i++)
        if (hdr[i].dwFlags & WHDR_DONE) {
            free_n++;
            if (slot < 0)
                slot = i;
        }
    if (slot < 0 || free_n <= NBUF - 6) {
        audio_drops++;
        return;
    }
    if (free_n == NBUF) {  /* underrun: pad with silence */
        audio_underruns++;
        memset(bufs[slot], 0, (size_t)frames * 4);
        hdr[slot].dwBufferLength = (DWORD)frames * 4;
        hdr[slot].dwFlags &= ~WHDR_DONE;
        A.Write(dev, &hdr[slot], sizeof hdr[slot]);
        for (slot = 0; slot < NBUF && !(hdr[slot].dwFlags & WHDR_DONE); slot++)
            ;
        if (slot == NBUF)
            return;
    }
    memcpy(bufs[slot], s, (size_t)frames * 4);
    hdr[slot].dwBufferLength = (DWORD)frames * 4;
    hdr[slot].dwFlags &= ~WHDR_DONE;
    A.Write(dev, &hdr[slot], sizeof hdr[slot]);
}

#else
#include <dlfcn.h>
static void *pcm;
static int (*a_writei)(void *, const void *, unsigned long);
static int (*a_prepare)(void *);

static int dev_open(int rate)
{
    void *lib = dlopen("libasound.so.2", RTLD_NOW);
    int (*open_)(void **, const char *, int, int);
    int (*set_params)(void *, int, int, unsigned, unsigned, int, unsigned);
    if (!lib)
        return 0;
    open_ = (int (*)(void **, const char *, int, int))dlsym(lib, "snd_pcm_open");
    set_params = (int (*)(void *, int, int, unsigned, unsigned, int, unsigned))dlsym(lib, "snd_pcm_set_params");
    a_writei = (int (*)(void *, const void *, unsigned long))dlsym(lib, "snd_pcm_writei");
    a_prepare = (int (*)(void *))dlsym(lib, "snd_pcm_prepare");
    if (!open_ || !set_params || !a_writei || !a_prepare)
        return 0;
    /* playback, non-blocking; S16_LE (2), interleaved read/write access (3), 50 ms latency */
    if (open_(&pcm, "default", 0, 1) < 0 || set_params(pcm, 2, 3, 2, (unsigned)rate, 1, 50000) < 0) {
        pcm = 0;
        return 0;
    }
    return 1;
}

static void dev_push(const s16 *s, int frames)
{
    if (pcm && a_writei(pcm, s, (unsigned long)frames) < 0)
        a_prepare(pcm);  /* underrun: recover, drop this push */
}
#endif

void audio_open(int rate)
{
    wav_rate = rate;
    if (getenv("NDOGS_WAV")) {
        wav = fopen(getenv("NDOGS_WAV"), "wb");
        if (wav)
            wav_header();
    }
    if (!getenv("NDOGS_MUTE") && !getenv("NDOGS_HEADLESS") && !dev_open(rate))
        fprintf(stderr, "audio: no output device\n");
}

static int gain = -1;  /* 256 = 1.0 */

void audio_push(const s16 *in, int frames)
{
    static s16 stereo[4096 * 2];
    int i;
    if (gain < 0)
        gain = getenv("NDOGS_VOLUME") ? atoi(getenv("NDOGS_VOLUME")) * 512 / 100 : 512;
    if (frames > 4096)
        frames = 4096;
    for (i = 0; i < frames * 2; i++) {
        s32 v = (in[i] * gain) >> 8;
        stereo[i] = (s16)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
    }
    if (wav) {
        fwrite(stereo, 4, (size_t)frames, wav);
        wav_bytes += (u32)frames * 4;
        if ((wav_bytes & 0xfffff) < (u32)frames * 4)
            wav_header();  /* keep the file valid while running */
    }
    dev_push(stereo, frames);
}
