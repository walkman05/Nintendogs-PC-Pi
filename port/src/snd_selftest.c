/* Self-test for port/src/snd.c (not part of the game build). Builds a synthetic shared work, wave archive (looped
 * PCM16 sine, looped IMA-ADPCM sine), bank (PCM, PSG, key split, drum set with noise) and a two-track sequence at
 * the DS main RAM address 0x02000000 (reserved with VirtualAlloc, as the port maps it), renders it, writes
 * C:\nintendogs_build\snd_test.wav and checks pitch (zero crossings), variables, status bits and ext pan/fader.
 *   gcc -m32 -O2 -I port/include -I include port/src/snd_selftest.c port/src/snd.c -o snd_selftest.exe */
#include "snd.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>

#define RAM 0x02000000u
#define SHARED (RAM + 0x0000)
#define SWAR (RAM + 0x1000)
#define SBNK (RAM + 0x2000)
#define SSEQ (RAM + 0x3000)

static u8 *M(u32 a) { return (u8 *)(size_t)a; }
static void w8(u32 a, u32 v) { *M(a) = (u8)v; }
static void w16(u32 a, u32 v) { w8(a, v); w8(a + 1, v >> 8); }
static void w32(u32 a, u32 v) { w16(a, v); w16(a + 2, v >> 16); }

static const s8 idx_tab[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };
static const u16 step_tab[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97,
    107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428,
    4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350,
    22385, 24623, 27086, 29794, 32767 };

static int adpcm_enc(int s, int *pred, int *idx)
{
    int st = step_tab[*idx], d = s - *pred, n = 0, diff;
    if (d < 0) { n = 8; d = -d; }
    if (d >= st) { n |= 4; d -= st; }
    if (d >= st / 2) { n |= 2; d -= st / 2; }
    if (d >= st / 4) n |= 1;
    diff = st >> 3;
    if (n & 1) diff += st >> 2;
    if (n & 2) diff += st >> 1;
    if (n & 4) diff += st;
    *pred += (n & 8) ? -diff : diff;
    if (*pred > 0x7fff) *pred = 0x7fff;
    if (*pred < -0x7fff) *pred = -0x7fff;
    *idx += idx_tab[n & 7];
    *idx = *idx < 0 ? 0 : *idx > 88 ? 88 : *idx;
    return n;
}

static const s16 sine32[32] = { 0, 4875, 9564, 13883, 17670, 20785, 23097, 24519, 25000, 24519, 23097, 20785,
                                17670, 13883, 9564, 4875, 0, -4875, -9564, -13883, -17670, -20785, -23097,
                                -24519, -25000, -24519, -23097, -20785, -17670, -13883, -9564, -4875 };

static void build(void)
{
    u32 w0 = SWAR + 0x40, w1 = SWAR + 0x400, a, i, p = SSEQ, t1;
    int pred = 0, idx = 0;
    /* wave archive: 2 waves */
    w32(SWAR + 0x18, 2);
    w32(SWAR + 0x1c, w0 - SWAR);
    w32(SWAR + 0x20, w1 - SWAR);
    /* wave 0: PCM16, 32-sample sine looped, 14080 Hz (A4 = 440 Hz) */
    w8(w0, 1); w8(w0 + 1, 1); w16(w0 + 2, 14080); w16(w0 + 4, 16756991 / 14080); w16(w0 + 6, 0); w32(w0 + 8, 16);
    for (i = 0; i < 32; i++) w16(w0 + 12 + i * 2, (u16)sine32[i]);
    /* wave 1: ADPCM, 8 periods (256 samples) looped after the header word */
    w8(w1, 2); w8(w1 + 1, 1); w16(w1 + 2, 14080); w16(w1 + 4, 16756991 / 14080); w16(w1 + 6, 1); w32(w1 + 8, 32);
    w16(w1 + 12, 0); w8(w1 + 14, 0); w8(w1 + 15, 0);
    for (i = 0; i < 256; i += 2) {
        int lo = adpcm_enc(sine32[i & 31], &pred, &idx), hi = adpcm_enc(sine32[(i + 1) & 31], &pred, &idx);
        w8(w1 + 16 + i / 2, lo | (hi << 4));
    }
    /* bank */
    w32(SBNK + 0x18, SWAR);
    w32(SBNK + 0x38, 4);
    a = SBNK + 0x60;
    w32(SBNK + 0x3c, 1 | ((a - SBNK) << 8));           /* 0: PCM wave 0, key 69 */
    w16(a, 0); w16(a + 2, 0); w8(a + 4, 69); w8(a + 5, 127); w8(a + 6, 127); w8(a + 7, 127); w8(a + 8, 127); w8(a + 9, 64);
    a += 10;
    w32(SBNK + 0x40, 2 | ((a - SBNK) << 8));           /* 1: PSG duty 3 (50%), key 60 */
    w16(a, 3); w16(a + 2, 0); w8(a + 4, 60); w8(a + 5, 127); w8(a + 6, 127); w8(a + 7, 127); w8(a + 8, 127); w8(a + 9, 64);
    a += 10;
    w32(SBNK + 0x44, 17 | ((a - SBNK) << 8));          /* 2: key split: <=60 ADPCM wave 1, else PCM wave 0 */
    w8(a, 60); w8(a + 1, 127); for (i = 2; i < 8; i++) w8(a + i, 0);
    a += 8;
    w16(a, 1); w16(a + 2, 1); w16(a + 4, 0); w8(a + 6, 69); w8(a + 7, 127); w8(a + 8, 127); w8(a + 9, 127); w8(a + 10, 127); w8(a + 11, 64);
    a += 12;
    w16(a, 1); w16(a + 2, 0); w16(a + 4, 0); w8(a + 6, 69); w8(a + 7, 127); w8(a + 8, 127); w8(a + 9, 127); w8(a + 10, 127); w8(a + 11, 64);
    a += 12;
    w32(SBNK + 0x48, 16 | ((a - SBNK) << 8));          /* 3: drum set 40..41: noise, PSG */
    w8(a, 40); w8(a + 1, 41);
    a += 2;
    w16(a, 3); w16(a + 2, 0); w16(a + 4, 0); w8(a + 6, 60); w8(a + 7, 127); w8(a + 8, 127); w8(a + 9, 127); w8(a + 10, 127); w8(a + 11, 64);
    a += 12;
    w16(a, 2); w16(a + 2, 1); w16(a + 4, 0); w8(a + 6, 60); w8(a + 7, 127); w8(a + 8, 127); w8(a + 9, 127); w8(a + 10, 127); w8(a + 11, 64);
    /* sequence */
#define B(x) w8(p++, x)
    B(0xfe); B(0x03); B(0x00);                        /* tracks 0, 1 */
    B(0x93); B(0x01); t1 = p; B(0); B(0); B(0);       /* open track 1 (patched below) */
    B(0xb0); B(0); B(5); B(0);                        /* var0 = 5 */
    B(0xb8); B(0); B(5); B(0);                        /* var0 == 5 -> true */
    B(0xa2); B(0xb1); B(0); B(10); B(0);              /* if: var0 += 10 */
    B(0xb9); B(0); B(100); B(0);                      /* var0 >= 100 -> false */
    B(0xa2); B(0xb1); B(0); B(0xe8); B(0x03);     /* if (false): var0 += 1000 */
    B(0xb0); B(16); B(0x34); B(0x12);                 /* global var 0 = 0x1234 */
    B(0x81); B(0); B(0x45); B(127); B(96);            /* prg 0, A4 1 s */
    B(0x81); B(1); B(0x51); B(127); B(96);            /* prg 1 (PSG), A5 1 s */
    B(0x81); B(2); B(0x39); B(127); B(96);            /* prg 2 (key split -> ADPCM), A3 1 s */
    B(0xd4); B(2); B(0x80); B(24); B(0xfc);           /* loop 2: rest 24 */
    B(0xff);
    w8(t1, (p - SSEQ) & 0xff); w8(t1 + 1, (p - SSEQ) >> 8); w8(t1 + 2, 0);
    B(0x80); B(0x82); B(0x38);                        /* rest 312 */
    B(0x81); B(3); B(40); B(127); B(48);              /* drum: noise at key 40 (20 semitones below 60: ~660 Hz LFSR clock), 0.5 s */
    B(0xff);
#undef B
}

static s16 buf[2 * SND_RATE * 6];

static int crossings(const s16 *s, int from, int to, int ch)
{
    int i, n = 0, state = 0;
    for (i = from; i < to; i++) {
        int x = s[2 * i + ch];
        if (x < -1000) state = -1;
        else if (x > 1000 && state < 0) { state = 1; n++; }
    }
    return n;
}
static double rms(const s16 *s, int from, int to, int ch)
{
    double e = 0;
    int i;
    for (i = from; i < to; i++) e += (double)s[2 * i + ch] * s[2 * i + ch];
    return to > from ? __builtin_sqrt(e / (to - from)) : 0;
}

static void write_wav(const char *path, const s16 *s, int frames)
{
    FILE *f = fopen(path, "wb");
    u32 data = frames * 4, v;
    if (!f) { printf("cannot write %s\n", path); return; }
    fwrite("RIFF", 1, 4, f); v = 36 + data; fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
    v = 1 | (2 << 16); fwrite(&v, 4, 1, f); v = SND_RATE; fwrite(&v, 4, 1, f);
    v = SND_RATE * 4; fwrite(&v, 4, 1, f); v = 4 | (16 << 16); fwrite(&v, 4, 1, f);
    fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
    fwrite(s, 4, frames, f);
    fclose(f);
}

static int fails;
static void check(int ok, const char *what) { printf("%s: %s\n", ok ? "PASS" : "FAIL", what); fails += !ok; }

int main(void)
{
    int done = 0, chunk = 547, total = SND_RATE * 9 / 2, n, sec = SND_RATE;
    u32 status_mid = 0, ticks;
    char msg[160];
    if (VirtualAlloc((void *)RAM, 0x400000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) != (void *)RAM) {
        printf("cannot map 0x02000000\n");
        return 1;
    }
    build();
    snd_init();
    snd_set_shared_work(SHARED);
    snd_command(2, 0, SSEQ, 0, SBNK);          /* PREPARE_SEQ player 0 */
    snd_command(6, 0, 4, 64, 1);               /* prio */
    snd_command(6, 0, 6, 0, 2);                /* extFader 0 dB */
    snd_command(3, 0, 0, 0, 0);                /* START_PREPARED_SEQ */
    while (done < total) {
        n = total - done < chunk ? total - done : chunk;
        snd_render(buf + 2 * done, n);
        done += n;
        if (done > sec / 2 && !status_mid)
            status_mid = *(u32 *)(size_t)(SHARED + 4) | (*(u16 *)(size_t)(SHARED + 8) << 16);
    }
    write_wav("C:\\nintendogs_build\\snd_test.wav", buf, total);

    n = crossings(buf, sec / 10, sec * 9 / 10, 0);
    sprintf(msg, "PCM16 A4: %d cycles in 0.8 s (expect 352 = 440 Hz)", n); check(n >= 349 && n <= 355, msg);
    n = crossings(buf, sec * 11 / 10, sec * 19 / 10, 0);
    sprintf(msg, "PSG A5: %d cycles in 0.8 s (expect 704 = 880 Hz)", n); check(n >= 700 && n <= 708, msg);
    n = crossings(buf, sec * 21 / 10, sec * 29 / 10, 1);
    sprintf(msg, "ADPCM via key split A3: %d cycles in 0.8 s (expect 176 = 220 Hz)", n); check(n >= 174 && n <= 178, msg);
    sprintf(msg, "levels L/R: %.0f/%.0f %.0f/%.0f %.0f/%.0f", rms(buf, sec / 10, sec * 9 / 10, 0),
            rms(buf, sec / 10, sec * 9 / 10, 1), rms(buf, sec * 11 / 10, sec * 19 / 10, 0),
            rms(buf, sec * 11 / 10, sec * 19 / 10, 1), rms(buf, sec * 21 / 10, sec * 29 / 10, 0),
            rms(buf, sec * 21 / 10, sec * 29 / 10, 1));
    check(rms(buf, sec / 10, sec * 9 / 10, 0) > 3000, msg);
    sprintf(msg, "noise drum rms %.0f, crossings %d", rms(buf, sec * 33 / 10, sec * 37 / 10, 0),
            crossings(buf, sec * 33 / 10, sec * 37 / 10, 0));
    check(rms(buf, sec * 33 / 10, sec * 37 / 10, 0) > 3000 && crossings(buf, sec * 33 / 10, sec * 37 / 10, 0) > 20, msg);
    sprintf(msg, "silence after the end (rms %.1f)", rms(buf, sec * 4, total, 0));
    check(rms(buf, sec * 4, total, 0) == 0, msg);
    sprintf(msg, "status at 0.5 s: playerStatus %x channelStatus %x", status_mid & 0xffff, status_mid >> 16);
    check((status_mid & 1) && (status_mid >> 16) == 0x10, msg);
    sprintf(msg, "status at end: playerStatus %x channelStatus %x", *(u32 *)(size_t)(SHARED + 4), *(u16 *)(size_t)(SHARED + 8));
    check(*(u32 *)(size_t)(SHARED + 4) == 0 && *(u16 *)(size_t)(SHARED + 8) == 0, msg);
    sprintf(msg, "var0 = %d (expect 15), global var0 = 0x%x (expect 0x1234)", *(s16 *)(size_t)(SHARED + 0x20),
            (u16)*(s16 *)(size_t)(SHARED + 0x260));
    check(*(s16 *)(size_t)(SHARED + 0x20) == 15 && *(s16 *)(size_t)(SHARED + 0x260) == 0x1234, msg);
    ticks = *(u32 *)(size_t)(SHARED + 0x20 + 32);
    sprintf(msg, "tickCounter %u (expect ~360)", ticks); check(ticks >= 355 && ticks <= 365, msg);

    /* ext pan full right, then extFader to the minimum */
    snd_command(0, 0, SSEQ, 0, SBNK);
    snd_command(7, 0 | (1 << 24), 1, 9, 127);
    snd_render(buf, sec / 2);
    sprintf(msg, "ext pan right: L %.0f R %.0f", rms(buf, sec / 10, sec / 2, 0), rms(buf, sec / 10, sec / 2, 1));
    check(rms(buf, sec / 10, sec / 2, 1) > 8 * rms(buf, sec / 10, sec / 2, 0), msg);
    snd_command(6, 0, 6, (u16)-723, 2);
    snd_render(buf, sec / 4);
    sprintf(msg, "extFader -723: rms %.1f after 20 ms", rms(buf, sec / 50, sec / 4, 1));
    check(rms(buf, sec / 50, sec / 4, 1) == 0, msg);
    snd_command(6, 0, 6, 0, 2);
    snd_command(1, 0, 0, 0, 0);                /* STOP_SEQ: release 127 -> silent */
    snd_render(buf, sec / 4);
    sprintf(msg, "stop: rms %.1f after 20 ms", rms(buf, sec / 50, sec / 4, 1));
    check(rms(buf, sec / 50, sec / 4, 1) == 0, msg);
    /* invalidate the wave archive mid-note: the voice stops at once */
    snd_command(0, 0, SSEQ, 0, SBNK);
    snd_render(buf, sec / 4);
    snd_command(32, SWAR, SWAR + 0x1000, 0, 0);
    snd_render(buf, sec / 4);
    sprintf(msg, "invalidate wave: rms %.1f, playerStatus %x", rms(buf, 8, sec / 4, 0), *(u32 *)(size_t)(SHARED + 4));
    check(rms(buf, 8, sec / 4, 0) == 0 && *(u32 *)(size_t)(SHARED + 4) == 0, msg);
    /* garbage: a sequence of random bytes and a bank pointing outside RAM must not crash */
    {
        u32 i, x = 1;
        for (i = 0; i < 0x1000; i++) { x = x * 1103515245u + 12345u; w8(RAM + 0x10000 + i, x >> 16); }
        w32(RAM + 0x20000 + 0x38, 0x7fffffff);
        snd_command(0, 1, RAM + 0x10000, 0, RAM + 0x10000);
        snd_command(0, 2, RAM + 0x10100, 0, 0x01000000);
        snd_command(0, 3, RAM + 0x3ffffe, 0, RAM + 0x20000);
        snd_render(buf, sec * 3);
        check(1, "random data survived");
    }
    printf("%s\n", fails ? "SOME TESTS FAILED" : "all tests passed");
    return fails != 0;
}
