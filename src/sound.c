#include <stdlib.h>
#include <tonc_memmap.h>
#include <tonc_memdef.h>
#include <tonc_math.h>
#include <pitchlut_bin.h>
#include "sound.h"
#include "log.h"
#include "tonc_core.h"
#include "tonc_types.h"
#include "gba_util.h"

#define SND_TICK_LENGTH    8
#define TICKS_PER_FRAME    8
#define ARPEGGIO_TICK      (2 * TICKS_PER_FRAME)

#define SNDCMD_END 0x0000

#define SNDCMD_CH_SQR1  0
#define SNDCMD_CH_SQR2  1
#define SNDCMD_CH_WAVE  2
#define SNDCMD_CH_NOISE 3

#define SNDCMD_CH_SQR_DUTY2     0
#define SNDCMD_CH_SQR_DUTY4     1
#define SNDCMD_CH_SQR_DUTY8     2
#define SNDCMD_CH_WAVE_TRIANGLE 0

#define SNDCMD_PRIO_PLAYER  1
#define SNDCMD_PRIO_DEFAULT 0

#define SNDCMD_SET_CH(idx, mode) (0x0001 | ((idx) << 4) | ((mode) << 6))
#define SNDCMD_ARP2(ofs) (0x0002 | ((ofs) << 4))
#define SNDCMD_ARP3(ofs) (0x0003 | ((ofs) << 4))
#define SNDCMD_PITCH(i, key) (0x0004 | ((key) << 4) | ((i) << 12))
#define SNDCMD_PLAY_ENV(len, start, end) (0x0005 | ((len) << 4) | ((start) << 10) | ((end) << 13))
#define SNDCMD_PLAY_ENV_SWP(len, start, end) (0x0006 | ((len) << 4) | ((start) << 10) | ((end) << 13))
#define SNDCMD_PLAY(len) SNDCMD_PLAY_ENV(len, 7, 7)
#define SNDCMD_PLAY_SWP(len) SNDCMD_PLAY_ENV_SWP(len, 7, 7)
#define SNDCMD_VIBRATO(spd, pow) (0x0007 | ((spd) << 4) | ((pow) << 10))
#define SNDCMD_PRIO(prio) (0x0008 | ((prio) << 4))

#define SNDCMD_KEY(key, octave) (SND_KEY_##key + ((octave) - 2) * 12)

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

static const snd_cmd sound_player_jump[] = {
    SNDCMD_PRIO(SNDCMD_PRIO_PLAYER),
    SNDCMD_SET_CH(SNDCMD_CH_SQR1, SNDCMD_CH_SQR_DUTY8),
    SNDCMD_PITCH(0, SNDCMD_KEY(C, 5)),
    SNDCMD_PITCH(1, SNDCMD_KEY(Eb, 6)),
    SNDCMD_PLAY_SWP(6),
    SNDCMD_END,
};

static const snd_cmd sound_player_shoot[] = {
    SNDCMD_PRIO(SNDCMD_PRIO_PLAYER),
    SNDCMD_SET_CH(SNDCMD_CH_SQR1, SNDCMD_CH_SQR_DUTY2),
    SNDCMD_PITCH(0, SNDCMD_KEY(C, 7)),
    SNDCMD_PITCH(1, SNDCMD_KEY(Eb, 4)),
    SNDCMD_PLAY_SWP(3),
    SNDCMD_END,
};

static const snd_cmd sound_player_spit[] = {
    SNDCMD_PRIO(SNDCMD_PRIO_PLAYER),
    SNDCMD_SET_CH(SNDCMD_CH_SQR1, SNDCMD_CH_SQR_DUTY2),
    SNDCMD_PITCH(0, SNDCMD_KEY(C, 5)),
    SNDCMD_PITCH(1, SNDCMD_KEY(C, 4)),
    SNDCMD_PLAY_SWP(3),
    SNDCMD_PITCH(1, SNDCMD_KEY(D, 7)),
    SNDCMD_PLAY_SWP(3),
    SNDCMD_END,
};

static const snd_cmd sound_checkpoint[] = {
    SNDCMD_PRIO(SNDCMD_PRIO_PLAYER),
    SNDCMD_SET_CH(SNDCMD_CH_SQR1, SNDCMD_CH_SQR_DUTY2),
    SNDCMD_PITCH(0, SNDCMD_KEY(C, 4)),
    SNDCMD_PITCH(1, SNDCMD_KEY(A, 5)),
    SNDCMD_ARP2(4),
    SNDCMD_PLAY_SWP(8),
    SNDCMD_END,
};

snd_slot_s snd_slots[SND_SLOT_COUNT];
const snd_cmd *snd_sounds[SND_SOUND_COUNT];

// static vu16 *const cnt_regs[] =
//     { &REG_SND1CNT, &REG_SND2CNT, &REG_SND3CNT, &REG_SND4CNT };
// #define REG_SNDXCTL(n) (*cnt_regs[n])
// #define REG_SNDXFREQ(n) (*(vu16 *)(0x04000000 + 0x0064 + ((n) << 3)))

static u16 reg_ctl_vals[4][TICKS_PER_FRAME];
static u16 reg_freq_vals[4][TICKS_PER_FRAME];
static u16 reg_dmgctl_vals[TICKS_PER_FRAME];
static uint frame_tick_idx = 0;
static uint scanline_wait = 0;

#define SCANLINE_WAIT_RESET (228 / TICKS_PER_FRAME)

#define pitch_lut ((const FIXED *)pitchlut_bin)

void snd_init(void)
{
    snd_sounds[SND_ID_PLAYER_JUMP]    = sound_player_jump;
    snd_sounds[SND_ID_PLAYER_SHOOT]   = sound_player_shoot;
    snd_sounds[SND_ID_PLAYER_SPIT]    = sound_player_spit;
    snd_sounds[SND_ID_PLATFORM_PLACE] = NULL;
    snd_sounds[SND_ID_PLAYER_DIE]     = NULL;
    snd_sounds[SND_ID_CHECKPOINT]     = sound_checkpoint;
    snd_sounds[SND_ID_SPRING]         = NULL;
    snd_sounds[SND_ID_ENEMY_SPIT]     = NULL;
    snd_sounds[SND_ID_ENEMY_DIE]      = NULL;
    snd_sounds[SND_ID_MENU_MOVE]      = NULL;
    snd_sounds[SND_ID_MENU_SELECT]    = NULL;

    // turn sound on
    REG_SNDSTAT = SSTAT_ENABLE;
    REG_SNDDMGCNT = SDMG_BUILD_LR(SDMG_SQR1, 7);
    REG_SNDDSCNT |= SDS_DMG100;

    // no sweep
    REG_SND1SWEEP = SSW_OFF;
}

static bool proc_snd_slot(snd_slot_s *slot)
{
    if (slot->wait != 0)
    {
        slot->pitch += slot->pitch_increment;
        slot->vol += slot->vol_increment;

        if (slot->flags & SND_SLOT_FLAG_ARP2)
        {
            if (++slot->arp_index == (2 * ARPEGGIO_TICK))
                slot->arp_index = 0;

            FIXED pitches[2];
            pitches[0] = slot->pitch;
            pitches[1] = slot->pitch + int2fx(slot->arp_offset0);
            slot->final_pitch = pitches[(uint)slot->arp_index / ARPEGGIO_TICK];
        }
        else if (slot->flags & SND_SLOT_FLAG_ARP3)
        {
            if (++slot->arp_index == (3 * ARPEGGIO_TICK))
                slot->arp_index = 0;

            FIXED pitches[3];
            pitches[0] = slot->pitch;
            pitches[1] = slot->pitch + int2fx(slot->arp_offset0);
            pitches[2] = slot->pitch + int2fx(slot->arp_offset1);
            slot->final_pitch = pitches[(uint)slot->arp_index / ARPEGGIO_TICK];
        }
        else
        {
            slot->final_pitch = slot->pitch;
        }

        if (--slot->wait == 0)
        {
            slot->flags &= ~(SND_SLOT_FLAG_ARP2 | SND_SLOT_FLAG_ARP3 |
                             SND_SLOT_FLAG_SWEEP | SND_SLOT_FLAG_VIBRATO);
            
            u8 tmp;
            SWAP3(slot->pitch_reg[0], slot->pitch_reg[1], tmp);
        }

        return true;
    }

    while (true)
    {
        snd_cmd instr = *(slot->ip++);
        uint opcode = instr & SNDCMD_OP_MASK;
        switch (opcode)
        {
        case SNDCMD_OP_END:
            LOG_DBG("END");
            return false;
        
        case SNDCMD_OP_SET_CH:
            LOG_DBG("SET_CH");
            slot->channel = (instr >> 4) & 3;
            slot->channel_config = (instr >> 6) & 3;
            break;
        
        case SNDCMD_OP_ARP2:
            slot->arp_offset0 = (instr >> 4) & 0xF;
            slot->flags |= SND_SLOT_FLAG_ARP2;
            break;
        
        case SNDCMD_OP_ARP3:
            LOG_DBG("ARP3");
            slot->arp_offset0 = (instr >> 4) & 0xF;
            slot->arp_offset1 = (instr >> 8) & 0xF;
            slot->flags |= SND_SLOT_FLAG_ARP3;
            break;
        
        case SNDCMD_OP_PITCH:
            LOG_DBG("PITCH: %i", (instr >> 4) & 0xFF);
            slot->pitch_reg[(instr >> 12) & 1] = (instr >> 4) & 0xFF;
            break;
        
        case SNDCMD_OP_PLAY:
        case SNDCMD_OP_PLAY_SWP:
        {
            uint len = (instr >> 4) & 0x3F;
            uint vol_start = (instr >> 10) & 7;
            uint vol_end = (instr >> 13) & 7;
            int denom = len * SND_TICK_LENGTH + 1;
            
            slot->pitch = int2fx(slot->pitch_reg[0]);
            slot->vol_increment = int2fx(vol_end - vol_start) / denom;
            slot->vol = vol_start;
            slot->wait = len * SND_TICK_LENGTH;
            slot->arp_index = 0;

            if (opcode == SNDCMD_OP_PLAY_SWP)
            {
                LOG_DBG("PLAY_SWP");
                slot->flags |= SND_SLOT_FLAG_SWEEP;
                slot->pitch_increment = int2fx(slot->pitch_reg[1] - slot->pitch_reg[0])
                                            / denom;
            }
            else
            {
                slot->pitch_reg[1] = slot->pitch_reg[0];
                LOG_DBG("PLAY");
            }
            
            goto yield;
        }
        
        case SNDCMD_OP_VIBRATO:
            LOG_DBG("VIBRATO");
            slot->vib_speed = (instr >> 4) & 0x3F;
            slot->vib_strength = (instr >> 10) & 0x3F;
            slot->flags |= SND_SLOT_FLAG_VIBRATO;
            break;
        
        case SNDCMD_OP_PRIORITY:
            LOG_DBG("PRIORITY");
            slot->priority = (instr >> 4) & 0xFF;
            break;
        }
    }

    yield:;
    return true;
}

static void snd_tick(uint tick_idx)
{
    snd_slot_s *channel_slot[4] = { NULL, NULL, NULL, NULL };

    for (int i = 0; i < SND_SLOT_COUNT; ++i)
    {
        snd_slot_s *slot = snd_slots + i;
        if (!(slot->flags & SND_SLOT_FLAG_ACTIVE)) continue;

        if (!proc_snd_slot(slot))
        {
            slot->flags = 0;
            continue;
        }

        uint ch = slot->channel;

        if (channel_slot[ch] && slot->priority < channel_slot[ch]->priority)
        {
            continue;
        }

        channel_slot[ch] = slot;
    }

    u16 *reg_dmgctl = &reg_dmgctl_vals[tick_idx];
    *reg_dmgctl = SDMG_RVOL(3) | SDMG_LVOL(3);

    for (uint ch = 0; ch < 4; ++ch)
    {
        const snd_slot_s *const slot = channel_slot[ch];
        u16 *reg_ctl = &reg_ctl_vals[ch][tick_idx];
        u16 *reg_freq = &reg_freq_vals[ch][tick_idx];

        if (!slot)
        {
            *reg_ctl = SSQR_IVOL(0);
            continue;
        }

        uint pitch = fx2int(slot->final_pitch);
        FIXED pitch_frac = fx2ufrac(slot->final_pitch);

        // uint rate = fx2int(pitch_lut[pitch]);
        // LOG_DBG("pitch: %i, rate: %i", pitch, rate & 0x7FF);

        FIXED rate0 = pitch_lut[pitch];
        FIXED rate1 = pitch_lut[pitch + 1];
        FIXED rate = fxmul((rate1 - rate0), pitch_frac) + rate0;

        static const uint duty_flags[] =
            { SSQR_DUTY1_2, SSQR_DUTY1_4, SSQR_DUTY1_8 };
        
        *reg_dmgctl |= (1 << (ch + 0x8)) | (1 << (ch + 0xC));
        *reg_ctl = SSQR_IVOL(12) | duty_flags[slot->channel_config];
        *reg_freq = SFREQ_HOLD | SFREQ_RESET | (fx2int(rate) & 0x7FF);
    }
}

void snd_frame(void)
{
    for (uint i = 0; i < TICKS_PER_FRAME; ++i)
        snd_tick(i);

    scanline_wait = 0;
    frame_tick_idx = 0;
    snd_irq_hblank();
    // snd_timer_irq();
}

void snd_play(snd_id_e id)
{
    // find inactive sound slot. if none were found, cut off the 0th slot.
    // TODO: this does not properly remove the oldest sound. Fix this.
    snd_slot_s *slot = snd_slots;
    for (int i = 0; i < SND_SLOT_COUNT; ++i)
    {
        snd_slot_s *v = snd_slots + i;
        if (!(v->flags & SND_SLOT_FLAG_ACTIVE))
        {
            slot = v;
            break;
        }
    }

    *slot = (snd_slot_s)
    {
        .flags = SND_SLOT_FLAG_ACTIVE,
        .ip = snd_sounds[id]
    };
    
    if (!proc_snd_slot(slot))
        slot->flags = 0;
}

ARM_FUNC
void snd_irq_hblank(void)
{    
    if (frame_tick_idx == TICKS_PER_FRAME) return;
    if (scanline_wait-- != 0) return;

    REG_SNDDMGCNT = reg_dmgctl_vals[frame_tick_idx];
    REG_SND1CNT  = reg_ctl_vals[0][frame_tick_idx];
    REG_SND2CNT  = reg_ctl_vals[1][frame_tick_idx];
    REG_SND3CNT  = reg_ctl_vals[2][frame_tick_idx];
    REG_SND4CNT  = reg_ctl_vals[3][frame_tick_idx];
    REG_SND1FREQ = reg_freq_vals[0][frame_tick_idx];
    REG_SND2FREQ = reg_freq_vals[1][frame_tick_idx];
    REG_SND3FREQ = reg_freq_vals[2][frame_tick_idx];
    REG_SND4FREQ = reg_freq_vals[3][frame_tick_idx];

    ++frame_tick_idx;
    scanline_wait = SCANLINE_WAIT_RESET;
}