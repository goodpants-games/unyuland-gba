#include <tonc.h>
#include <platutil.h>
#include <math.h>

#include "psg_ctl.h"
#include "../main/log.h"
#include "audioutil.h"


// amount of seconds between each (hypothethical) hblank
#define GBA_CLOCK_SPEED (16 * 1024 * 1024)
#define HBLANK_INTERVAL (1232.0 / GBA_CLOCK_SPEED)

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

static u16 *const reg_snd_freq[2] = { &REG_SND1FREQ, &REG_SND2FREQ };
static u16 *const reg_snd_cnt[2] = { &REG_SND1CNT, &REG_SND2CNT };

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

// https://www.martin-finke.de/articles/audio-plugins-018-polyblep-oscillator/
static double poly_blep(double phase_inc, double t)
{
    double dt = phase_inc;
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0;
    }
    else if (t > 1.0 - dt)
    {
        t = (t - 1.0) / dt;
        return t * t + t + t + 1.0;
    }
    else return 0.0;
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
}

void psg_set_sample_rate(int sr)
{
    s_sample_rate = sr;
    s_dt_per_sample = 1.0 / sr;
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

        bool sqr_l[2];
        bool sqr_r[2];
        int sqr_rate[2];
        double sqr_duty[2];
        double sqr_freq[2];
        double sqr_phdt[2];

        for (int i = 0; i < 2; ++i)
        {
            sqr_l[i] = REG_SNDDMGCNT & (SDMG_LSQR1 << i);
            sqr_r[i] = REG_SNDDMGCNT & (SDMG_RSQR1 << i);

            sqr_duty[i] = pulse_duties[(*reg_snd_cnt[i] & SSQR_DUTY_MASK) >> SSQR_DUTY_SHIFT];
            sqr_rate[i] = (*reg_snd_freq[i] & SFREQ_RATE_MASK) >> SFREQ_RATE_SHIFT;
            sqr_freq[i] = 131072.0 / (2048 - sqr_rate[i]);
            sqr_phdt[i] = sqr_freq[i] * s_dt_per_sample;

            if (*reg_snd_freq[i] & SFREQ_RESET)
            {
                // s_ch_phase[0] = 0.0;
                *reg_snd_freq[i] &= ~SFREQ_RESET;
            }
        }

        uint lvol = (REG_SNDDMGCNT & SDMG_LVOL_MASK) >> SDMG_LVOL_SHIFT;
        uint rvol = (REG_SNDDMGCNT & SDMG_LVOL_MASK) >> SDMG_LVOL_SHIFT;

        double lvol_f = lvol / 7.0;
        double rvol_f = rvol / 7.0;

        // sqr0_l = true;
        // sqr0_r = true;
        // sqr0_freq = 440.0 * 2;
        // sqr0_duty = 0.25;

        for (; frames_to_proc != 0; --frames_to_proc, out += 2)
        {
            double smp;
            double outf[2];
            outf[0] = 0.0;
            outf[1] = 0.0;

            const double amp = 0.1;

            for (int c = 0; c < 2; ++c)
            {
                smp = s_ch_phase[c] < sqr_duty[c] ? 1.0 : 0.0;
                smp += poly_blep(sqr_phdt[c], s_ch_phase[c]);
                smp -= poly_blep(sqr_phdt[c], fmod(s_ch_phase[c] + 1.0 - sqr_duty[c], 1.0));
                smp *= amp;

                s_ch_phase[c] = fmod(s_ch_phase[c] + sqr_phdt[c], 1.0);

                outf[0] += (sqr_l[c] ? smp : 0.0) * lvol_f;
                outf[1] += (sqr_r[c] ? smp : 0.0) * rvol_f;
            }

            out[0] = smpconv_f64_s16(outf[0]);
            out[1] = smpconv_f64_s16(outf[1]);
        }

        if (frames_to_next_hbl == 0)
        {
            hblank();
            s_time_to_next_hbl += HBLANK_INTERVAL;
        }
    }
}