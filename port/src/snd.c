/* ARM7 sound driver stand-in: the NitroSDK sound driver's behaviour, in C, behind the ARM9's sound commands.
 *
 * Written from the public descriptions of the DS sound hardware (GBATEK) and of the SDAT formats (SSEQ sequence
 * commands, SBNK instruments, SWAR/SWAV waves). Scope:
 *  - 16 sequence players x 16 tracks with the full SSEQ command set (notes, rests, jumps/calls/loops, random and
 *    variable arguments, conditionals, variables and comparisons, pan/volume/expression/transpose/pitch bend,
 *    priority, mono (note-wait) mode, ties, portamento, modulation, ADSR overrides, tempo, sweep), local/global
 *    variables and tick counters in SNDSharedWork, player/track parameters written by the ARM9;
 *  - bank lookup (PCM, PSG, noise, direct PCM, drum sets, key splits) and wave archives;
 *  - 16 voices like the hardware channels: PCM8/PCM16/IMA-ADPCM with loops, PSG squares on channels 8-13, noise
 *    on 14-15 (hardware LFSR), allocation by priority with stealing, ADSR envelope with the SDK's rate
 *    conversions, decibel volume summing, pitch in 1/64 semitones, LFO (pitch/volume/pan), sweep/portamento;
 *  - a mixer: linear interpolation to SND_RATE, pan, master volume, s16 clamp.
 * The driver frame (envelopes, LFOs, sequence ticks) runs every 174592 ARM7 cycles (~5.2 ms) of output.
 * Not implemented: direct channel/timer/capture/alarm commands (12-22), driver info (33), the hardware's volume
 * quantisation (volumes are applied as exact linear gains), capture.
 *
 * DS memory is read at its DS address; every pointer taken from game data is checked against main RAM (and the
 * DTCM/system area), so bad data stops a track or a note instead of crashing. No libm: the tables are built with
 * small series at init. */
#include "snd.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define NV 16          /* voices (hardware channels) */
#define NP 16          /* players */
#define NT 16          /* tracks per player */
#define ENV_MIN (-92544)
#define DB_MIN (-723)
#define UNITS_PER_OUT 33513982ull               /* ARM7 Hz: one output frame = 33513982 units */
#define UNITS_PER_FRAME (174592ull * SND_RATE)  /* one driver frame = 174592 cycles */

enum { K_PCM, K_PSG, K_NOISE };
enum { E_ATTACK, E_DECAY, E_SUSTAIN, E_RELEASE };
enum { ARG_U8, ARG_S16, ARG_VLV, ARG_RAN, ARG_VAR, ARG_NONE };

typedef struct { u8 type, speed, depth, range; u16 delay, delay_cnt, counter; } Lfo;

typedef struct {
    u8 active, ended, kind, fmt, loop, duty, ad_saved, env_state;
    u8 key, orig_key, velocity, auto_sweep;
    s8 inst_pan, owner_p, owner_t;
    u32 wave;                     /* DS address of the sample data (for INVALIDATE_WAVE) */
    const u8 *data;
    u32 total, loop_start, idx;   /* in samples */
    u32 base_timer, frac, step;   /* step: samples per output frame, 16.16 */
    s32 s0, s1;
    s32 ad_pred, ad_idx, ad_lpred, ad_lidx;
    u16 lfsr, phase;
    s32 env, sustain;
    u16 attack, decay, release;
    s32 length;                   /* ticks left (-1 = tie) */
    int prio;
    s32 user_vol, user_pitch, user_pan;   /* from the track: 0.1 dB, 1/64 semitone, pan offset */
    s32 sweep_pitch, sweep_len, sweep_cnt;
    Lfo lfo;
    s32 gl, gr, cur_db;
} Voice;

typedef struct {
    u8 used, active, err, mute;
    u8 note_wait, note_finish_wait, tie, portamento, cmp, sp;
    u8 volume, volume2, pan_range, bend_range, prio, porta_key, porta_time;
    u8 attack, decay, sustain, release;
    u8 mod_depth, mod_speed, mod_type, mod_range;
    s8 pan, ext_pan, pitch_bend, transpose;
    u16 prg, mod_delay;
    s16 ext_fader, ext_pitch, sweep_pitch;
    u32 base, pc, stack[3];
    u8 loopcnt[3];
    s32 wait;
} Track;

typedef struct {
    u8 active, started, paused, prio, volume;
    s16 ext_fader;
    u16 tempo, tempo_ratio;
    s32 tempo_cnt;
    u32 seq_base, bank, ticks;
    s16 var[16];
    Track tr[NT];
} Player;

typedef struct { int type; u16 wave[2]; u8 key, a, d, s, r, pan; } Inst;

static Voice s_v[NV];
static Player s_pl[NP];
static s16 s_gvar[16];
static u32 s_shared, s_alloc_mask, s_lock_mask, s_rng, s_master;
static s64 s_until;
static int s_ready;

static s16 s_dec[128];        /* 0..127 -> 0.1 dB, 20*log10((x/127)^2) */
static u16 s_gain[724];       /* -n*0.1 dB -> linear gain, 32768 = 1.0 */
static u32 s_ptab[768];       /* 2^(n/768) in 16.16 */
static s8 s_sin[33];          /* quarter sine, 0..127 */

static const u8 s_attack_tab[19] = { 0x00, 0x01, 0x05, 0x0E, 0x1A, 0x26, 0x33, 0x3F, 0x49, 0x54,
                                     0x5C, 0x64, 0x6D, 0x74, 0x7B, 0x7F, 0x84, 0x89, 0x8F };
static const s8 s_ad_index[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };
static const u16 s_ad_step[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97,
    107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428,
    4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350,
    22385, 24623, 27086, 29794, 32767 };

/* ---- tables (no libm) ----------------------------------------------------------------------------------------- */
#define LN2 0.69314718055994531
#define LOG2_10 3.32192809488736235

static double m_exp2(double x)
{
    int i = (int)x, k;
    double f, t = 1, s = 1;
    if ((double)i > x)
        i--;
    f = (x - i) * LN2;
    for (k = 1; k < 24; k++) {
        t *= f / k;
        s += t;
    }
    for (; i > 0; i--) s *= 2;
    for (; i < 0; i++) s *= 0.5;
    return s;
}

static double m_log2(double x)  /* x > 0 */
{
    int e = 0, k;
    double y, y2, t, s = 0;
    while (x >= 2) { x *= 0.5; e++; }
    while (x < 1) { x *= 2; e--; }
    y = (x - 1) / (x + 1);
    y2 = y * y;
    for (t = y, k = 1; k < 61; k += 2, t *= y2) s += t / k;
    return e + 2 * s / LN2;
}

static double m_sin(double x)  /* |x| <= pi/2 */
{
    double t = x, s = x;
    int k;
    for (k = 1; k < 10; k++) {
        t *= -x * x / ((2 * k) * (2 * k + 1));
        s += t;
    }
    return s;
}

static int iround(double x) { return x < 0 ? -(int)(-x + 0.5) : (int)(x + 0.5); }

static void init_tables(void)
{
    int i;
    s_dec[0] = DB_MIN;
    for (i = 1; i < 128; i++) {
        int d = iround(400.0 * m_log2(i / 127.0) / LOG2_10);
        s_dec[i] = (s16)(d < DB_MIN ? DB_MIN : d);
    }
    for (i = 0; i < 724; i++) s_gain[i] = (u16)iround(32768.0 * m_exp2(-i * LOG2_10 / 200.0));
    for (i = 0; i < 768; i++) s_ptab[i] = (u32)iround(65536.0 * m_exp2(i / 768.0));
    for (i = 0; i <= 32; i++) s_sin[i] = (s8)iround(127.0 * m_sin(i * 3.14159265358979324 / 64.0));
}

/* ---- DS memory ------------------------------------------------------------------------------------------------ */
static int mem_ok(u32 a, u32 n)
{
    if (a >= 0x02000000u && a < 0x02400000u)
        return n <= 0x02400000u - a;
    if (a >= 0x027c0000u && a < 0x02800000u)
        return n <= 0x02800000u - a;
    return 0;
}
static inline const u8 *P(u32 a) { return (const u8 *)(size_t)a; }
static inline u32 m8(u32 a) { return *P(a); }
static inline u32 m16(u32 a) { return P(a)[0] | (P(a)[1] << 8); }
static inline u32 m32(u32 a) { return m16(a) | (m16(a + 2) << 16); }

static u32 rnd16(void)
{
    s_rng = s_rng * 1664525u + 1013904223u;
    return s_rng >> 16;
}

static s16 *var_ptr(int pn, int n)
{
    if (n < 0 || n >= 32)
        return NULL;
    if (s_shared) {
        u8 *w = (u8 *)(size_t)s_shared;
        return n < 16 ? (s16 *)(w + 0x20 + pn * 36 + n * 2) : (s16 *)(w + 0x260 + (n - 16) * 2);
    }
    return n < 16 ? &s_pl[pn].var[n] : &s_gvar[n - 16];
}

/* ---- conversions ---------------------------------------------------------------------------------------------- */
static u16 attack_rate(int x) { x &= 127; return x >= 109 ? s_attack_tab[127 - x] : (u16)(255 - x); }
static u16 fall_rate(int x)  /* decay/release */
{
    x &= 127;
    if (x == 127) return 0xffff;
    if (x == 126) return 0x3c00;
    if (x < 50) return (u16)(x * 2 + 1);
    return (u16)(0x1e00 / (126 - x));
}

static u32 calc_timer(u32 base, s32 pitch)  /* base timer raised by pitch (1/64 semitone) */
{
    s32 q = -pitch, oct, f;
    u64 t;
    oct = q >= 0 ? q / 768 : -((-q + 767) / 768);
    f = q - oct * 768;
    if (oct >= 16)
        return 0xffff;
    if (oct < -40)
        return 0x10;
    t = ((u64)base * s_ptab[f]) >> (16 - oct);
    return t < 0x10 ? 0x10 : t > 0xffff ? 0xffff : (u32)t;
}

static s32 sin_idx(int i)
{
    i &= 127;
    if (i < 32) return s_sin[i];
    if (i < 64) return s_sin[64 - i];
    if (i < 96) return -s_sin[i - 64];
    return -s_sin[128 - i];
}
static s32 lfo_value(const Lfo *l)
{
    if (!l->depth || l->delay_cnt < l->delay)
        return 0;
    return sin_idx(l->counter >> 8) * l->depth * l->range;
}
static void lfo_step(Lfo *l)
{
    if (l->delay_cnt < l->delay)
        l->delay_cnt++;
    else
        l->counter = (u16)((l->counter + ((u32)l->speed << 6)) % (128u << 8));
}

/* ---- voices --------------------------------------------------------------------------------------------------- */
static inline s32 fetch(Voice *v)
{
    u32 i;
    if (v->kind == K_PSG) {
        s32 s = (v->duty < 7 && v->phase <= v->duty) ? 0x7fff : -0x7fff;
        v->phase = (v->phase + 1) & 7;
        return s;
    }
    if (v->kind == K_NOISE) {
        if (v->lfsr & 1) {
            v->lfsr = (u16)((v->lfsr >> 1) ^ 0x6000);
            return -0x7fff;
        }
        v->lfsr >>= 1;
        return 0x7fff;
    }
    if (v->idx >= v->total) {
        if (!v->loop || v->loop_start >= v->total) {
            v->ended = 1;
            return 0;
        }
        v->idx = v->loop_start;
        if (v->fmt == 2 && v->ad_saved) {
            v->ad_pred = v->ad_lpred;
            v->ad_idx = v->ad_lidx;
        }
    }
    i = v->idx++;
    if (v->fmt == 0)
        return (s8)v->data[i] << 8;
    if (v->fmt == 1)
        return (s16)(v->data[2 * i] | (v->data[2 * i + 1] << 8));
    {
        int n = (v->data[i >> 1] >> ((i & 1) * 4)) & 15;
        s32 st = s_ad_step[v->ad_idx], d = st >> 3;
        if (i == v->loop_start && !v->ad_saved) {
            v->ad_lpred = v->ad_pred;
            v->ad_lidx = v->ad_idx;
            v->ad_saved = 1;
        }
        if (n & 1) d += st >> 2;
        if (n & 2) d += st >> 1;
        if (n & 4) d += st;
        if (n & 8) {
            v->ad_pred -= d;
            if (v->ad_pred < -0x7fff) v->ad_pred = -0x7fff;
        } else {
            v->ad_pred += d;
            if (v->ad_pred > 0x7fff) v->ad_pred = 0x7fff;
        }
        v->ad_idx += s_ad_index[n & 7];
        v->ad_idx = v->ad_idx < 0 ? 0 : v->ad_idx > 88 ? 88 : v->ad_idx;
        return v->ad_pred;
    }
}

static int wave_setup(Voice *v, u32 w)  /* SNDWaveParam + samples */
{
    u32 fmt, ls, ll, words, data;
    if (!mem_ok(w, 12))
        return 0;
    fmt = m8(w);
    ls = m16(w + 6);
    ll = m32(w + 8);
    if (fmt > 2 || !m16(w + 4) || ll > 0x100000)
        return 0;
    words = ls + ll;
    data = w + 12;
    if (!mem_ok(data, words * 4))
        return 0;
    v->kind = K_PCM;
    v->fmt = (u8)fmt;
    v->loop = m8(w + 1) != 0;
    v->base_timer = m16(w + 4);
    v->wave = data;
    if (fmt == 2) {
        if (words < 2)
            return 0;
        v->ad_pred = (s16)m16(data);
        v->ad_idx = m8(data + 2) > 88 ? 88 : (s32)m8(data + 2);
        v->loop_start = ls ? (ls - 1) * 8 : 0;
        v->total = (words - 1) * 8;
        v->data = P(data + 4);
    } else {
        u32 spw = fmt ? 2 : 4;
        v->loop_start = ls * spw;
        v->total = words * spw;
        v->data = P(data);
    }
    return v->total != 0;
}

static int vprio(int c) { return s_v[c].active ? s_v[c].prio : 0; }

static Voice *voice_alloc(int kind, int prio)
{
    static const u8 pcm_order[16] = { 4, 5, 6, 7, 2, 0, 3, 1, 8, 9, 10, 11, 14, 12, 15, 13 };
    static const u8 psg_order[6] = { 8, 9, 10, 11, 12, 13 };
    static const u8 noise_order[2] = { 14, 15 };
    const u8 *order = kind == K_PCM ? pcm_order : kind == K_PSG ? psg_order : noise_order;
    int n = kind == K_PCM ? 16 : kind == K_PSG ? 6 : 2, k, best = -1;
    u32 mask = s_alloc_mask & ~s_lock_mask;
    for (k = 0; k < n; k++) {
        int c = order[k];
        if (!((mask >> c) & 1))
            continue;
        if (best < 0 || vprio(c) < vprio(best) || (vprio(c) == vprio(best) && s_v[c].cur_db < s_v[best].cur_db))
            best = c;
    }
    if (best < 0 || vprio(best) > prio)
        return NULL;
    memset(&s_v[best], 0, sizeof(Voice));
    s_v[best].owner_p = s_v[best].owner_t = -1;
    s_v[best].cur_db = -32768;
    return &s_v[best];
}

static void voice_free(Voice *v)
{
    v->active = 0;
    v->owner_p = v->owner_t = -1;
    v->cur_db = -32768;
    v->gl = v->gr = 0;
}

static void voice_release(Voice *v, int rate)
{
    if (rate >= 0)
        v->release = fall_rate(rate);
    v->env_state = E_RELEASE;
    v->prio = 1;
}

static int track_has_voice(int pn, int ti)
{
    int k;
    for (k = 0; k < NV; k++)
        if (s_v[k].active && s_v[k].owner_p == pn && s_v[k].owner_t == ti)
            return 1;
    return 0;
}

/* release (rate -1 = the voice's own) or kill a track's voices, and detach them from the track */
static void track_voices_off(int pn, int ti, int rate, int kill)
{
    int k;
    for (k = 0; k < NV; k++) {
        Voice *v = &s_v[k];
        if (!v->active || v->owner_p != pn || v->owner_t != ti)
            continue;
        if (kill)
            voice_free(v);
        else {
            voice_release(v, rate);
            v->owner_p = v->owner_t = -1;
        }
    }
}

/* the track's contribution to its voices: volume, pitch, pan, modulation parameters */
static void track_apply(Voice *v, const Player *p, const Track *t)
{
    s32 pan = t->pan;
    v->user_vol = s_dec[t->volume & 127] + s_dec[t->volume2 & 127] + s_dec[p->volume & 127] + t->ext_fader +
                  p->ext_fader;
    if (t->mute)
        v->user_vol = -32768;
    v->user_pitch = ((s32)t->pitch_bend * t->bend_range >> 1) + t->ext_pitch;
    if (t->pan_range != 127)
        pan = (pan * t->pan_range + 0x40) >> 7;
    v->user_pan = pan + t->ext_pan;
    v->lfo.type = t->mod_type;
    v->lfo.speed = t->mod_speed;
    v->lfo.depth = t->mod_depth;
    v->lfo.range = t->mod_range;
    v->lfo.delay = t->mod_delay;
}

/* one driver frame of a voice: release at note end, envelope, LFO, sweep, then timer step and gains */
static void voice_frame(Voice *v)
{
    s32 lfo, pitch, db, pan, sweep = 0, g;
    u32 timer;
    if (!v->active)
        return;
    if (v->ended) {
        voice_free(v);
        return;
    }
    if (v->length == 0 && v->env_state != E_RELEASE)
        voice_release(v, -1);
    switch (v->env_state) {
    case E_ATTACK:
        v->env = v->env * v->attack / 256;
        if (v->env == 0)
            v->env_state = E_DECAY;
        break;
    case E_DECAY:
        v->env -= v->decay;
        if (v->env <= v->sustain) {
            v->env = v->sustain;
            v->env_state = E_SUSTAIN;
        }
        break;
    case E_RELEASE:
        v->env -= v->release;
        if (v->env <= ENV_MIN) {
            voice_free(v);
            return;
        }
        break;
    }
    lfo = lfo_value(&v->lfo);
    lfo_step(&v->lfo);
    if (!v->auto_sweep && v->sweep_cnt < v->sweep_len)
        v->sweep_cnt++;
    if (v->sweep_pitch && v->sweep_cnt < v->sweep_len)
        sweep = (s32)((s64)v->sweep_pitch * (v->sweep_len - v->sweep_cnt) / v->sweep_len);

    pitch = ((s32)v->key - v->orig_key) * 64 + v->user_pitch + sweep;
    if (v->lfo.type == 0)
        pitch += (lfo * 64) >> 14;
    timer = calc_timer(v->base_timer, pitch);
    v->step = (u32)(UNITS_PER_OUT / timer);

    db = s_dec[v->velocity & 127] + v->user_vol + (v->env >> 7);
    if (v->lfo.type == 1)
        db += (lfo * 60) >> 14;
    db = db < DB_MIN ? DB_MIN : db > 0 ? 0 : db;
    pan = v->inst_pan + v->user_pan;
    if (v->lfo.type == 2)
        pan += lfo >> 8;
    pan = (pan < -64 ? -64 : pan > 63 ? 63 : pan) + 64;
    g = db <= DB_MIN ? 0 : (s32)s_gain[-db] * (s32)s_master / 127;
    v->gl = g * (128 - pan) >> 7;
    v->gr = g * pan >> 7;
    v->cur_db = db;
}

/* ---- banks ---------------------------------------------------------------------------------------------------- */
static int inst_lookup(u32 bank, int prg, int key, Inst *in)
{
    u32 rec, a, cnt;
    int type, i;
    if (!mem_ok(bank, 0x3c))
        return 0;
    cnt = m32(bank + 0x38);
    if (prg < 0 || (u32)prg >= cnt || cnt > 0x10000 || !mem_ok(bank + 0x3c, cnt * 4))
        return 0;
    rec = m32(bank + 0x3c + prg * 4);
    type = rec & 0xff;
    a = bank + (rec >> 8);
    if (type == 16) {  /* drum set */
        u32 lo, hi;
        if (!mem_ok(a, 2))
            return 0;
        lo = m8(a);
        hi = m8(a + 1);
        if ((u32)key < lo || (u32)key > hi)
            return 0;
        a += 2 + (key - lo) * 12;
        if (!mem_ok(a, 2))
            return 0;
        type = m16(a);
        a += 2;
    } else if (type == 17) {  /* key split */
        if (!mem_ok(a, 8))
            return 0;
        for (i = 0; i < 8; i++)
            if ((u32)key <= m8(a + i))
                break;
        if (i == 8)
            return 0;
        a += 8 + i * 12;
        if (!mem_ok(a, 2))
            return 0;
        type = m16(a);
        a += 2;
    }
    if (type < 1 || type > 4 || !mem_ok(a, 10))
        return 0;
    in->type = type;
    in->wave[0] = (u16)m16(a);
    in->wave[1] = (u16)m16(a + 2);
    in->key = (u8)m8(a + 4);
    in->a = (u8)m8(a + 5);
    in->d = (u8)m8(a + 6);
    in->s = (u8)m8(a + 7);
    in->r = (u8)m8(a + 8);
    in->pan = (u8)m8(a + 9);
    return 1;
}

static u32 wave_addr(u32 bank, int arc_no, u32 wave_no)
{
    u32 arc, cnt, off;
    if (arc_no > 3 || !mem_ok(bank + 0x18 + arc_no * 8, 4))
        return 0;
    arc = m32(bank + 0x18 + arc_no * 8);
    /* SNDWaveArc in memory: file + block headers (0x18), link pointer, 7 reserved words, wave count @0x38,
     * wave offsets @0x3c */
    if (!mem_ok(arc, 0x3c))
        return 0;
    cnt = m32(arc + 0x38);
    if (wave_no >= cnt || !mem_ok(arc + 0x3c + wave_no * 4, 4))
        return 0;
    off = m32(arc + 0x3c + wave_no * 4);
    if (!off)
        return 0;
    return off < 0x02000000u ? arc + off : off;
}

static int s_notelog;

/* ---- sequences ------------------------------------------------------------------------------------------------ */
static void note_on(int pn, Track *t, int ti, int key, int vel, s32 len)
{
    Player *p = &s_pl[pn];
    Voice *v = NULL;
    s32 sweep;
    int k;
    if (t->tie)
        for (k = 0; k < NV && !v; k++)
            if (s_v[k].active && s_v[k].owner_p == pn && s_v[k].owner_t == ti && s_v[k].env_state != E_RELEASE)
                v = &s_v[k];
    if (v) {
        v->key = (u8)key;
        v->velocity = (u8)vel;
    } else {
        Inst in;
        int prio = p->prio + t->prio, kind, ok;
        if (!inst_lookup(p->bank, t->prg, key, &in)) {
            if (s_notelog)
                fprintf(stderr, "note p%d t%d prg %d key %d: no instrument\n", pn, ti, t->prg, key);
            return;
        }
        kind = in.type == 2 ? K_PSG : in.type == 3 ? K_NOISE : K_PCM;
        v = voice_alloc(kind, prio > 255 ? 255 : prio);
        if (!v) {
            if (s_notelog)
                fprintf(stderr, "note p%d t%d prg %d key %d: no free voice (prio %d)%c", pn, ti, t->prg, key, prio, 10);
            return;
        }
        if (in.type == 1)
            ok = wave_setup(v, wave_addr(p->bank, in.wave[1], in.wave[0]));
        else if (in.type == 4)
            ok = wave_setup(v, in.wave[0] | ((u32)in.wave[1] << 16));
        else {
            v->kind = (u8)kind;
            v->duty = (u8)(in.wave[0] & 7);
            v->base_timer = 8006;  /* C4 (key 60) for a duty cycle of 8 steps */
            v->lfsr = 0x7fff;
            ok = 1;
        }
        if (!ok) {
            if (s_notelog)
                fprintf(stderr, "note p%d t%d prg %d key %d: type %d wave %d/%d not usable (addr %08x)\n", pn, ti,
                        t->prg, key, in.type, in.wave[0], in.wave[1], wave_addr(p->bank, in.wave[1], in.wave[0]));
            v->active = 0;
            if (s_notelog && in.type == 1 && in.wave[1] < 4) {
                u32 arc = m32(p->bank + 0x18 + in.wave[1] * 8), i;
                fprintf(stderr, "  bank %08x arc[%d] %08x", p->bank, in.wave[1], arc);
                if (mem_ok(arc, 0x40)) {
                    fprintf(stderr, " hdr %08x count %u offs", m32(arc), m32(arc + 0x38));
                    for (i = 0; i < 8; i++)
                        fprintf(stderr, " %08x", m32(arc + 0x3c + i * 4));
                }
                fputc(10, stderr);
            }
            return;
        }
        if (s_notelog)  /* NDOGS_SNDLOG=2: every note started */
            fprintf(stderr, "note p%d t%d prg %d key %d vel %d len %d: type %d fmt %d timer %u orig %d adsr %d/%d/%d/%d"
                    " loop %d total %u\n", pn, ti, t->prg, key, vel, (int)len, in.type, v->fmt, v->base_timer,
                    in.key, in.a, in.d, in.s, in.r, v->loop, v->total);
        v->active = 1;
        v->key = (u8)key;
        v->orig_key = in.key;
        v->velocity = (u8)vel;
        v->inst_pan = (s8)(in.pan - 64);
        v->attack = attack_rate(t->attack != 0xff ? t->attack : in.a);
        v->decay = fall_rate(t->decay != 0xff ? t->decay : in.d);
        v->sustain = (s32)s_dec[(t->sustain != 0xff ? t->sustain : in.s) & 127] << 7;
        v->release = fall_rate(t->release != 0xff ? t->release : in.r);
        v->env = ENV_MIN;
        v->env_state = E_ATTACK;
        v->prio = prio > 255 ? 255 : prio;
        v->length = t->tie ? -1 : len;
        v->owner_p = (s8)pn;
        v->owner_t = (s8)ti;
        v->s0 = 0;
        v->s1 = fetch(v);
        track_apply(v, p, t);
    }
    sweep = t->sweep_pitch;
    if (t->portamento)
        sweep += ((s32)t->porta_key - key) << 6;
    v->sweep_pitch = sweep;
    v->sweep_cnt = 0;
    if (t->porta_time == 0) {
        v->auto_sweep = 1;
        v->sweep_len = len;
    } else {
        v->auto_sweep = 0;
        v->sweep_len = (s32)(((u32)t->porta_time * t->porta_time * (u32)(sweep < 0 ? -sweep : sweep)) >> 11);
    }
}

static u32 rd8(Track *t)
{
    if (!mem_ok(t->pc, 1)) {
        t->err = 1;
        return 0;
    }
    return m8(t->pc++);
}
static u32 rd16(Track *t) { u32 lo = rd8(t); return lo | (rd8(t) << 8); }
static u32 rd24(Track *t) { u32 lo = rd16(t); return lo | (rd8(t) << 16); }
static s32 rd_vlv(Track *t)
{
    u32 x = 0, b;
    int k = 0;
    do {
        b = rd8(t);
        x = (x << 7) | (b & 0x7f);
    } while ((b & 0x80) && ++k < 4);
    return (s32)x;
}

static s32 rd_arg(int pn, Track *t, int at, int def)
{
    if (at == ARG_RAN) {
        s32 lo = (s16)rd16(t), hi = (s16)rd16(t);
        return lo + (s32)((rnd16() * (u32)(hi - lo + 1)) >> 16);
    }
    if (at == ARG_VAR) {
        s16 *vp = var_ptr(pn, (int)rd8(t));
        return vp ? *vp : 0;
    }
    if (def == ARG_U8) return (s32)rd8(t);
    if (def == ARG_S16) return (s16)rd16(t);
    return rd_vlv(t);
}

static void var_op(int pn, Track *t, int cmd, int n, s32 val)
{
    s16 *vp = var_ptr(pn, n);
    s32 x;
    if (!vp)
        return;
    x = *vp;
    switch (cmd) {
    case 0xb0: x = val; break;
    case 0xb1: x += val; break;
    case 0xb2: x -= val; break;
    case 0xb3: x *= val; break;
    case 0xb4: if (val) x /= val; break;
    case 0xb5:
        if (val >= 0) x = val > 31 ? 0 : (s32)((u32)x << val);
        else x = -val > 31 ? (x < 0 ? -1 : 0) : x >> -val;
        break;
    case 0xb6: {
        u32 r = rnd16();
        x = val < 0 ? -(s32)(r % (u32)(-val + 1)) : (s32)(r % (u32)(val + 1));
        break;
    }
    case 0xb8: t->cmp = x == val; return;
    case 0xb9: t->cmp = x >= val; return;
    case 0xba: t->cmp = x > val; return;
    case 0xbb: t->cmp = x <= val; return;
    case 0xbc: t->cmp = x < val; return;
    case 0xbd: t->cmp = x != val; return;
    default: return;
    }
    *vp = (s16)x;
}

static void track_init(Track *t, u32 base, u32 pc)
{
    memset(t, 0, sizeof *t);
    t->used = 1;
    t->base = base;
    t->pc = pc;
    t->volume = t->volume2 = 127;
    t->pan_range = 127;
    t->bend_range = 2;
    t->prio = 64;
    t->note_wait = 1;
    t->porta_key = 60;
    t->cmp = 1;
    t->attack = t->decay = t->sustain = t->release = 0xff;
    t->mod_speed = 16;
    t->mod_range = 1;
}

/* one sequence command; returns -1 at the end of the track */
static int exec(int pn, Track *t, int ti, int skip)
{
    Player *p = &s_pl[pn];
    u32 cmd = rd8(t);
    int doit = 1, at = ARG_NONE;
    s32 v;
    if (cmd == 0xa2) {
        cmd = rd8(t);
        doit = t->cmp;
    }
    if (cmd == 0xa0) {
        cmd = rd8(t);
        at = ARG_RAN;
    } else if (cmd == 0xa1) {
        cmd = rd8(t);
        at = ARG_VAR;
    }
    if (t->err)
        return -1;
    if (cmd < 0x80) {
        int vel = (int)rd8(t), key = (int)cmd + t->transpose;
        s32 len = rd_arg(pn, t, at, ARG_VLV);
        if (!doit || t->err)
            return t->err ? -1 : 0;
        if (len < 0)
            len = 0;
        key = key < 0 ? 0 : key > 127 ? 127 : key;
        if (!skip && !t->mute)
            note_on(pn, t, ti, key, vel & 127, len);
        t->porta_key = (u8)key;
        if (t->note_wait) {
            t->wait = len;
            if (len == 0)
                t->note_finish_wait = 1;
        }
        return 0;
    }
    switch (cmd) {
    case 0x80: v = rd_arg(pn, t, at, ARG_VLV); if (doit) t->wait = v < 0 ? 0 : v; break;
    case 0x81: v = rd_arg(pn, t, at, ARG_VLV); if (doit) t->prg = (u16)v; break;
    case 0x93: {
        u32 tn = rd8(t), off = rd24(t);
        if (doit && tn < NT && (int)tn != ti && !t->err) {
            Track *o = &p->tr[tn];
            if (!o->used)
                track_init(o, t->base, 0);
            o->pc = t->base + off;
            o->active = 1;
        }
        break;
    }
    case 0x94: { u32 off = rd24(t); if (doit) t->pc = t->base + off; break; }
    case 0x95: {
        u32 off = rd24(t);
        if (doit && t->sp < 3) {
            t->loopcnt[t->sp] = 0;
            t->stack[t->sp++] = t->pc;
            t->pc = t->base + off;
        }
        break;
    }
    case 0xfd: if (doit && t->sp) t->pc = t->stack[--t->sp]; break;
    case 0xd4:
        v = rd_arg(pn, t, at, ARG_U8);
        if (doit && t->sp < 3) {
            t->loopcnt[t->sp] = (u8)v;
            t->stack[t->sp++] = t->pc;
        }
        break;
    case 0xfc:
        if (doit && t->sp) {
            u8 c = t->loopcnt[t->sp - 1];
            if (c && --c == 0) {
                t->sp--;
                break;
            }
            t->loopcnt[t->sp - 1] = c;
            t->pc = t->stack[t->sp - 1];
        }
        break;
    case 0xe0: v = rd_arg(pn, t, at, ARG_S16); if (doit) t->mod_delay = (u16)v; break;
    case 0xe1: v = rd_arg(pn, t, at, ARG_S16); if (doit) p->tempo = (u16)v; break;
    case 0xe3: v = rd_arg(pn, t, at, ARG_S16); if (doit) t->sweep_pitch = (s16)v; break;
    case 0xfe: rd16(t); break;
    case 0xff: if (doit) return -1; break;
    default:
        if (cmd >= 0xb0 && cmd <= 0xbd) {
            int n = (int)rd8(t);
            v = rd_arg(pn, t, at, ARG_S16);
            if (doit && !t->err)
                var_op(pn, t, (int)cmd, n, v);
            break;
        }
        if (cmd < 0xc0 || cmd > 0xd6)
            return -1;  /* unknown command: stop the track */
        v = rd_arg(pn, t, at, ARG_U8);
        if (!doit)
            break;
        switch (cmd) {
        case 0xc0: t->pan = (s8)((v & 127) - 64); break;
        case 0xc1: t->volume = (u8)v; break;
        case 0xc2: p->volume = (u8)v; break;
        case 0xc3: t->transpose = (s8)v; break;
        case 0xc4: t->pitch_bend = (s8)v; break;
        case 0xc5: t->bend_range = (u8)v; break;
        case 0xc6: t->prio = (u8)v; break;
        case 0xc7: t->note_wait = (u8)(v & 1); break;
        case 0xc8: t->tie = (u8)(v & 1); track_voices_off(pn, ti, -1, 0); break;
        case 0xc9: {
            int k = v + t->transpose;
            t->porta_key = (u8)(k < 0 ? 0 : k > 127 ? 127 : k);
            t->portamento = 1;
            break;
        }
        case 0xca: t->mod_depth = (u8)v; break;
        case 0xcb: t->mod_speed = (u8)v; break;
        case 0xcc: t->mod_type = (u8)v; break;
        case 0xcd: t->mod_range = (u8)v; break;
        case 0xce: t->portamento = (u8)(v & 1); break;
        case 0xcf: t->porta_time = (u8)v; break;
        case 0xd0: t->attack = (u8)v; break;
        case 0xd1: t->decay = (u8)v; break;
        case 0xd2: t->sustain = (u8)v; break;
        case 0xd3: t->release = (u8)v; break;
        case 0xd5: t->volume2 = (u8)v; break;
        default: break;  /* 0xd6 print variable */
        }
        break;
    }
    return t->err ? -1 : 0;
}

static int track_tick(int pn, int ti, int skip)
{
    Track *t = &s_pl[pn].tr[ti];
    int k, guard = 0;
    for (k = 0; k < NV; k++) {
        Voice *v = &s_v[k];
        if (v->active && v->owner_p == pn && v->owner_t == ti) {
            if (v->length > 0)
                v->length--;
            if (v->auto_sweep && v->sweep_cnt < v->sweep_len)
                v->sweep_cnt++;
        }
    }
    if (t->note_finish_wait) {
        if (track_has_voice(pn, ti))
            return 0;
        t->note_finish_wait = 0;
    }
    if (t->wait > 0 && --t->wait > 0)
        return 0;
    while (t->wait == 0 && !t->note_finish_wait) {
        if (++guard > 1024)
            return 0;  /* zero-wait loop: carry on next tick */
        if (exec(pn, t, ti, skip) < 0)
            return -1;
    }
    return 0;
}

static void track_close(int pn, int ti, int kill)
{
    track_voices_off(pn, ti, -1, kill);
    s_pl[pn].tr[ti].active = 0;
}

static void player_stop(int pn, int kill)
{
    int i;
    for (i = 0; i < NT; i++)
        track_close(pn, i, kill);
    s_pl[pn].active = s_pl[pn].started = 0;
}

static int player_tick(int pn, int skip)  /* returns 1 when every track has ended */
{
    int i, any = 0;
    for (i = 0; i < NT; i++) {
        if (!s_pl[pn].tr[i].active)
            continue;
        if (track_tick(pn, i, skip) < 0)
            track_close(pn, i, 0);
        else
            any = 1;
    }
    return !any;
}

static void set_tick_counter(int pn)
{
    if (s_shared)
        memcpy((u8 *)(size_t)s_shared + 0x20 + pn * 36 + 32, &s_pl[pn].ticks, 4);
}

static void player_main(int pn)
{
    Player *p = &s_pl[pn];
    int n = 0, k;
    if (!p->active || !p->started || p->paused)
        return;
    while (p->tempo_cnt >= 240 && n < 64) {
        p->tempo_cnt -= 240;
        n++;
    }
    if (p->tempo_cnt >= 240)
        p->tempo_cnt = 0;
    for (k = 0; k < n; k++) {
        if (player_tick(pn, 0)) {
            player_stop(pn, 0);
            break;
        }
        p->ticks++;
    }
    set_tick_counter(pn);
    if (p->active)
        p->tempo_cnt += ((s32)p->tempo * p->tempo_ratio) >> 8;
}

static void player_prepare(int pn, u32 base, u32 off, u32 bank)
{
    if (s_notelog && mem_ok(base, 0x4000)) {  /* debugging: dump the sequence data */
        char name[64];
        FILE *f;
        snprintf(name, sizeof name, "seq_%08x.bin", base);
        if ((f = fopen(name, "wb"))) {
            fwrite(P(base), 1, 0x4000, f);
            fclose(f);
        }
        snprintf(name, sizeof name, "bank_%08x.bin", bank);  /* and the bank it plays with */
        if (mem_ok(bank, 0x40) && (f = fopen(name, "wb"))) {
            fwrite(P(bank), 1, 0x40, f);
            fclose(f);
        }
        fprintf(stderr, "prepare player %d seq %08x+%x bank %08x\n", pn, base, off, bank);
    }
    Player *p = &s_pl[pn];
    u32 pc = base + off, mask = 1;
    int i;
    player_stop(pn, 0);
    memset(p->tr, 0, sizeof p->tr);
    p->seq_base = base;
    p->bank = bank;
    p->tempo = 120;
    p->tempo_ratio = 256;
    p->tempo_cnt = 240;
    p->volume = 127;
    p->prio = 64;
    p->ext_fader = 0;
    p->ticks = 0;
    p->paused = p->started = 0;
    for (i = 0; i < 16; i++)
        *var_ptr(pn, i) = -1;
    set_tick_counter(pn);
    if (!mem_ok(pc, 1))
        return;
    if (mem_ok(pc, 3) && m8(pc) == 0xfe) {
        mask = m16(pc + 1) | 1;
        pc += 3;
    }
    for (i = 0; i < NT; i++)
        if ((mask >> i) & 1)
            track_init(&p->tr[i], base, pc);
    p->tr[0].active = 1;
    p->active = 1;
}

/* ---- driver frame --------------------------------------------------------------------------------------------- */
static void shared_update(void)
{
    u32 ps = 0, cs = 0;
    int i;
    u8 *w = (u8 *)(size_t)s_shared;
    for (i = 0; i < NP; i++)
        if (s_pl[i].active)
            ps |= 1u << i;
    for (i = 0; i < NV; i++)
        if (s_v[i].active)
            cs |= 1u << i;
    memcpy(w + 4, &ps, 4);
    memcpy(w + 8, &cs, 2);
    memset(w + 0xa, 0, 2);
}

static void driver_frame(void)
{
    int i;
    for (i = 0; i < NP; i++)
        player_main(i);
    for (i = 0; i < NV; i++) {
        Voice *v = &s_v[i];
        if (v->active && v->owner_p >= 0) {
            Player *p = &s_pl[v->owner_p];
            if (p->active)
                track_apply(v, p, &p->tr[v->owner_t]);
        }
        voice_frame(v);
    }
    if (s_shared)
        shared_update();
}

static void mix(s32 *acc, int n)
{
    int k, i;
    for (k = 0; k < NV; k++) {
        Voice *v = &s_v[k];
        s32 gl = v->gl, gr = v->gr;
        u32 step = v->step;
        if (!v->active || v->ended)
            continue;
        for (i = 0; i < n; i++) {
            s32 s;
            v->frac += step;
            while (v->frac >= 0x10000) {
                v->frac -= 0x10000;
                v->s0 = v->s1;
                v->s1 = fetch(v);
            }
            s = v->s0 + (((v->s1 - v->s0) * (s32)(v->frac >> 2)) >> 14);
            acc[2 * i] += (s * gl) >> 15;
            acc[2 * i + 1] += (s * gr) >> 15;
        }
    }
}

/* ---- interface ------------------------------------------------------------------------------------------------ */
void snd_init(void)
{
    int i;
    if (!s_ready)
        init_tables();
    s_notelog = getenv("NDOGS_SNDLOG") && atoi(getenv("NDOGS_SNDLOG")) >= 2;
    s_ready = 1;
    memset(s_v, 0, sizeof s_v);
    memset(s_pl, 0, sizeof s_pl);
    for (i = 0; i < NV; i++)
        voice_free(&s_v[i]);
    for (i = 0; i < 16; i++)
        s_gvar[i] = -1;
    s_shared = 0;
    s_alloc_mask = 0xffff;
    s_lock_mask = 0;
    s_master = 127;
    s_rng = 0x12345678u;
    s_until = 0;
}

static void ensure_init(void)
{
    if (!s_ready)
        snd_init();
}

void snd_set_shared_work(u32 addr)
{
    int pn;
    ensure_init();
    if (!mem_ok(addr, 0x280))
        return;
    /* the variables live in the shared work from now on (the ARM9 reads them there) */
    for (pn = 0; pn < NP; pn++) {
        memcpy((u8 *)(size_t)addr + 0x20 + pn * 36, s_pl[pn].var, 32);
        memcpy((u8 *)(size_t)addr + 0x20 + pn * 36 + 32, &s_pl[pn].ticks, 4);
    }
    memcpy((u8 *)(size_t)addr + 0x260, s_gvar, 32);
    s_shared = addr;
    shared_update();
}

static void put16(s16 *f, int hi, u32 b) { *f = (s16)(hi ? ((*f & 0xff) | (b << 8)) : ((*f & 0xff00) | b)); }
static void putu16(u16 *f, int hi, u32 b) { *f = (u16)(hi ? ((*f & 0xff) | (b << 8)) : ((*f & 0xff00) | b)); }

static void player_byte(Player *p, u32 off, u32 b)
{
    switch (off) {
    case 4: p->prio = (u8)b; break;
    case 5: p->volume = (u8)b; break;
    case 6: case 7: put16(&p->ext_fader, off & 1, b); break;
    case 0x18: case 0x19: putu16(&p->tempo, off & 1, b); break;
    case 0x1a: case 0x1b: putu16(&p->tempo_ratio, off & 1, b); break;
    default: break;
    }
}

static void track_byte(Track *t, u32 off, u32 b)
{
    switch (off) {
    case 1: t->pan_range = (u8)b; break;
    case 2: case 3: putu16(&t->prg, off & 1, b); break;
    case 4: t->volume = (u8)b; break;
    case 5: t->volume2 = (u8)b; break;
    case 6: t->pitch_bend = (s8)b; break;
    case 7: t->bend_range = (u8)b; break;
    case 8: t->pan = (s8)b; break;
    case 9: t->ext_pan = (s8)b; break;
    case 0xa: case 0xb: put16(&t->ext_fader, off & 1, b); break;
    case 0xc: case 0xd: put16(&t->ext_pitch, off & 1, b); break;
    default: break;
    }
}

static int player_uses_waves(const Player *p, u32 lo, u32 hi)
{
    int i;
    for (i = 0; i < 4; i++) {
        u32 a = p->bank + 0x18 + i * 8;
        if (mem_ok(a, 4) && m32(a) >= lo && m32(a) < hi)
            return 1;
    }
    return 0;
}

void snd_command(u32 id, u32 a0, u32 a1, u32 a2, u32 a3)
{
    u32 pn = a0 & 0xffffff, i;
    ensure_init();
    switch (id) {
    case 0:  /* START_SEQ */
    case 2:  /* PREPARE_SEQ */
        if (pn < NP) {
            player_prepare((int)pn, a1, a2, a3);
            if (id == 0 && s_pl[pn].active)
                s_pl[pn].started = 1;
        }
        break;
    case 1: if (pn < NP) player_stop((int)pn, 0); break;
    case 3: if (pn < NP && s_pl[pn].active) s_pl[pn].started = 1; break;
    case 4:  /* PAUSE_SEQ */
        if (pn < NP && s_pl[pn].active) {
            s_pl[pn].paused = a1 != 0;
            if (a1)
                for (i = 0; i < NT; i++)
                    track_voices_off((int)pn, (int)i, 127, 0);
        }
        break;
    case 5:  /* SKIP_SEQ */
        if (pn < NP && s_pl[pn].active)
            for (i = 0; i < a1 && i < 100000 && s_pl[pn].active; i++) {
                if (player_tick((int)pn, 1))
                    player_stop((int)pn, 0);
                else
                    s_pl[pn].ticks++;
            }
        break;
    case 6:  /* PLAYER_PARAM: offset a1, data a2, size a3 */
        if (pn < NP)
            for (i = 0; i < a3 && i < 4; i++)
                player_byte(&s_pl[pn], a1 + i, (a2 >> (8 * i)) & 0xff);
        break;
    case 7: {  /* TRACK_PARAM: a0 = player | size << 24, a1 track mask, a2 offset, a3 data */
        u32 size = a0 >> 24, t, k;
        pn = a0 & 0xff;
        if (pn >= NP)
            break;
        for (t = 0; t < NT; t++)
            if (((a1 >> t) & 1) && s_pl[pn].tr[t].used)
                for (k = 0; k < size && k < 4; k++)
                    track_byte(&s_pl[pn].tr[t], a2 + k, (a3 >> (8 * k)) & 0xff);
        break;
    }
    case 8:  /* MUTE_TRACK: flag 0 off, 1 mute, 2 mute + release, 3 mute + stop */
        if (pn < NP)
            for (i = 0; i < NT; i++)
                if ((a1 >> i) & 1) {
                    s_pl[pn].tr[i].mute = a2 != 0;
                    if (a2 >= 2)
                        track_voices_off((int)pn, (int)i, -1, a2 == 3);
                }
        break;
    case 9: s_alloc_mask = a0 & 0xffff; break;
    case 10:
        if (pn < NP && a1 < 16)
            *var_ptr((int)pn, (int)a1) = (s16)a2;
        break;
    case 11:
        if (a0 < 16)
            *var_ptr(0, (int)a0 + 16) = (s16)a1;
        break;
    case 23: s_master = a0 > 127 ? 127 : a0; break;
    case 26:  /* LOCK_CHANNEL */
        for (i = 0; i < NV; i++)
            if ((a0 >> i) & 1)
                voice_free(&s_v[i]);
        s_lock_mask |= a0 & 0xffff;
        break;
    case 27: s_lock_mask &= ~a0; break;
    case 30:  /* INVALIDATE_SEQ */
    case 31:  /* INVALIDATE_BANK */
    case 32:  /* INVALIDATE_WAVE */
        for (i = 0; i < NP; i++) {
            Player *p = &s_pl[i];
            int hit = id == 30 ? (p->seq_base >= a0 && p->seq_base < a1)
                    : id == 31 ? (p->bank >= a0 && p->bank < a1) : player_uses_waves(p, a0, a1);
            if (p->active && hit)
                player_stop((int)i, id == 32);
        }
        if (id == 32)
            for (i = 0; i < NV; i++)
                if (s_v[i].active && s_v[i].kind == K_PCM && s_v[i].wave >= a0 && s_v[i].wave < a1)
                    voice_free(&s_v[i]);
        break;
    default: break;  /* 12-22 direct channel/timer/capture/alarm, 33 driver info */
    }
}

void snd_render(s16 *out, int frames)
{
    static s32 acc[2 * 256];
    ensure_init();
    while (frames > 0) {
        int n, i;
        if (s_until <= 0) {
            driver_frame();
            s_until += (s64)UNITS_PER_FRAME;
        }
        n = (int)((s_until + (s64)UNITS_PER_OUT - 1) / (s64)UNITS_PER_OUT);
        if (n > frames) n = frames;
        if (n > 256) n = 256;
        if (n < 1) n = 1;
        memset(acc, 0, (size_t)n * 2 * sizeof(s32));
        mix(acc, n);
        for (i = 0; i < 2 * n; i++)
            out[i] = (s16)(acc[i] < -32768 ? -32768 : acc[i] > 32767 ? 32767 : acc[i]);
        out += 2 * n;
        frames -= n;
        s_until -= (s64)n * (s64)UNITS_PER_OUT;
    }
}
