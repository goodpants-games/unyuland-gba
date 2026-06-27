#include <stdlib.h>
#include <tonc.h>
#include <platutil.h>
#include <psg_ctl.h>

#include <data/pitchlut_bin.h>
#include <data/wave_tri_bin.h>
#include <data/wave_noise_bin.h>

#include "log.h"
#include "math_util.h"
#include "sound.h"
#include "sound_table.h"

#define TICKS_PER_PART     8
#define TICKS_PER_FRAME    8
#define ARPEGGIO_TICK      (2 * TICKS_PER_FRAME)
#define MAX_ACTIVE_SOUNDS  8

#define SWAV_SEL_DIM(n)    (((n) & 1) << 5)
#define SWAV_SEL_BANK(n)   (((n) & 1) << 6)
#define SWAV_SEL_ENABLE(n) (((n) & 1) << 7)
#define SWAV_SEL_ON        SWAV_SEL_ENABLE(1)
#define SWAV_SEL_OFF       SWAV_SEL_ENABLE(0)
#define SWAV_VOL(n)        (((n) & 3) << 13)

#define SLFSR_SHIFT(n) (((n) & 15) << 4)
#define SLFSR_DIV(n)   ((n) & 7)
#define SLFSR_WIDTH(n) (((n) & 1) << 3)

EWRAM_BSS static snd_slot_s snd_slots[MAX_ACTIVE_SOUNDS];
EWRAM_DATA static u16 next_snd_slot = 0;
const snd_cmd *snd_sounds[SND_SOUND_COUNT];

// static vu16 *const cnt_regs[] =
//     { &REG_SND1CNT, &REG_SND2CNT, &REG_SND3CNT, &REG_SND4CNT };
// #define REG_SNDXCTL(n) (*cnt_regs[n])
// #define REG_SNDXFREQ(n) (*(vu16 *)(0x04000000 + 0x0064 + ((n) << 3)))

static u16 reg_ctl_vals[4][TICKS_PER_FRAME];
static u16 reg_freq_vals[4][TICKS_PER_FRAME];
static u16 reg_dmgctl_vals[TICKS_PER_FRAME];
static u16 reg_wav_sel_vals[TICKS_PER_FRAME];

#define pitch_lut ((const FIXED *)pitchlut_bin)

ARM_FUNC static void tick_callback(int frame_tick_idx);

void snd_init(void)
{
    snd_init_table();

    // turn sound on
    REG_SNDSTAT = SSTAT_ENABLE;
    REG_SNDDMGCNT = SDMG_BUILD_LR(SDMG_SQR1, 7) | SDMG_BUILD_LR(SDMG_SQR2, 7)
                    | SDMG_BUILD_LR(SDMG_WAVE, 7);
    *((vu8 *)&REG_SNDDSCNT) = SDS_DMG100 | SDS_A50 | SDS_B50;

    // no sweep
    REG_SND1SWEEP = SSW_OFF;

    REG_SND3SEL = SWAV_SEL_BANK(0) | SWAV_SEL_DIM(0);
    // size of memcpy *should* be 4 words, but this is a remnant of a bug caused
    // by misunderstanding of the specs, and the platform place sound honestly
    // sounds better like this than when it actually works as intended.
    // it's just like the smoke graphic in mario 64...
    memcpy32((void *)REG_WAVE_RAM, wave_noise_bin, 2);
    REG_SND3SEL = SWAV_SEL_BANK(1) | SWAV_SEL_DIM(0);
    memcpy32((void *)REG_WAVE_RAM, wave_tri_bin, 4);
    REG_SND3SEL = SWAV_SEL_BANK(0) | SWAV_SEL_DIM(0);

    psg_init(TICKS_PER_FRAME, tick_callback);
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

        if (slot->flags & SND_SLOT_FLAG_VIBRATO)
        {
            slot->final_pitch +=
                (sine_lut(slot->vib_tick) * slot->vib_strength);

            slot->vib_tick += slot->vib_speed;
            if (slot->vib_tick >= 256)
                slot->vib_tick -= 256;
        }

        if (--slot->wait == 0)
        {
            slot->flags &= ~(SND_SLOT_FLAG_ARP2 | SND_SLOT_FLAG_ARP3 |
                             SND_SLOT_FLAG_SWEEP);
            
            u8 tmp;
            SWAP3(slot->pitch_reg[0], slot->pitch_reg[1], tmp);
        }

        return true;
    }

    while (true)
    {
        uint instr = (uint) *(slot->ip++);
        uint opcode = instr & SNDCMD_OP_MASK;
        switch (opcode)
        {
        case SNDCMD_OP_END:
            return false;
        
        case SNDCMD_OP_SET_CH:
            slot->channel = (instr >> 4) & 3;
            slot->channel_config = (instr >> 6) & 3;
            break;
        
        case SNDCMD_OP_ARP2:
            slot->arp_offset0 = (instr >> 4) & 0xF;
            slot->flags |= SND_SLOT_FLAG_ARP2;
            break;
        
        case SNDCMD_OP_ARP3:
            slot->arp_offset0 = (instr >> 4) & 0xF;
            slot->arp_offset1 = (instr >> 8) & 0xF;
            slot->flags |= SND_SLOT_FLAG_ARP3;
            break;
        
        case SNDCMD_OP_PITCH:
            slot->pitch_reg[(instr >> 12) & 1] = (instr >> 4) & 0xFF;
            break;
        
        case SNDCMD_OP_PLAY:
        case SNDCMD_OP_PLAY_SWP:
        {
            uint len = (instr >> 4) & 0x3F;
            if (len == 0)
            {
                LOG_WRN("sound.c: note length is 0");
                len = 1;
            }

            uint vol_start = (instr >> 10) & 7;
            uint vol_end = (instr >> 13) & 7;
            int denom = len * TICKS_PER_PART;
            
            slot->pitch = int2fx(slot->pitch_reg[0]);
            slot->vol_increment = int2fx(vol_end - vol_start) / denom;
            slot->vol = int2fx(vol_start);
            slot->wait = len * TICKS_PER_PART;
            slot->arp_index = 0;

            if (opcode == SNDCMD_OP_PLAY_SWP)
            {
                slot->flags |= SND_SLOT_FLAG_SWEEP;
                slot->pitch_increment = int2fx(slot->pitch_reg[1] - slot->pitch_reg[0])
                                            / denom;
            }
            else
            {
                slot->pitch_reg[1] = slot->pitch_reg[0];
                slot->pitch_increment = 0;
            }
            
            goto yield;
        }
        
        case SNDCMD_OP_VIBRATO:
            slot->vib_speed = (instr >> 4) & 0x3F;
            slot->vib_strength = (instr >> 10) & 0x3F;

            if (slot->vib_speed == 0 || slot->vib_strength == 0)
                slot->flags &= ~SND_SLOT_FLAG_VIBRATO;
            else
                slot->flags |= SND_SLOT_FLAG_VIBRATO;

            break;
        
        case SNDCMD_OP_PRIORITY:
            slot->priority = (instr >> 4) & 0xFF;
            break;
        }
    }

    yield:;
    return true;
}

static void stop_sound(snd_slot_s *slot)
{
    if (next_snd_slot == 0) return;

    snd_slot_s *end = snd_slots + (next_snd_slot - 1);
    while (slot != end)
    {
        *slot = *(slot + 1);
        ++slot;
    }

    --next_snd_slot;
}

static void snd_tick(uint tick_idx)
{
    static snd_slot_s *last_channel_slot[4] = { NULL, NULL, NULL, NULL };
    static u8 last_channel_vol[4] = { 0, 0, 0, 0 };
    snd_slot_s *channel_slot[4] = { NULL, NULL, NULL, NULL };

    if ((uint)next_snd_slot > MAX_ACTIVE_SOUNDS)
        __builtin_trap();
    
    for (uint i = 0; i < (uint)next_snd_slot; ++i)
    {
        snd_slot_s *slot = snd_slots + i;
        if (!(slot->flags & SND_SLOT_FLAG_ACTIVE)) continue;

        if (!proc_snd_slot(slot))
        {
            stop_sound(slot);
            --i;
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

    u16 *reg_wavsel = &reg_wav_sel_vals[tick_idx];

    for (uint ch = 0; ch < 4; ++ch)
    {
        snd_slot_s *const slot = channel_slot[ch];
        u16 *reg_ctl = &reg_ctl_vals[ch][tick_idx];
        u16 *reg_freq = &reg_freq_vals[ch][tick_idx];
        
        last_channel_slot[ch] = slot;

        if (!slot)
        {
            if (ch == 2)
            {
                *reg_wavsel = 0;
                *reg_ctl = 0;
            }
            else
            {
                *reg_ctl = SSQR_IVOL(0);
            }

            *reg_freq = 0;
            last_channel_vol[ch] = -1;
            continue;
        }

        u8 vol = (u8) fx2int(slot->vol) & 0xF;

        bool reset = channel_slot[ch] != last_channel_slot[ch] ||
                     vol != last_channel_vol[ch];
        last_channel_vol[ch] = vol;

        static const uint duty_flags[] =
            { SSQR_DUTY1_2, SSQR_DUTY1_4, SSQR_DUTY1_8 };
        
        *reg_dmgctl |= (1 << (ch + 0x8)) | (1 << (ch + 0xC));

        FIXED pitch0 = slot->final_pitch;

        if (ch == 2)
        {
            uint bank = slot->channel_config;
            *reg_wavsel = SWAV_SEL_ON | SWAV_SEL_BANK(bank) | SWAV_SEL_DIM(0);
            *reg_ctl = SWAV_VOL(1);
            pitch0 += TO_FIXED(0.5 + 12);
        }
        else if (ch == 3)
        {
            *reg_ctl = SSQR_IVOL(vol);
        }
        else
        {
            *reg_ctl = SSQR_IVOL(vol) |
                       duty_flags[slot->channel_config];
        }
        
        uint pitch1 = fx2int(pitch0);
        FIXED pitch_frac = fx2ufrac(pitch0);

        FIXED rate0 = pitch_lut[pitch1];
        FIXED rate1 = pitch_lut[pitch1 + 1];
        FIXED rate = fxmul((rate1 - rate0), pitch_frac) + rate0;

        if (ch == 3)
        {
            *reg_freq = SFREQ_HOLD | SLFSR_SHIFT(pitch1 / 8) |
                        SLFSR_DIV(0);
        }
        else
        {
            *reg_freq = SFREQ_HOLD |
                        SFREQ_RATE(fx2int(rate) & SFREQ_RATE_MASK);
        }
        
        if (reset) *reg_freq |= SFREQ_RESET;
    }
}

void snd_frame(void)
{
    for (uint i = 0; i < TICKS_PER_FRAME; ++i)
        snd_tick(i);

    psg_frame();
}

void snd_play(snd_id_e id)
{
    if (!snd_sounds[id]) return;
    if (next_snd_slot == MAX_ACTIVE_SOUNDS) return;
    
    snd_slot_s *slot = snd_slots + next_snd_slot++;
    *slot = (snd_slot_s)
    {
        .flags = SND_SLOT_FLAG_ACTIVE,
        .sound_id = id,
        .ip = snd_sounds[id]
    };
    
    if (!proc_snd_slot(slot))
        stop_sound(slot);
}

void snd_play_no_overlap(snd_id_e id)
{
    // don't play sound if an active sound slot using the same ID was found
    const uint e = next_snd_slot;
    snd_slot_s *slot_cut;
    for (uint i = 0; i < e; ++i)
    {
        slot_cut = snd_slots + i;
        if (slot_cut->sound_id == id)
        {
            *slot_cut = (snd_slot_s)
            {
                .flags = SND_SLOT_FLAG_ACTIVE,
                .sound_id = id,
                .ip = snd_sounds[id]
            };
            
            if (!proc_snd_slot(slot_cut))
                stop_sound(slot_cut);

            return;
        }
    }

    snd_play(id);
}

ARM_FUNC
static void tick_callback(int frame_tick_idx)
{
    REG_SNDDMGCNT = reg_dmgctl_vals[frame_tick_idx];
    REG_SND1CNT   = reg_ctl_vals[0][frame_tick_idx];
    REG_SND1FREQ  = reg_freq_vals[0][frame_tick_idx];
    REG_SND2CNT   = reg_ctl_vals[1][frame_tick_idx];
    REG_SND2FREQ  = reg_freq_vals[1][frame_tick_idx];
    REG_SND3SEL   = reg_wav_sel_vals[frame_tick_idx];
    REG_SND3CNT   = reg_ctl_vals[2][frame_tick_idx];
    REG_SND3FREQ  = reg_freq_vals[2][frame_tick_idx];
    REG_SND4CNT   = reg_ctl_vals[3][frame_tick_idx];
    REG_SND4FREQ  = reg_freq_vals[3][frame_tick_idx];
}