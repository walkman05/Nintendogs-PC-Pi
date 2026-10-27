#include "ctx.h"
/* NNSi_G3dFuncSbc_NODEDESC (game variant): SBC NODEDESC command handler. Restores the parent matrix, computes the
 * joint result (recorded, animated through funcBlendJnt at renderObj+0x14, or from the node's SRT data), sends it
 * through funcJntScale/funcJntMtxCalc, runs the game's per-node matrix callback (table at
 * (*data_027c0360)->0x1c->0x5c, entry +8) on the current matrix, stores the matrix and advances rs->c.
 * rs: +0 c (SBC pointer), +4 renderObj, +8 flag, +0xb4 pJntAnmResult, +0xd4 nodeDesc, +0xe8/+0xec callbacks. */

extern void HostIoWrite32(u32 addr, u32 value);

extern u8 *data_027c0360;
extern u8 data_0212f52c[];   /* pivot index tables (one byte per column, stride 4) */
extern u8 data_0212f52d[];
extern u8 data_0212f52e[];
extern u8 data_0212f52f[];
extern u32 data_027c0364[];  /* 0x48-byte geometry command block sent after the callback */
extern u32 data_027c0370[];  /* 4x3 matrix inside that block */

extern void func_020776d4(u32 param_1, u32 *param_2, s32 param_3);  /* geometry buffer: command + params */
extern void func_0206cae8(u32 *m);                                   /* zero 3x3 */
extern void func_020778d0(void);                                     /* flush geometry buffer */
extern u32 func_0206a33c(u32 *p);                                    /* read clip/current matrix, nonzero = busy */
extern void func_020673d0(u32 *param_1, u32 *param_2);              /* 4x4 -> 4x3 copy */
extern void func_0206c81c(const void *src, void *dst, u32 size);     /* MIi_CpuSend32 */

typedef BOOL (*BlendJntFunc)(u8 *result, u8 *anm, u32 nodeId);
typedef void (*JntScaleFunc)(u8 *result, const u8 *p, const u8 *c, u32 flag);
typedef void (*JntMtxFunc)(u8 *result);
typedef void (*NodeMtxFunc)(u8 *rs, u32 *mtx);

#define RS_C(rs)    (*(u8 **)(rs))
#define RS_FLAG(rs) (*(u32 *)((rs) + 8))
#define RES(r, o)   (*(s32 *)((r) + (o)))

static u8 *NodeData(u8 *rs, u32 nodeId)
{
    u8 *nd = *(u8 **)(rs + 0xd4);
    u8 *dict = nd + *(u16 *)(nd + 6);
    u32 ofs = *(u32 *)(dict + (u32)*(u16 *)dict * nodeId + 4);

    return nd + ofs;
}

void func_01ffe9ac(u8 *rs, u32 opt)
{
    u32 res[0x58 / 4];
    u32 mtx44[16];
    u32 arg;
    u32 cmdLen = 4;
    u8 *r;
    u32 nodeId;
    BOOL skip;
    u8 *ent;

    if (RS_FLAG(rs) & 0x400) {
        if (opt == 0x40 || opt == 0x60) {
            cmdLen++;
        }
        if (opt == 0x20 || opt == 0x60) {
            cmdLen++;
            if ((RS_FLAG(rs) & 0x100) == 0) {
                arg = RS_C(rs)[4];
                func_020776d4(0x14, &arg, 1);            /* MTX_RESTORE */
            }
        }
        RS_C(rs) += cmdLen;
        return;
    }
    if (opt == 0x40 || opt == 0x60) {
        cmdLen++;
        arg = (opt == 0x40) ? RS_C(rs)[4] : RS_C(rs)[5];
        if ((RS_FLAG(rs) & 0x100) == 0) {
            func_020776d4(0x14, &arg, 1);                /* MTX_RESTORE */
        }
    }

    r = (u8 *)res;
    *(u8 **)(rs + 0xb4) = r;
    nodeId = RS_C(rs)[1];
    {
        u8 *rec = *(u8 **)(*(u8 **)(rs + 4) + 0x34);

        if (rec != NULL) {
            r = rec + nodeId * 0x58;
            skip = (RS_FLAG(rs) & 0x80) == 0;
        } else {
            skip = FALSE;
        }
    }
    if (!skip) {
        u8 *ro = *(u8 **)(rs + 4);
        u8 *anm = *(u8 **)(ro + 0x10);

        RES(r, 0) = 0;
        if (anm != NULL && (*(BlendJntFunc *)(ro + 0x14))(r, anm, nodeId)) {
            u8 *nd = NodeData(rs, nodeId);
            const u8 *p = nd + 4;

            if (nodeId != 0 && (*(u16 *)(data_027c0360 + 0x20) & 4)) {
                if (*(u16 *)nd & 1) {
                    RES(r, 0) |= 4;
                } else {
                    RES(r, 0x4c) = *(const s32 *)p;
                    RES(r, 0x50) = *(const s32 *)(p + 4);
                    RES(r, 0x54) = *(const s32 *)(p + 8);
                }
            }
        } else {
            u8 *nd = NodeData(rs, nodeId);
            const u8 *p = nd + 4;
            u16 flag = *(u16 *)nd;

            if (flag & 1) {
                RES(r, 0) |= 4;
            } else {
                RES(r, 0x4c) = *(const s32 *)p;
                RES(r, 0x50) = *(const s32 *)(p + 4);
                RES(r, 0x54) = *(const s32 *)(p + 8);
                p += 12;
            }
            flag = *(u16 *)nd;
            if (flag & 2) {
                RES(r, 0) |= 2;
            } else if (flag & 8) {
                u32 idx = (flag & 0xf0u) >> 4;
                s32 b = *(const s16 *)(p + 2);
                s32 a = *(const s16 *)p;

                func_0206cae8((u32 *)(r + 0x28));
                p += 4;
                RES(r, 0x28 + idx * 4) = (*(u16 *)nd & 0x100) ? -0x1000 : 0x1000;
                RES(r, 0x28 + (u32)data_0212f52c[idx * 4] * 4) = a;
                RES(r, 0x28 + (u32)data_0212f52d[idx * 4] * 4) = b;
                if (*(u16 *)nd & 0x200) {
                    b = -b;
                }
                RES(r, 0x28 + (u32)data_0212f52e[idx * 4] * 4) = b;
                if (*(u16 *)nd & 0x400) {
                    a = -a;
                }
                RES(r, 0x28 + (u32)data_0212f52f[idx * 4] * 4) = a;
            } else {
                int i;

                RES(r, 0x28) = *(const s16 *)(nd + 2);
                for (i = 0; i < 8; i++) {
                    RES(r, 0x2c + i * 4) = *(const s16 *)(p + i * 2);
                }
                p += 16;
            }
            (*(JntScaleFunc *)(rs + 0xe8))(r, p, RS_C(rs), *(u16 *)nd);
        }
    }

    *(u8 **)(rs + 0xb4) = r;
    if ((RS_FLAG(rs) & 0x100) == 0) {
        (*(JntMtxFunc *)(rs + 0xec))(*(u8 **)(rs + 0xb4));
    }
    *(u8 **)(rs + 0xb4) = NULL;

    ent = *(u8 **)(*(u8 **)(data_027c0360 + 0x1c) + 0x5c) + (s32)(s16)RS_C(rs)[1] * 12;
    if (*(NodeMtxFunc *)(ent + 8) != NULL) {
        func_020778d0();
        HostIoWrite32(0x04000400u, 0x151110u);   /* GXFIFO: MTX_MODE, MTX_PUSH, MTX_IDENTITY packed */
        HostIoWrite32(0x04000400u, 0);           /* MTX_MODE(projection) */
        while (func_0206a33c(mtx44) != 0) {
        }
        func_020673d0(mtx44, data_027c0370);
        (*(NodeMtxFunc *)(ent + 8))(rs, data_027c0370);
        func_0206c81c(data_027c0364, (void *)0x04000400u, 0x48);
    }

    if (opt == 0x20 || opt == 0x60) {
        cmdLen++;
        if ((RS_FLAG(rs) & 0x100) == 0) {
            arg = RS_C(rs)[4];
            func_020776d4(0x13, &arg, 1);                /* MTX_STORE */
        }
    }
    RS_C(rs) += cmdLen;
}
