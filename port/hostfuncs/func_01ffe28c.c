#include "ctx.h"
/* Walks the circular node list at obj+0x10 (next at +0xc) and, for each node without bit 0 of byte +1 set,
 * calls its two callbacks at +0x14 and +0x18 (each with the node, skipped when NULL). */

typedef void (*NodeFunc)(u8 *node);

void func_01ffe28c(u8 *obj)
{
    u8 *head = *(u8 **)(obj + 0x10);
    u8 *n;

    if (head == NULL) {
        return;
    }
    n = head;
    do {
        if ((n[1] & 1) == 0) {
            NodeFunc f = *(NodeFunc *)(n + 0x14);
            if (f != NULL) {
                f(n);
            }
            f = *(NodeFunc *)(n + 0x18);
            if (f != NULL) {
                f(n);
            }
        }
        n = *(u8 **)(n + 0xc);
    } while (n != head);
}
