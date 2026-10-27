#pragma avoid_byte strb
#include "ctx.h"

typedef struct {
    u8 pad0[4];
    u16 map[32 * 32];
    u8 pad804[0xc04 - 0x804];
    u8 cell[16][16];
    s32 w;
    s32 h;
    u8 colHint[8][16];
    u8 rowHint[16][8];
} Picross;

extern Picross *data_ov016_0215d1c0;

void func_ov016_0215c000(void)
{
    s32 j1;
    s32 at3;
    s32 j0;
    s32 i1;
    s32 hw2;
    s32 i0;
    s32 at2;
    s32 i3;
    s32 n2;
    s32 hw;
    s32 hh;
    s32 k3;
    s32 n3;
    s32 run2;
    s32 run3;

    hw = (data_ov016_0215d1c0->w + 1) / 2;
    hh = (data_ov016_0215d1c0->h + 1) / 2;
    hw2 = (data_ov016_0215d1c0->w + 1) / 2;
    for (i0 = 0; i0 < hh; i0++) {
        for (j0 = 0; j0 < data_ov016_0215d1c0->w; j0++) {
            data_ov016_0215d1c0->colHint[i0][j0] = 0;
        }
    }
    for (i1 = 0; i1 < data_ov016_0215d1c0->h; i1++) {
        for (j1 = 0; j1 < hw2; j1++) {
            data_ov016_0215d1c0->rowHint[i1][j1] = 0;
        }
    }
    for (j0 = 0; j0 < data_ov016_0215d1c0->w; j0++) {
        run2 = (n2 = 0);
        at2 = hh - 1;
        for (i0 = data_ov016_0215d1c0->h - 1; i0 >= 0; i0--) {
            if (data_ov016_0215d1c0->cell[i0][j0] == 0 && run2 != 0) {
                data_ov016_0215d1c0->colHint[at2][j0] = n2;
                n2 = 0;
                run2 = 0;
                at2--;
            }
            if (data_ov016_0215d1c0->cell[i0][j0] == 1) {
                n2++;
                run2 = 1;
                if (i0 == 0) {
                    data_ov016_0215d1c0->colHint[at2][j0] = n2;
                }
            }
        }
    }
    for (i3 = 0; i3 < data_ov016_0215d1c0->h; i3++) {
        n3 = 0;
        run3 = 0;
        at3 = hw - 1;
        for (k3 = data_ov016_0215d1c0->w - 1; k3 >= 0; k3--) {
            if (data_ov016_0215d1c0->cell[i3][k3] == 0 && run3 != 0) {
                data_ov016_0215d1c0->rowHint[i3][at3] = n3;
                n3 = 0;
                run3 = 0;
                at3--;
            }
            if (data_ov016_0215d1c0->cell[i3][k3] == 1) {
                run3 = 1;
                n3++;
                if (k3 == 0) {
                    data_ov016_0215d1c0->rowHint[i3][at3] = n3;
                }
            }
        }
    }
    for (i0 = 0; i0 < hh; i0++) {
        for (j0 = 0; j0 < data_ov016_0215d1c0->w; j0++) {
            data_ov016_0215d1c0->map[(i0 - hh + 8) * 32 + j0 + 8] = data_ov016_0215d1c0->colHint[i0][j0] + 8;
        }
    }
    for (i0 = 0; i0 < data_ov016_0215d1c0->h; i0++) {
        for (j0 = 0; j0 < hw; j0++) {
            data_ov016_0215d1c0->map[(i0 + 8) * 32 + j0 - hw + 8] = data_ov016_0215d1c0->rowHint[i0][j0] + 8;
        }
    }
}
