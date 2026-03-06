#ifndef SOUND_H
#define SOUND_H

#include <tonc_types.h>
#include "gba_util.h"

#define SND_SLOT_COUNT     8
#define SND_SOUND_COUNT    12

typedef u16 snd_cmd;

// 16 bits
// first four bits - opcode
//
// 0000------------
//  OP0: End
//
// 0001xxyy--------
//  OP1: Set Channel
//  x - Channel (SQR1, SQR2, WAVE, NOISE)
//  y - Channel Config (SQR_DUTY2, SQR_DUTY8, SQR_DUTY4, WAVE_TRIANGLE)
//
// 0010xxxx--------
//  OP2: Arp2
//  x - Semitone Offset
//
// 0011xxxxyyyy----
//  OP3: Arp3
//  x - Semitone Offset 1
//  y - Semitone Offset 2
//
// 0100xxxxxxxxz---
//  OP4: Set Pitch Register
//  x - Note #
//  z - Pitch Register # (0 or 1)
//
// 0101xxxxxxaaabbb
//  OP5: Play Note (pitch register 0)
//  x - Length in ticks
//  a - Start volume
//  b - End volume
//
// 0110xxxxxxaaabbb
//  OP6: Play Note + pitch sweep (between the pitch registers)
//  x - Length in ticks
//  a - Start volume
//  b - End volume
//
// 0111xxxxxxyyyyyy
//  OP7: Vibrato
//  x - Speed
//  y - Strength

// 1000xxxxxxxx----
//  OP8: Set Priority
//  x - priority

#define SND_SLOT_FLAG_ACTIVE  1
#define SND_SLOT_FLAG_ARP2    2
#define SND_SLOT_FLAG_ARP3    4
#define SND_SLOT_FLAG_SWEEP   8
#define SND_SLOT_FLAG_VIBRATO 16

typedef struct snd_slot
{
    u8 flags;

    u8 channel;
    u8 channel_config;
    u8 priority;

    u8 arp_offset0;
    u8 arp_offset1;

    u8 vib_speed;
    u8 vib_strength;

    u8 wait;
    u8 pitch_reg[2];

    FIXED pitch_increment; // per tick
    FIXED vol_increment;  // per tick
    FIXED pitch;
    FIXED vol;
    const snd_cmd *ip;
} snd_slot_s;

typedef enum snd_id
{
    SND_ID_PLAYER_JUMP,
    SND_ID_PLAYER_SHOOT,
    SND_ID_PLAYER_SPIT,
    SND_ID_PLATFORM_PLACE,
    SND_ID_PLAYER_DIE,
    SND_ID_CHECKPOINT,
    SND_ID_SPRING,
    SND_ID_ENEMY_SPIT,
    SND_ID_ENEMY_DIE,
    SND_ID_MENU_MOVE,
    SND_ID_MENU_SELECT,
} snd_id_e;

extern snd_slot_s snd_slots[SND_SLOT_COUNT];
extern const snd_cmd *snd_sounds[SND_SOUND_COUNT];

void snd_init(void);
void snd_frame(void);
void snd_play(snd_id_e id);
ARM_FUNC void snd_irq_hblank(void);

//
// 0110xxxxxx------
//  OP6: Play Note (pitch sweep)
//  x - Length in ticks

// player jump
// SNDCMD_CH_SQR1(SNDCMD_DUTY_8)
// SNDCMD_PITCH_ENV(C,5, Eb,6, 2)

// player death sound
// SNDCMD_CH_WAVE(SNDCMD_WAVE_TRIANGLE)
// SNDCMD_ARP2(10)
// SNDCMD_PITCH_ENV(Eb,6, C,5, 8)
// SNDCMD_PITCH_ENV(Eb,3, G,2, 3)

// enemy death sound
// SNDCMD_CH_SQR2(SNDCMD_DUTY_SQR)
// SNDCMD_PITCH_ENV(Gb,3, Eb,2, 2)
// SNDCMD_PITCH_ENV(Eb,3, G,2, 3)

#endif