#include "ctx.h"
#include "nitro.h"
/* Dog per-frame update (node callback at +0x14 of the dog object): runs the pending action callback, steps
 * position/velocity and the per-state handler, updates the head/ear/eye blend states and weights, the head bob
 * offset for the current gait, the motion events (+0x166) and expression/tail animations, the four animation
 * channels (+0xc2.., starting them through func_01ffd8f8), the turn speed, the orientation basis and model
 * matrices, colours and effects, then ticks the animations (func_01ffe474).
 * The original keeps many constants and locals in stack slots; they are plain C locals here. */

/* ---- data ---- */
typedef void (*DogFunc)(u8 *dog);
extern DogFunc data_0213bba4[];    /* pending action callbacks, by s16 +0xfa */
extern DogFunc data_02138a84[];    /* motion handlers, by s16 +0xea */
extern DogFunc data_0213b8a8[];    /* head state callbacks, by s8 +0x93 */
extern u8 *data_0214f460;
extern u8 *data_0214f464;          /* game state; word 0 is the random seed */
extern u8 *data_02159468;
extern s32 data_0213b90c[];        /* side state: { max, incA, incB } */
extern s32 data_0213be7c[];        /* head state (stride 4 words): max/inc for +0x450 and +0x454 */
extern s32 data_0213be80[];
extern s32 data_0213be84[];
extern s32 data_0213be88[];
extern s16 data_02126868[];        /* FX sin/cos table (sin at even indices) */
extern u16 data_0213b5a8[];
extern u8 data_0213c41c[];         /* per dog type, 100 bytes each */
extern s8 data_0213c42c[];         /* = data_0213c41c + 0x10 */
extern s8 data_0213c42d[];         /* = data_0213c41c + 0x11 */
extern s32 data_0213c470[];        /* = data_0213c41c + 0x54 */
extern s16 data_0213b578[];        /* per channel: 2 animation entries (-1 = none) */
extern u16 data_0213b750[];        /* per tail mode (stride 4 bytes): resource index */
extern s8 data_0213b752[];
extern s8 data_0213b753[];
extern u32 data_0213b6e4[];
extern s16 data_0213b4fc[];
extern u8 data_0213cde0[];         /* channel 0 table, 8 bytes per entry */
extern u8 *data_0213b568[];        /* channels 1-3 tables, 6 bytes per entry */
extern u8 data_0212b14c[];         /* resource descriptors, 0x18 bytes each, 0x267 entries */
extern char data_0213e100[];       /* name format */
extern s16 data_0213b4a8[];
extern s16 data_0213b484[];
extern u8 data_027c0bbc[];
extern s16 data_0213b45c[];

/* ---- functions ---- */
extern fx32 func_01ff832c(fx32 a, const VecFx32 *v1, const VecFx32 *v2, VecFx32 *dst); /* VEC_MultAdd */
extern void func_01ff810c(const MtxFx43 *a, const MtxFx43 *b, MtxFx43 *ab);           /* MTX_Concat43 */
extern void func_01ff86bc(const s32 *a, const s32 *b, s32 *out, s32 t);               /* rotation blend */
extern BOOL func_01ff89d0(VecFx32 *v);                                                 /* guarded VEC_Normalize */
extern void func_01ffd8f8(u8 *dog, s32 idx, u32 res, u32 id, s32 frame, s16 spd, s32 mode, s16 startMode,
                          s16 len);
extern void func_01ffe154(u8 *p, s32 a, u32 b);
extern void func_01ffe474(u8 *obj);
extern void func_02067d04(const VecFx32 *a, const VecFx32 *b, VecFx32 *axb);           /* VEC_CrossProduct */
extern void func_02067e34(const VecFx32 *a, const VecFx32 *b, VecFx32 *ab);            /* VEC_Subtract */
extern void func_02067e68(s32 *param_1, s32 *param_2, s32 *param_3);                   /* VEC_Add */
extern u64 func_0205e270(s32 a, s32 b);                                                /* divide, quotient low */
extern void func_0205f050(u32 param_1, u32 param_2, u32 param_3, ...);                 /* OS_SNPrintf */
extern u64 func_0206c8d4(u32 *src, u32 *dst);                                          /* 48-byte copy */
extern void func_02066bdc(u32 *param_1, u32 *param_2);
extern void func_02009230(u32 param_1);
extern void func_02009698(u8 *param_1);
extern void func_0203679c(u8 *p);
extern void func_02039d08(u8 *p);
extern void func_0203ad20(u8 *p);
extern void func_0203b3fc(u8 *param_1);
extern void func_0203b794(u8 *p);
extern void func_0203bc54(u8 *dog);
extern void func_0203e458(u8 *p, u32 val);
extern void func_020aad40(u8 *o, s32 idx);
extern s32 func_020aaeb8(u8 *p, u32 i);
extern void func_020bbe14(u32 param_1, s32 param_2);
extern s32 func_020bc14c(void);
extern u32 func_020bc3c4(void);
extern u8 *func_020bcad8(s32 param_1);
extern void func_0208e468(u8 *param_1, u8 *param_2);
extern BOOL func_0208cac8(u8 *p, s16 i);
extern void func_0208ca50(u8 *param_1, s16 param_2);
extern void func_0208ca9c(u8 *param_1, s16 param_2);
extern void func_0208ec84(u8 *param_1, void *param_2, u32 *param_3);
extern void func_0209c33c(void *param_1, s32 *param_2, s32 *param_3, u16 param_4);
/* These are tail calls in the original whose result (or extra argument) is passed through; the decompiled
 * definitions in src/ are declared void / with fewer parameters. Declared here as the original uses them. */
extern u32 func_020bc130(void);
extern u32 func_0208b9e8(u32 name);
extern u32 func_0208ca20(u8 *obj, s16 idx);
extern void func_0208f820(u8 *p, u32 color);
extern void func_0208f7e4(u8 *p, u32 color);

#define S8(o) (*(s8 *)(dog + (o)))
#define U8(o) (*(u8 *)(dog + (o)))
#define S16(o) (*(s16 *)(dog + (o)))
#define U16(o) (*(u16 *)(dog + (o)))
#define S32(o) (*(s32 *)(dog + (o)))
#define VEC(o) ((VecFx32 *)(dog + (o)))
#define TYPE() (*(u8 **)(dog + 0x9e8))
#define SIN(i) (data_02126868[(i) * 2])
#define NORES 0x267

static s32 Rnd(s32 a, s32 b)
{
    return (s32)(u32)(((u64)((s64)a * b) + 0x800u) >> 12);
}

static void CopyVec(u8 *dst, const u8 *src)
{
    *(VecFx32 *)dst = *(const VecFx32 *)src;
}

/* Raises *w towards max by inc (only while below max). */
static void Approach(s32 *w, s32 max, s32 inc)
{
    if (*w < max) {
        *w += inc;
        if (*w > max) {
            *w = max;
        }
    }
}

/* Side (ear) state: on -> onSt (capturing the side's target point when entering), else offSt. */
static void SideState(u8 *dog, int side, BOOL on, s8 onSt, s8 offSt)
{
    if (on) {
        if (S8(0x91 + side) != onSt) {
            CopyVec(dog + 0x3f0 + side * 12, dog + 0x30c + side * 12);
        }
        S8(0x91 + side) = onSt;
    } else {
        S8(0x91 + side) = offSt;
    }
}

/* The game's "every remote controller slot is free" check, then release of the controller. */
static void ReleaseIfAllFree(void)
{
    BOOL allFree = TRUE;
    s8 k;

    for (k = 0; k < 3; k = (s8)(k + 1)) {
        if (func_020aaeb8(data_0214f460, (u32)(s32)k) != -1) {
            allFree = FALSE;
            break;
        }
    }
    if (allFree) {
        func_020bbe14(1, -1);
        *(u32 *)(func_020bc3c4() + 0x88 + 0x6c) = 0;
    }
}

/* Whether the animation entry data_0213b578[tblIdx] of dog+0x74 has reached its end in direction dir. */
static BOOL AnimDone(u8 *dog, int tblIdx, s8 dir)
{
    u32 ea = *(u32 *)(dog + 0x74) + (u32)((s32)data_0213b578[tblIdx] * 0x24);
    u8 *e = (u8 *)ea;

    if (ea == 0) {
        return FALSE;
    }
    if (dir > 0) {
        return (*(s32 *)(e + 0x20) >> 12) >= *(s16 *)(e + 0x18) - 1;
    }
    return (*(s32 *)(e + 0x20) >> 12) <= 1;
}

/* Motion event (+0x166) helpers. */
static void EventStart(u8 *dog, u8 val)
{
    u32 a = *(u32 *)(dog + 0x74) + 0x48;

    U8(0x9d) = val;
    if (a != 0) {
        S16(0x150) = (s16)(*(s32 *)(a + 0x20) >> 12);
    }
    func_0203e458(*(u8 **)(data_0214f464 + 0x7420), (u32)(s32)S8(0x88));
}

static void EventForce(u8 *dog, s16 ev)
{
    s16 cur = S16(0x156);

    if (cur != S16(0x166) && cur != ev) {
        S16(0x14c) = cur;
        S16(0x156) = ev;
    }
}

static void EventFollow(u8 *dog)
{
    s16 cur = S16(0x156);

    if (cur != S16(0x166) && cur != S16(0x14c)) {
        S16(0x14c) = cur;
        S16(0x156) = S16(0x166);
    }
}

static void EventFinish(u8 *dog)
{
    if (S16(0x166) == S16(0x156) && AnimDone(dog, 4, S8(0xc0))) {
        U8(0xad) = 1;
    }
    if (U8(0xad) == 1) {
        S16(0x156) = S16(0x14c);
    }
}

/* Head bob offsets of the gait table (v = frame counter + 2 * dog index). */
static s32 Bob24(s32 v) { return Rnd(SIN(((v % 24) * 0xaaa) >> 4), 0x2000); }
static s32 Bob16(s32 v) { return Rnd(SIN((v % 16) * 256), 0x3000); }
static s32 Bob8(s32 v)  { return Rnd(SIN((v % 8) * 512), 0x4000); }
static s32 Bob20(s32 v) { return Rnd(SIN(((v % 20) * 0xccc) >> 4), 0x280a); }

static u8 *ResDesc(u32 r) { return data_0212b14c + r * 0x18; }

void func_01ffb34c(u8 *dog)
{
    s32 oldGait = S8(0x98);
    s32 oldHead = S8(0x93);
    s32 oldMouth = S8(0x99);
    s32 oldTail = S8(0x97);
    s16 oldEye[2], oldEar[2];
    u8 *anm74;
    u8 *tail;
    s32 bobY;
    s32 ofs[3];
    MtxFx43 prevMtx;
    s32 trk[3];
    s32 chain = 0;    /* stack slot reused across channels; uninitialised in the original */
    s16 i;
    DogFunc fn;

    for (i = 0; i < 2; i = (s16)(i + 1)) {
        oldEye[i] = S8(0x8f + i);
        oldEar[i] = S8(0x91 + i);
    }
    CopyVec(dog + 0x2ac, dog + 0x2a0);
    U16(0x12c) = *(u16 *)(TYPE() + 0x64e);
    U8(0xac) = 1;

    /* ---- pending action ---- */
    if (data_0213bba4[S16(0xfa)] != NULL) {
        s16 act = S16(0xf0);
        s16 mot = S16(0xea);
        BOOL run;

        if ((act >= 4 && act <= 8) || act == 0x57 || (mot >= 0xf && mot <= 0x4b) ||
            (mot >= 0x119 && mot <= 0x13c)) {
            run = FALSE;
        } else if ((mot >= 0x478 && mot <= 0x47e) || (mot >= 0x4c7 && mot <= 0x4ca) || act == 0x53 ||
                   act == 0x54) {
            run = FALSE;
        } else {
            run = TRUE;
        }
        if (!run) {
            S16(0xfa) = 0;
        } else {
            s32 me;

            func_02009230((u32)dog);
            data_0213bba4[S16(0xfa)](dog);
            func_020aad40(data_0214f460, S8(0x88));
            if (func_020bc130() != 0) {
                ReleaseIfAllFree();
            }
            me = S8(0x88);
            if ((s8)data_0214f464[0x16 + me] != -1) {
                s16 k;

                for (k = 0; k < 3; k = (s16)(k + 1)) {
                    u8 *g = data_0214f464;
                    s8 o = (s8)g[k + 0x16];

                    if (o > (s8)g[me + 0x16]) {
                        g[k + 0x16] = (u8)(o - 1);
                    }
                }
                data_0214f464[0x15] = (u8)((s8)data_0214f464[0x15] - 1);
                data_0214f464[me + 0x16] = 0xff;
            }
            if (S16(0xf0) > 0xc && S16(0xf0) < 0x6f) {
                U8(0xa8) = 0;
                S16(0x112) = 0;
                S16(0xf0) = 0x6f;
                U8(0xa5) = 0;
                U8(0xa3) = 0;
            }
        }
    }

    /* ---- movement ---- */
    *(u8 **)(dog + 0x94c) = func_020bcad8(S8(0x88));
    func_0203b794(dog);
    anm74 = *(u8 **)(dog + 0x74);
    tail = *(u8 **)(dog + 0x7c);
    func_0203bc54(dog);
    S32(0x264) = S32(0x264) + S32(0x25c);
    if ((S32(0x924) & 0x40000000) || U8(0xab) != 0) {
        S32(0x260) = Rnd(S32(0x260), S32(0x258));
        S32(0x268) = Rnd(S32(0x268), S32(0x258));
        if ((S32(0x260) > 0 ? S32(0x260) : -S32(0x260)) <= 2) {
            S32(0x260) = 0;
        }
        if ((S32(0x268) > 0 ? S32(0x268) : -S32(0x268)) <= 2) {
            S32(0x268) = 0;
        }
    }
    U8(0x9c) = 0;
    S16(0x14e) = (s16)(*(s32 *)(anm74 + 0x20) >> 12);
    data_02138a84[S16(0xea)](dog);
    func_01ff832c(S32(0x250), VEC(0x1a4), VEC(0x260), VEC(0x260));
    func_02067e68((s32 *)(dog + 0x2a0), (s32 *)(dog + 0x260), (s32 *)(dog + 0x2a0));
    if (S32(0x91c) > S32(0x920)) {
        S32(0x91c) -= 0xcd;
        if (S32(0x91c) < S32(0x920)) {
            S32(0x91c) = S32(0x920);
        }
    } else if (S32(0x91c) < S32(0x920)) {
        S32(0x91c) += 0xcd;
        if (S32(0x91c) > S32(0x920)) {
            S32(0x91c) = S32(0x920);
        }
    }

    /* ---- side (ear) states from the motion ---- */
    {
        BOOL busy = U16(0x100) > 10;
        s16 f = S16(0x14e);

        switch (S16(0x162)) {
        case 0x10:
            SideState(dog, 0, busy && S16(0x14e) < 0x1c, 6, 8);
            SideState(dog, 1, U16(0x100) > 10 && S16(0x14e) >= 0x1e, 6, 8);
            break;
        case 0x11:
            SideState(dog, 0, busy && S16(0x14e) < 0x12, 6, 8);
            SideState(dog, 1, U16(0x100) > 10 && S16(0x14e) >= 0x14, 6, 8);
            break;
        case 0x12:
            SideState(dog, 0, busy && f >= 0 && f < 6, 7, 9);
            f = S16(0x14e);
            SideState(dog, 1, U16(0x100) > 10 && f >= 0xf && f < 0x15, 7, 9);
            break;
        default:
            for (i = 0; i < 2; i = (s16)(i + 1)) {
                if (S8(0x91 + i) == 6) {
                    S8(0x91 + i) = 8;
                }
                if (S8(0x91 + i) == 7) {
                    S8(0x91 + i) = 9;
                }
            }
            break;
        }
    }
    if (S8(0x9c) == 0) {
        S32(0x818) = Rnd(S32(0x818), 0xcd0);
        S32(0x248) = Rnd(S32(0x248), 0xcd0);
    }

    /* ---- side and eye blend weights ---- */
    for (i = 0; i < 2; i = (s16)(i + 1)) {
        s32 st = S8(0x91 + i);
        const s32 *ent = &data_0213b90c[st * 3];

        if (!(st == 0 && oldEar[i] == 8) && st != oldEar[i]) {
            S32(0x440 + i * 4) = 0;
            S32(0x448 + i * 4) = 0;
        }
        Approach((s32 *)(dog + 0x440 + i * 4), ent[0], ent[1]);
        Approach((s32 *)(dog + 0x448 + i * 4), ent[0], ent[2]);
    }
    for (i = 0; i < 2; i = (s16)(i + 1)) {
        s32 st = S8(0x8f + i);

        if (st != oldEye[i]) {
            S32(0x438 + i * 4) = 0;
        }
        Approach((s32 *)(dog + 0x438 + i * 4), st == 0 ? 0x1000 : 0x400, 0x80);
    }

    /* ---- head state ---- */
    {
        s32 hs = S8(0x93);
        s32 maxA = data_0213be7c[hs * 4];
        s32 incA = data_0213be80[hs * 4];
        s32 maxB = data_0213be84[hs * 4];
        s32 incB = data_0213be88[hs * 4];

        if (data_0213b8a8[hs] != NULL) {
            data_0213b8a8[hs](dog);
        }
        if (S8(0x93) != oldHead) {
            S32(0x450) = 0;
            S32(0x454) = 0;
        }
        Approach((s32 *)(dog + 0x450), maxA, incA);
        Approach((s32 *)(dog + 0x454), maxB, incB);
    }
    func_0203679c(dog);

    /* ---- gait head bob ---- */
    ofs[0] = 0;
    ofs[1] = 0;
    ofs[2] = 0;
    bobY = 0;
    {
        s32 v = (s32)U16(0xfe) + S8(0x88) * 2;

        switch (S8(0x98)) {
        case 1:
            bobY = Bob24(v);
            break;
        case 2:
            bobY = Bob16(v);
            break;
        case 3:
            bobY = Bob8(v);
            break;
        case 4:
            ofs[0] = Rnd(SIN((v % 32) * 128), 0x1000);
            ofs[1] = ofs[0];
            ofs[2] = ofs[0];
            break;
        case 5:
            ofs[0] = 0x800;
            break;
        case 6:
            ofs[0] = -0x800;
            break;
        case 7:
            bobY = -0x1800;
            break;
        case 8:
            bobY = 0x1800;
            break;
        case 9: {
            s8 k = S8(0x8b);

            if (k == 2) {
                ofs[0] = 0x800;
            } else if (k == 4 || k == 5) {
                ofs[0] = -0x800;
            } else {
                s32 lim = data_0213b5a8[S8(0x8d)];
                s32 h = *(s16 *)(TYPE() + 0x1a);

                if (h > lim) {
                    bobY = Bob16(v);
                } else if (h > (lim >> 1)) {
                    bobY = Bob20(v);
                } else if (h > (lim >> 2)) {
                    bobY = Bob24(v);
                }
            }
            break;
        }
        default:
            break;
        }
    }
    if (S8(0x98) != oldGait || U8(0x9a) != U8(0x9b)) {
        S32(0x458) = 0;
    }
    {
        const u8 *row;

        if (S32(0x458) < 0x1000) {
            S32(0x458) += 0x40;
            S32(0x808) = S32(0x808) + Rnd(bobY - S32(0x808), S32(0x458));
            for (i = 0; i < 3; i = (s16)(i + 1)) {
                s32 *p = (s32 *)(dog + 0x80c + i * 4);

                row = data_0213c41c + (u32)*(u16 *)TYPE() * 100 + U8(0x9a) * 6;
                *p = *p + Rnd(ofs[i] + *(const s16 *)(row + 0x16 + i * 2) - *p, S32(0x458));
            }
        } else {
            S32(0x808) = bobY;
            for (i = 0; i < 3; i = (s16)(i + 1)) {
                row = data_0213c41c + (u32)*(u16 *)TYPE() * 100 + U8(0x9a) * 6;
                S32(0x80c + i * 4) = ofs[i] + *(const s16 *)(row + 0x16 + i * 2);
            }
        }
    }
    if (S8(0x99) != oldMouth) {
        S32(0x45c) = 0;
    }
    if (S32(0x45c) < 0x1000) {
        S32(0x45c) += 0x100;
    }

    /* ---- motion events ---- */
    switch (S16(0x166)) {
    case 1: {
        s16 n = (s16)(S16(0x13e) - 3);
        u8 *obj = NULL;
        u8 *ty = TYPE();
        s32 lim, scale;
        s16 cnt = 0;
        s32 ang = 0;

        if (n >= 0) {
            u8 *list = data_02159468;

            if (*(s16 *)(list + 0x20) > n) {
                obj = *(u8 **)(list + n * 4 + 4);
            }
        }
        lim = obj != NULL ? *(s32 *)(obj + 0x124) : 0x600;
        scale = Rnd(data_0213c470[(u32)*(u16 *)ty * 25], *(s32 *)(ty + 8));
        for (;;) {
            if (!(lim > Rnd(scale, SIN(ang >> 4)))) {
                break;
            }
            cnt = (s16)(cnt + 1);
            ang += 0x4bd;
            if (cnt >= 9) {
                break;
            }
        }
        *(s32 *)(*(u8 **)(dog + 0x74) + 0x48 + 0x20) = (s32)((u32)(s32)cnt << 12);
        U8(0x9d) = 0;
        S16(0x150) = 0;
        break;
    }
    case 0x11:
        EventStart(dog, 1);
        EventForce(dog, 0x13);
        break;
    case 0x12:
        EventStart(dog, 2);
        EventForce(dog, 0x13);
        break;
    case 0x13:
        EventStart(dog, 3);
        EventFollow(dog);
        EventFinish(dog);
        break;
    case 0x14:
        EventStart(dog, 4);
        EventForce(dog, 0x14);
        EventFollow(dog);
        EventFinish(dog);
        break;
    case 0x15:
        EventStart(dog, 5);
        EventForce(dog, 0x15);
        EventFollow(dog);
        EventFinish(dog);
        break;
    default:
        U8(0x9d) = 0;
        S16(0x150) = 0;
        break;
    }

    /* ---- expression ---- */
    if (U8(0xb3) != 0) {
        s8 t = data_0213c42d[(u32)*(u16 *)TYPE() * 100];

        if (t == 1) {
            S16(0x158) = 0x11;
        } else if (t == 2) {
            S16(0x158) = 0x12;
        } else if (t == 3) {
            S16(0x158) = 0x13;
        }
    } else if (U8(0xb4) != 0) {
        S16(0x158) = (s16)(data_0213c42d[(u32)*(u16 *)TYPE() * 100] + 0xc);
        if (S16(0x168) == S16(0x158) && AnimDone(dog, 6, S8(0xc1))) {
            S16(0x158) = 0;
            U8(0xb4) = 0;
        }
    }

    /* ---- tail ---- */
    if (S8(0x97) != oldTail) {
        s32 cur = S8(0x97);
        u16 a = data_0213b750[cur * 2];

        if (a != data_0213b750[oldTail * 2]) {
            func_0208e468(tail, (u8 *)func_0208b9e8(data_0213b6e4[a]));
        }
        cur = S8(0x97);
        tail[0] = (u8)data_0213b753[cur * 4];
        *(s32 *)(tail + 8) = (s32)((u32)(s32)data_0213b752[S8(0x97) * 4] << 12);
        U16(0x10e) = 0;
    }
    switch (S8(0x97)) {
    case 0:
    case 5:
        if (*(s32 *)(tail + 8) == 0 && U16(0x10e) > 0x3c) {
            u32 *seed = (u32 *)data_0214f464;
            u32 s = *seed * 0x10dcdu + 1u;
            s32 r = (s16)(s & 0xfffu);

            *seed = s;
            if (Rnd(r, data_0213b4fc[S8(0x8b)]) == 0) {
                U16(0x10e) = 0;
                *(s32 *)(tail + 0xc) = (s32)((u32)(s32)*(s16 *)(tail + 2) << 12);
                *(s32 *)(tail + 8) = 0x1000;
                tail[1] = 0;
                *(s32 *)(tail + 8) = 0x1000;
            }
        }
        break;
    case 3:
    case 4:
    case 6: {
        s32 stop = S8(0x97) == 4 ? 5 : S8(0x97) == 3 ? 7 : 0xe;

        if ((*(s32 *)(tail + 0xc) >> 12) == stop) {
            U16(0x10e) = 0;
            *(s32 *)(tail + 8) = 0;
        }
        break;
    }
    default:
        break;
    }

    /* ---- animation channels ---- */
    for (i = 0; i < 4; i = (s16)(i + 1)) {
        u8 *g = data_0214f464;
        u8 *res = *(u8 **)(g + S8(0x88) * 4 + 0x48);
        s8 st = S8(0xc2 + i);
        u32 arg = 0;          /* sp20: resource */
        u32 id = 0;           /* r8 */
        s32 frame = 0;        /* r11 */
        s32 spd = 0;          /* r7 */
        s32 mode = 0;         /* r6 */
        s32 start = 0;        /* sp24 */
        s32 len = 0;          /* r5 */
        s16 j;

        if (st == 0 && S16(0x15a + i * 2) != S16(0x152 + i * 2)) {
            s16 want = S16(0x152 + i * 2);
            u32 extra = 0;
            u8 *desc = NULL;
            u32 r;

            if (i == 0) {
                r = *(u16 *)(data_0213cde0 + want * 8);
            } else {
                r = *(u16 *)(data_0213b568[i] + want * 6);
            }
            if (r < NORES) {
                desc = ResDesc(r);
                if ((s8)data_0213c41c[(u32)*(u16 *)TYPE() * 100 + 0x10] == 1) {
                    extra = desc[5];
                    if (extra != 0) {
                        r = (u16)(r + extra);
                        desc = ResDesc(r);
                    }
                }
            }
            /* desc is NULL only for r > NORES (out of the table); the original then reads address 6. */
            if ((s8)g[0x10] != 0 && r != NORES && (desc == NULL || desc[6] == 0)) {
                goto play;
            }
            if (r == NORES) {
                S8(0xc2 + i) = 3;
            } else if (desc != NULL && desc[6] != 0) {
                char name[0x80];

                func_0205f050((u32)name, 0x80, (u32)data_0213e100, *(u32 *)desc);
                S32(0x17c + i * 4) = (s32)func_0208b9e8((u32)name);
                S8(0xc2 + i) = 2;
            } else {
                func_01ffe154(res, i, r);
                data_0214f464[0x10] = 1;
                S8(0xc2 + i) = 1;
            }
            S16(0x15a + i * 2) = S16(0x152 + i * 2);
            U16(0x16a + i * 2) = (u16)r;
            U8(0xca + i) = (u8)extra;
        }
    play:
        st = S8(0xc2 + i);
        if (st != 1 && st != 2 && st != 3) {
            continue;
        }
        if (st == 1 && !func_0208cac8(res, i)) {
            continue;
        }
        if (S8(0xba + i) != 0) {
            func_0208ca50(res, i);
        }
        if (st == 3) {
            U8(0xba + i) = 0;
            S32(0x17c + i * 4) = 0;
        } else if (st == 2) {
            U8(0xba + i) = 0;
        } else {
            U8(0xba + i) = 1;
            S32(0x17c + i * 4) = (s32)func_0208ca20(res, i);
            func_0208ca9c(res, i);
            data_0214f464[0x10] = 0;
        }
        U8(0xc2 + i) = 0;
        S16(0x162 + i * 2) = S16(0x15a + i * 2);
        U16(0x172 + i * 2) = U16(0x16a + i * 2);
        U8(0xce + i) = U8(0xca + i);

        if (i == 0) {
            const u8 *ent;
            s32 t;

            arg = (u32)S32(0x17c);
            id = U16(0x172);
            frame = 0;
            S8(0xbe) = (s8)data_0213cde0[S16(0x162) * 8 + 4];
            spd = S8(0xbe);
            U8(0xc6) = id < NORES ? ResDesc(id)[4] : 0;
            mode = S8(0xc6);
            ent = data_0213cde0 + S16(0x162) * 8;
            len = *(const s16 *)(ent + 2);
            start = *(const s8 *)(ent + 5);
            if (start == 2) {
                frame = -1;
            }
            U8(0x9b) = U8(0x9a);
            t = (s8)data_0213c41c[(u32)*(u16 *)TYPE() * 100 + 0x14];
            U8(0x9a) = id < NORES ? *(ResDesc(id) + t + 8) : 0;
            U16(0x100) = 0;
        } else {
            if (i == 2) {
                U8(0xad) = 0;
            }
            arg = (u32)S32(0x17c + i * 4);
            if (arg != 0) {
                const u8 *ent = data_0213b568[i] + S16(0x162 + i * 2) * 6;

                frame = -1;
                spd = *(const s8 *)(ent + 4);
                id = U16(0x172 + i * 2);
                mode = (s8)(id < NORES ? ResDesc(id)[4] : 0);
                len = *(const s16 *)(ent + 2);
                start = *(const s8 *)(ent + 5);
            } else {
                /* Follow the channel's parent chain to the first one with a resource. */
                s16 c = data_0213b4a8[data_0213b578[i * 2]];

                while (c != -1) {
                    s16 slot = data_0213b484[c];

                    if (S32(0x17c + slot * 4) != 0) {
                        chain = slot;
                        break;
                    }
                    c = data_0213b4a8[c];
                }
                arg = (u32)S32(0x17c + chain * 4);
                frame = *(s32 *)(*(u8 **)(dog + 0x74) + (s32)c * 0x24 + 0x20) >> 12;
                spd = S8(0xbe + chain);
                mode = S8(0xc6 + chain);
                id = U16(0x172 + chain * 2);
                if (chain == 0) {
                    len = *(const s16 *)(data_0213cde0 + S16(0x162) * 8 + 2);
                } else {
                    len = *(const s16 *)(data_0213b568[chain] + S16(0x162 + chain * 2) * 6 + 2);
                }
                start = -1;
            }
            U8(0xc6 + i) = (u8)mode;
            U8(0xbe + i) = (u8)spd;
        }
        if (U8(0xae) != 0) {
            U8(0xae) = 0;
            if (U16(0xfe) < 5) {
                len = 1;
            }
        }
        for (j = 0; j < 2; j = (s16)(j + 1)) {
            s16 a = data_0213b578[i * 2 + j];

            if (a >= 0) {
                func_01ffd8f8(dog, a, arg, id, frame, (s16)spd, mode, (s16)start, (s16)len);
            }
        }
    }
    if (S16(0x15a) == S16(0x152) && S8(0xc2) == 0) {
        S32(0x250) = S32(0x240);
        S32(0x254) = S32(0x244);
        S32(0x258) = S32(0x24c);
    }

    /* ---- controller hand-over ---- */
    if (S8(0x88) == (s8)data_0214f464[7]) {
        s16 act = S16(0xf0);
        s16 mot = S16(0xea);
        BOOL busy = (act >= 10 && act < 0x6f) || (act >= 4 && act <= 8) || (mot >= 0x37c && mot <= 0x3af) ||
                    (mot >= 0xec && mot <= 0x254);

        if (!busy) {
            s32 me;
            BOOL rel;

            if (func_020aaeb8(data_0214f460, (u32)(s32)S8(0x88)) != -1) {
                func_020aad40(data_0214f460, S8(0x88));
                if (func_020bc130() != 0) {
                    ReleaseIfAllFree();
                }
            }
            me = S8(0x88);
            rel = TRUE;
            if (me != func_020bc14c() && me != (s8)data_0214f464[7]) {
                rel = FALSE;
            }
            if (rel) {
                func_020bbe14(1, -1);
                *(u32 *)(func_020bc3c4() + 0x88 + 0x6c) = 0;
            }
            if (S8(0x88) == (s8)data_0214f464[7]) {
                u8 *g = data_0214f464;

                g[8] = 0xff;
                g[7] = g[8];
            }
        }
    }
    if (S8(0xb2) == 0) {
        U8(0xb1) = 0;
    } else {
        U8(0xb1) = U8(0xb2);
        U8(0xb2) = 0;
    }

    /* ---- counters and timers ---- */
    U16(0xfc) = (u16)(U16(0xfc) + 1);
    U16(0xfe) = (u16)(U16(0xfe) + 1);
    U16(0x100) = (u16)(U16(0x100) + 1);
    U16(0x10e) = (u16)(U16(0x10e) + 1);
    if (S8(0x88) == (s32)*(u16 *)(data_0214f464 + 0x38)) {
        func_0203b3fc(dog);
    }
    if (S8(0x88) == (s32)*(u16 *)(data_0214f464 + 0x3a)) {
        func_0203ad20(dog);
    }
    if (S8(0x88) == (s32)*(u16 *)(data_0214f464 + 0x3c)) {
        func_02039d08(dog);
    }
    if (U8(0xa4) == 0 && S16(0x116) > 0) {
        S16(0x116) = (s16)(S16(0x116) - 1);
        if (S16(0x116) == 0) {
            S16(0xf2) = 0;
            U8(0xa6) = 0;
        }
    }
    if (U8(0xa3) == 0 && S16(0x118) > 0) {
        S16(0x118) = (s16)(S16(0x118) - 1);
        if (S16(0x118) == 0) {
            S16(0xf8) = -1;
        }
    }
    if (S8(0x88) != (s8)data_0214f464[8]) {
        U8(0xb9) = 0;
    }
    if (S32(0x2a4) < 0) {
        S32(0x2a4) = 0;
    }

    /* ---- orientation basis (+0x18c side, +0x198 up, +0x1a4 forward) ---- */
    func_0206c8d4((u32 *)(dog + 0x3c), (u32 *)&prevMtx);
    switch (S8(0x9e)) {
    case 0:
        VEC(0x198)->x = 0;
        VEC(0x198)->y = 0x1000;
        VEC(0x198)->z = 0;
        func_02067d04(VEC(0x198), VEC(0x1a4), VEC(0x18c));
        func_01ff89d0(VEC(0x18c));
        func_02067d04(VEC(0x18c), VEC(0x198), VEC(0x1a4));
        break;
    case 3:
        CopyVec(dog + 0x198, dog + 0x390);
        func_02067d04(VEC(0x198), VEC(0x1a4), VEC(0x18c));
        func_01ff89d0(VEC(0x18c));
        func_01ff89d0(VEC(0x198));
        func_02067d04(VEC(0x18c), VEC(0x198), VEC(0x1a4));
        break;
    default:
        func_02067e34(VEC(0x390), VEC(0x2a0), VEC(0x198));
        func_02067d04(VEC(0x198), VEC(0x1a4), VEC(0x18c));
        func_01ff89d0(VEC(0x18c));
        func_01ff89d0(VEC(0x198));
        func_02067d04(VEC(0x18c), VEC(0x198), VEC(0x1a4));
        break;
    }
    CopyVec(dog + 0x30, dog + 0x2a0);

    /* ---- vertical bounce (types with flag +0x10 == 1) ---- */
    if (data_0213c42c[(u32)*(u16 *)TYPE() * 100] == 1) {
        u8 *ty = TYPE();

        if (U8(0xce) != 0) {
            if (S32(0x290) > 0) {
                if (S16(0x130) != S16(0x162)) {
                    u8 *a = *(u8 **)(dog + 0x74);

                    if (a != NULL && *(s16 *)(a + 6) != 0) {
                        S32(0x298) = (s32)(u32)func_0205e270(S32(0x290), *(s16 *)(a + 6));
                    } else {
                        S32(0x298) = 0;
                    }
                }
                S32(0x290) = S32(0x290) - S32(0x298);
                if (S32(0x290) < 0) {
                    S32(0x290) = 0;
                }
                S32(0x28c) = Rnd(S32(0x28c), S32(0x290));
            } else {
                S32(0x28c) = 0;
            }
            S32(0x294) = S32(0x290);
            S32(0x288) = S32(0x284);
        } else {
            if (S32(0x294) < 0x1000) {
                s32 base, cur, tgt;

                if (S16(0x130) != S16(0x162)) {
                    u8 *a = *(u8 **)(dog + 0x74);

                    if (a != NULL && *(s16 *)(a + 6) != 0) {
                        S32(0x298) = (s32)(u32)func_0205e270(0x1000 - S32(0x294), *(s16 *)(a + 6));
                    } else {
                        S32(0x298) = 0;
                    }
                }
                S32(0x294) = S32(0x294) + S32(0x298);
                if (S32(0x294) > 0x1000) {
                    S32(0x294) = 0x1000;
                }
                base = Rnd(S32(0x288), 0x1863);
                cur = base + Rnd(S32(0x284) - base, S32(0x294));
                tgt = Rnd(Rnd(cur, -0x570), *(s32 *)(ty + 8));
                S32(0x28c) = S32(0x28c) + Rnd(tgt - S32(0x28c), S32(0x294));
            } else {
                S32(0x28c) = Rnd(Rnd(S32(0x284), -0x570), *(s32 *)(ty + 8));
            }
            S32(0x290) = 0x1000;
        }
        S16(0x130) = S16(0x162);
        S32(0x34) = S32(0x34) + S32(0x28c);
    }

    /* ---- model matrices ---- */
    {
        int k;

        for (k = 0; k < 9; k++) {
            S32(0x3c + k * 4) = S32(0x18c + k * 4);
        }
    }
    if (S8(0x9e) != S8(0x9f)) {
        S32(0x460) = 0;
    }
    if (S32(0x460) < 0x1000) {
        S32(0x460) += 0x100;
        func_01ff86bc((const s32 *)&prevMtx, (const s32 *)(dog + 0x3c), (s32 *)(dog + 0x3c), S32(0x460));
    }
    S8(0x9f) = S8(0x9e);
    func_01ff810c((const MtxFx43 *)(dog + 0x3c), (const MtxFx43 *)(dog + 0x1b0), (MtxFx43 *)(dog + 0x1e0));
    S32(0x204) = S32(0x30);
    S32(0x208) = S32(0x34);
    S32(0x20c) = S32(0x38);
    func_02066bdc((u32 *)(dog + 0x3c), (u32 *)(dog + 0x210));
    S32(0x234) = -S32(0x30);
    S32(0x238) = -S32(0x34);
    S32(0x23c) = -S32(0x38);
    S32(0x928) = (S32(0x2d0) + S32(0x2a0)) >> 1;
    S32(0x92c) = S32(0x2a4) + S32(0x91c);
    S32(0x930) = (S32(0x2d8) + S32(0x2a8)) >> 1;
    func_02009698(dog);
    {
        u32 c = (u32)((*(s16 *)(TYPE() + 0x26) >> 9) + 0xb);

        func_0208f820(dog, (u16)(c | (c << 5) | (c << 10)));
    }
    if (dog != NULL) {
        func_0208f7e4(dog, (u16)(data_027c0bbc[0] | (data_027c0bbc[1] << 5) | (data_027c0bbc[2] << 10)));
    }
    func_0208ec84((u8 *)func_020bc3c4(), dog + 0x2a0, (u32 *)(dog + 0x950));
    for (i = 0; i < 3; i = (s16)(i + 1)) {
        trk[i] = *(s32 *)(*(u8 **)(dog + 0x74) + data_0213b45c[i] * 0x24 + 0x20);
    }
    func_0209c33c(dog + 0x95c, (s32 *)(dog + 0x950), trk, (u16)S8(0x8d));
    func_01ffe474(dog);
}
