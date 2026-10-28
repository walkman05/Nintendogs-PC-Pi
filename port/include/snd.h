/* ARM7 sound driver stand-in (port/src/snd.c): the NitroSDK sound driver's behaviour, answering the ARM9's
 * sound commands (PXI channel 7) and producing the mixed output of the 16 DS sound channels.
 * Everything runs on the game thread; DS memory (sequences, banks, wave archives, shared work) is read directly
 * at its DS address. */
#ifndef SND_H
#define SND_H
#include "ctx.h"

#define SND_RATE 32768  /* output sample rate (Hz), stereo */

void snd_init(void);
void snd_set_shared_work(u32 addr);                        /* SNDSharedWork address (command 29) */
void snd_command(u32 id, u32 a0, u32 a1, u32 a2, u32 a3);  /* one SND command (all except 29) */
/* produces `frames` stereo frames (interleaved L,R) at SND_RATE, running the driver's 5.2 ms frames in between */
void snd_render(s16 *out, int frames);

#endif
