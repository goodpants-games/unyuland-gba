#include <tonc.h>
#include <platutil.h>
#include <math.h>
#include <assert.h>

#include "psg_ctl.h"
#include <log.h>
#include "audioutil.h"
#include "tonc_memdef.h"
#include "tonc_memmap.h"


// amount of seconds between each (hypothethical) hblank
#define GBA_CLOCK_SPEED (16 * 1024 * 1024)
#define HBLANK_INTERVAL (1232.0 / GBA_CLOCK_SPEED)

// i removed the reference to this register from libtonc. code must call
// psg_set_wavsel to effectively interact with the register. only psg.c will be
// able to directly read/write to that spot of memory, for bookkeeping.
#define REG_SND3SEL *(u16*)(REG_BASE+0x0070)

static psg_tick_apply_f s_tick_apply = NULL;
static psg_tick_calc_f s_tick_calc = NULL;
static int s_scanline_wait_reset;
static uint s_ticks_per_frame;
static uint s_frame_tick_idx = 0;
static uint s_scanline_wait = 0;

static int s_sample_rate;

static double s_time_to_next_hbl = 0;
static double s_dt_per_sample = 0;
static double s_ch_phase[4];

static u8 s_wave_banks[2][16];
static int s_cur_wave_bank;

static u16 *const reg_snd_freq[3] = { &REG_SND1FREQ, &REG_SND2FREQ, &REG_SND3FREQ };
static u16 *const reg_snd_cnt[3] = { &REG_SND1CNT, &REG_SND2CNT, &REG_SND3CNT };

static void hblank(void)
{    
    if (s_frame_tick_idx == s_ticks_per_frame)
    {
        s_frame_tick_idx = 0;
        s_scanline_wait = 0;

        if (s_tick_calc)
        {
            for (uint i = 0; i < s_ticks_per_frame; ++i)
                s_tick_calc(i);
        }
    }

    if (s_scanline_wait-- != 0) return;

    if (s_tick_apply)
        s_tick_apply(s_frame_tick_idx);

    ++s_frame_tick_idx;
    s_scanline_wait = s_scanline_wait_reset;
}

void psg_init(const psg_init_params_s *params)
{
    s_ticks_per_frame = params->ticks_per_frame;
    s_scanline_wait_reset = 228 / params->ticks_per_frame;
    s_tick_apply = params->tick_apply;
    s_tick_calc = params->tick_calc;
    s_time_to_next_hbl = 0.0;

    s_ch_phase[0] = 0.0;
    s_ch_phase[1] = 0.0;
    s_ch_phase[2] = 0.0;
    s_ch_phase[3] = 0.0;

    memset(s_wave_banks, 0, sizeof(s_wave_banks));
    s_cur_wave_bank = 0;
}

void psg_set_sample_rate(int sr)
{
    s_sample_rate = sr;
    s_dt_per_sample = 1.0 / sr;
}

void psg_set_wavsel(u16 value)
{
    REG_SND3SEL = value;

    uint bank_idx = (value & SWSEL_BANK_MASK) >> SWSEL_BANK_SHIFT;

    if (bank_idx == s_cur_wave_bank) return;
    assert(bank_idx == 0 || bank_idx == 1);

    // copy current REG_WAVE_RAM to bank being switched to, then copy the
    // contents of the bank being switched away from to REG_WAVE_RAM.
    memcpy(s_wave_banks + bank_idx, REG_WAVE_RAM, 16);
    memcpy(REG_WAVE_RAM, s_wave_banks + s_cur_wave_bank, 16);

    s_cur_wave_bank = bank_idx;
}

void psg_render(int16_t *out, size_t frame_count)
{
    while (frame_count > 0)
    {
        size_t frames_to_next_hbl = (size_t)(s_time_to_next_hbl * s_sample_rate);

        size_t frames_to_proc = frames_to_next_hbl;
        if (frames_to_proc > frame_count)
            frames_to_proc = frame_count;
        
        s_time_to_next_hbl -= frames_to_proc * s_dt_per_sample;
        frames_to_next_hbl -= frames_to_proc;
        frame_count -= frames_to_proc;

        static const double pulse_duties[] = {
            0.125, 0.25, 0.50, 0.75
        };

        bool     ch_l   [3];
        bool     ch_r   [3];
        bool     ch_on  [3];
        uint16_t ch_vol [3][2];
        double   ch_duty[3];
        double   ch_freq[3];
        double   ch_phdt[3];        

        for (int i = 0; i < 3; ++i)
        {
            // sqr_on[i] = REG_SNDSTAT & (1 << i);
            // if (!sqr_on[i])
            //     continue;
            ch_on[i] = true;

            bool enable_l = REG_SNDDMGCNT & (SDMG_LSQR1 << i);
            bool enable_r = REG_SNDDMGCNT & (SDMG_RSQR1 << i);

            if (*reg_snd_freq[i] & SFREQ_RESET)
            {
                // s_ch_phase[0] = 0.0;
                *reg_snd_freq[i] &= ~SFREQ_RESET;
            }

            uint16_t vol;

            if (i != 2)
            {
                ch_duty[i] = pulse_duties[(*reg_snd_cnt[i] & SSQR_DUTY_MASK) >> SSQR_DUTY_SHIFT];
                vol = (*reg_snd_cnt[i] & SSQR_IVOL_MASK) >> SSQR_IVOL_SHIFT;

                uint rate = (*reg_snd_freq[i] & SFREQ_RATE_MASK) >> SFREQ_RATE_SHIFT;
                ch_freq[i] = 131072.0 / (2048 - rate);
            }
            else
            {
                vol = (*reg_snd_cnt[i] & SWAV_IVOL_MASK) >> SWAV_IVOL_SHIFT;

                uint rate = (*reg_snd_freq[i] & SFREQ_RATE_MASK) >> SFREQ_RATE_SHIFT;
                ch_freq[i] = (65536.0 / (2048 - rate));
            }

            ch_phdt[i] = ch_freq[i] * s_dt_per_sample;
            
            ch_l[i] = enable_l;
            ch_r[i] = enable_r;
            ch_vol[i][0] = enable_l ? vol : 0;
            ch_vol[i][1] = enable_r ? vol : 0;
        }

        uint lvol = (REG_SNDDMGCNT & SDMG_LVOL_MASK) >> SDMG_LVOL_SHIFT;
        uint rvol = (REG_SNDDMGCNT & SDMG_LVOL_MASK) >> SDMG_LVOL_SHIFT;
        ch_on[2] = REG_SND3SEL & SWSEL_ON;

        for (; frames_to_proc != 0; --frames_to_proc, out += 2)
        {
            out[0] = 0;
            out[1] = 0;

            for (int c = 0; c < 2; ++c)
            {
                if (!ch_on[c]) continue;

                uint16_t smp = s_ch_phase[c] < ch_duty[c] ? 1 : 0;
                s_ch_phase[c] = fmod(s_ch_phase[c] + ch_phdt[c], 1.0);

                uint16_t smp_l = ch_l[c] ? (smp * ch_vol[c][0]) : 0;
                uint16_t smp_r = ch_r[c] ? (smp * ch_vol[c][1]) : 0;
                out[0] += (int16_t)(smp_l << 1) - (ch_vol[c][0]);
                out[1] += (int16_t)(smp_r << 1) - (ch_vol[c][1]);
            }

            // process wave channel
            if (ch_on[2])
            {
                const int c = 2;

                uint nibble_idx = (uint)(s_ch_phase[c] * 32);
                assert(nibble_idx >= 0 && nibble_idx < 32);
                uint16_t smp_byte = s_wave_banks[s_cur_wave_bank][nibble_idx>>1];

                assert(s_cur_wave_bank == 0 || s_cur_wave_bank == 1);

                uint16_t smp = (smp_byte >> ((1 - (nibble_idx & 1)) * 4)) & 0xF;

                s_ch_phase[c] = fmod(s_ch_phase[c] + ch_phdt[c], 1.0);

                uint16_t smp_l = ch_l[c] ? (smp * ch_vol[c][0]) : 0;
                uint16_t smp_r = ch_r[c] ? (smp * ch_vol[c][1]) : 0;
                out[0] += (int16_t)(smp_l << 1) - 15;
                out[1] += (int16_t)(smp_r << 1) - 15;
            }

            out[0] *= lvol;
            out[1] *= rvol;
        }

        if (frames_to_next_hbl == 0)
        {
            hblank();
            s_time_to_next_hbl += HBLANK_INTERVAL;
        }
    }
}