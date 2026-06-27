#ifndef SOUND_TABLE_H
#define SOUND_TABLE_H

#include "sound.h"

#define SNDCMD_OP_END      0x0000
#define SNDCMD_OP_SET_CH   0x0001
#define SNDCMD_OP_ARP2     0x0002
#define SNDCMD_OP_ARP3     0x0003
#define SNDCMD_OP_PITCH    0x0004
#define SNDCMD_OP_PLAY     0x0005
#define SNDCMD_OP_PLAY_SWP 0x0006
#define SNDCMD_OP_VIBRATO  0x0007
#define SNDCMD_OP_PRIORITY 0x0008
#define SNDCMD_OP_MASK     0x000F

// 0:  C2  (MIDI 36,  65.40639 Hz)
// 33: A4  (MIDI 69,  440.0000 HZ)
// 84: C9
enum
{
    SND_KEY_C,
    SND_KEY_Db,
    SND_KEY_D,
    SND_KEY_Eb,
    SND_KEY_E,
    SND_KEY_F,
    SND_KEY_Gb,
    SND_KEY_G,
    SND_KEY_Ab,
    SND_KEY_A,
    SND_KEY_Bb,
    SND_KEY_B
};

extern const snd_cmd *snd_sounds[SND_SOUND_COUNT];

void snd_init_table(void);

#endif