// SPDX-License-Identifier: ISC
//
// Copyright (c) 2008, Mukunda Johnson (mukunda@maxmod.org)
// Copyright (c) 2021-2025, Antonio Niño Díaz (antonio_nd@outlook.com)

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <maxmod.h>
#include <mm_mas.h>

#include "../gba/main_gba.h"
#include "../gba/mixer.h"

#define ARM_CODE//   __attribute__((target("arm")))
#define IWRAM_CODE// __attribute__((section(".iwram"), long_call))

mm_byte mp_mix_seg; // Mixing segment select

mm_addr mm_mixbuffer;

mm_mixer_channel *mm_mix_channels;

mm_word mm_bpmdv;

mm_word mm_mixlen;

mm_word mm_ratescale;

mm_addr mp_writepos; // wavebuffer write position

static mm_addr mm_wavebuffer;

static mm_word mm_mixch_count;

mm_mixer_channel *mm_mixch_end;

static mm_word mm_timerfreq;

// Pointer to a user function to be called during the vblank irq
static mm_voidfunc mm_vblank_function;

// Set channel volume
void mmMixerSetVolume(int channel, mm_word volume)
{
    mm_mix_channels[channel].vol = volume;
}

// Set channel panning
void mmMixerSetPan(int channel, mm_byte panning)
{
    mm_mix_channels[channel].pan = panning;
}

// Scale mixing frequency
void mmMixerMulFreq(int channel, mm_word factor)
{
    mm_word freq = mm_mix_channels[channel].freq;

    freq = (freq * factor) >> 10;

    mm_mix_channels[channel].freq = freq;
}

// Stop mixing channel
void mmMixerStopChannel(int channel)
{
    // Set MSB (disable) of source
    mm_mix_channels[channel].src = MIXCH_GBA_SRC_STOPPED;
}

// Set channel read position
void mmMixerSetRead(int channel, mm_word value)
{
    // Store new offset
    mm_mix_channels[channel].read = value;
}

// Set channel mixing rate
void mmMixerSetFreq(int channel, mm_word rate)
{
    mm_mix_channels[channel].freq = rate << 2;
}

static bool vblank_handler_enabled = false;

// VBL wrapper, used to reset DMA. It needs the highest priority.
IWRAM_CODE ARM_CODE void mmVBlank(void)
{
    // Disable until ready
    if (vblank_handler_enabled)
    {
        // Swap mixing segment
        mp_mix_seg = ~mp_mix_seg;

        if (mp_mix_seg != 0)
        {
            // DMA control: Restart DMA

            // Disable DMA
            REG_DMA1CNT_H = 0x0440;
            REG_DMA2CNT_H = 0x0440;

            // Restart DMA
            REG_DMA1CNT_H = 0xB600;
            REG_DMA2CNT_H = 0xB600;
        }
        else
        {
            // Restart write position
            mp_writepos = mm_wavebuffer;
        }
    }

    // Call user handler
    if (mm_vblank_function != NULL)
        mm_vblank_function();
}

// Set function to be called during the vblank IRQ
void mmSetVBlankHandler(mm_voidfunc function)
{
    mm_vblank_function = function;
}

// Get function to be called during the vblank IRQ
mm_voidfunc mmGetVBlankHandler(void)
{
    return mm_vblank_function;
}

// Initialize mixer
void mmMixerInit(mm_gba_system *setup)
{
    mm_mixch_count = setup->mix_channel_count;

    mm_mix_channels = setup->mixing_channels;

    mm_mixch_end = &mm_mix_channels[mm_mixch_count];

    mm_mixbuffer = setup->mixing_memory;

    mm_wavebuffer = setup->wave_memory;

    mp_writepos = mm_wavebuffer;

    mm_word mode = setup->mixing_mode;

    // round(rate / 59.737)
    static const mm_hword mp_mixing_lengths[] = {
        136,  176,   224,   264,   304,   352,   448,   528
    //  8khz, 10khz, 13khz, 16khz, 18khz, 21khz, 27khz, 32khz
    };

    mm_mixlen = mp_mixing_lengths[mode];

    // 15768*16384 / rate
    static const mm_hword mp_rate_scales[] = {
        31812, 24576, 19310, 16384, 14228, 12288,  9655,  8192
    //  8khz,  10khz, 13khz, 16khz, 18khz, 21khz, 27khz, 32khz
    //  8121,  10512, 13379, 15768, 18157, 21024, 26758, 31536
    };

    mm_ratescale = mp_rate_scales[mode];

    // gbaclock / rate
    static const mm_hword mp_timing_sheet[] = {
        -2066, -1596, -1254, -1064, -924,  -798,  -627,  -532
    //  8khz,  10khz, 13khz, 16khz, 18khz, 21khz, 27khz, 32khz
    };

    mm_timerfreq = mp_timing_sheet[mode];

    // rate * 2.5
    static const mm_word mp_bpm_divisors[] = {
        20302, 26280, 33447, 39420, 45393, 52560, 66895, 78840
    };

    mm_bpmdv = mp_bpm_divisors[mode];

    // Clear wave buffer
    memset(mm_wavebuffer, 0, mm_mixlen * sizeof(mm_word));

    // Reset mixing segment
    mp_mix_seg = 0;

    // Disable mixing channels

    mm_mixer_channel *mix_ch = &mm_mix_channels[0];

    for (mm_word i = 0; i < mm_mixch_count; i++)
        mix_ch[i].src = MIXCH_GBA_SRC_STOPPED;

    // Enable VBL routine
    vblank_handler_enabled = true;

    // Clear fifo data
    *REG_SGFIFOA = 0;
    *REG_SGFIFOB = 0;

    // Reset direct sound
    REG_SOUNDCNT_H = 0;

    // Setup sound: DIRECT SOUND A/B reset, timer0, A=left, B=right, volume=100%
    REG_SOUNDCNT_H = 0x9A0C;

    // TODO: Setup DMA source addresses (playback buffers)
    // REG_DMA1SAD = (mm_word)mm_wavebuffer;
    // REG_DMA2SAD = (mm_word)mm_wavebuffer + mm_mixlen * 2;

    // TODO: Setup DMA destination (sound fifo)
    // REG_DMA1DAD = (mm_word)REG_SGFIFOA;
    // REG_DMA2DAD = (mm_word)REG_SGFIFOB;

    // Enable DMA [enable, fifo request, 32-bit, repeat]
    REG_DMA1CNT = 0xB6000000;
    REG_DMA2CNT = 0xB6000000;

    // Master sound enable
    REG_SOUNDCNT_X = 0x80;

    // Enable sampling timer
    REG_TM0CNT = mm_timerfreq | (0x80 << 16);
}

void mmMixerEnd(void)
{
    // Silence direct sound channels
    REG_SOUNDCNT_H = 0;

    // Disable VBL routine
    vblank_handler_enabled = false;

    // Disable DMA
    REG_DMA1CNT = 0;
    REG_DMA2CNT = 0;

    // Disable sampling timer
    REG_TM0CNT = 0;
}

// void mmMixerMix(mm_word samples_count)
// {
//   int panCalc;
//   mm_word smpRemain;
//   mm_word *writeDest;
//   mm_word *puVar1;
//   mm_word sampleReadCnt;
//   mm_word readCnt2;
//   int iVar2;
//   mm_word fetch2_0;
//   mm_hword *puVar3;
//   mm_word fetch1;
//   mm_word fetch2_1;
//   mm_hword *puVar4;
//   int extraout_r3;
//   int extraout_r3_00;
//   int rMixCC;
//   int iVar5;
//   mm_word fetch2;
//   mm_word fetch2_2;
//   mm_word fetch3;
//   mm_word fetch2_3;
//   mm_word fetch4;
//   mm_word fetch2_4;
//   mm_word rVolR;
//   mm_word rread;
//   mm_word fetch5;
//   mm_word fetch2_5;
//   int iVar6;
//   mm_word fetch6;
//   mm_word fetch2_6;
//   mm_word *puVar7;
//   mm_word *puVar8;
//   mm_word fetch7;
//   mm_word fetch2_7;
//   mm_word uVar9;
//   mm_word rfreq;
//   mm_mas_gba_sample *rsrc;
//   mm_byte *srcSmp;
//   mm_word *puVar10;
//   mm_word *puVar11;
//   mm_word rVolA;
//   mm_word fetch8;
//   mm_word fetch2_8;
//   mm_word uVar12;
//   mm_word fetch9;
//   mm_word fetch2_9;
//   mm_word uVar13;
//   char cVar14;
//   char cVar15;
//   bool bVar16;
//   mm_word local_28;
//   mm_mixer_channel *rchan;
//   mm_word rVolL;
  
//   if (samples_count == 0) {
//     return;
//   }
//   puVar8 = &local_28;
//   local_28 = samples_count;
//   rVolA = samples_count & 7;
//   puVar7 = mm_mixbuffer;
//   for (samples_count = samples_count >> 3; samples_count != 0; samples_count = samples_count - 1) {
//     *puVar7 = 0;
//     puVar7[1] = 0;
//     puVar7[2] = 0;
//     puVar7[3] = 0;
//     puVar7[4] = 0;
//     puVar7[5] = 0;
//     puVar7[6] = 0;
//     puVar7[7] = 0;
//     puVar7 = puVar7 + 8;
//   }
//   for (; rVolA != 0; rVolA = rVolA - 1) {
//     *puVar7 = 0;
//     puVar7 = puVar7 + 1;
//   }
//   rVolA = 0;
//   rchan = mm_mix_channels;
//   do {
//     rsrc = (mm_mas_gba_sample *)rchan->src;
//     if ((-1 < (int)rsrc) && (rchan->freq != 0)) {
//       rfreq = mm_ratescale * rchan->freq >> 0xe;
//       rread = rchan->read;
//       panCalc = 0x100 - (mm_word)rchan->pan;
//       rVolL = panCalc * (mm_word)rchan->vol >> 8;
//       rVolR = (0x100 - panCalc) * (mm_word)rchan->vol >> 8;
//       rVolA = rVolA + rVolL + rVolR * 0x10000;
//       rMixCC = *puVar8;
//       puVar7 = mm_mixbuffer;
// .mpm_remix_test:
//       do {
//         bVar16 = false;
//         sampleReadCnt = rMixCC * rfreq;
//         if (((int)rfreq < 0x1780) && (0x180000 < sampleReadCnt)) {
//           sampleReadCnt = 0x180000;
//           bVar16 = true;
//         }
//         smpRemain = rsrc[-1].length * 0x1000 - rread;
//         if ((smpRemain < sampleReadCnt) || (smpRemain = sampleReadCnt, bVar16)) {
//           puVar8[-1] = smpRemain;
//           iVar2 = 0;
//           while (bVar16 = rfreq * 0x10000 <= smpRemain, smpRemain = smpRemain + rfreq * -0x10000,
//                 bVar16) {
//             iVar2 = iVar2 + 0x10000;
//           }
//           smpRemain = smpRemain + rfreq * 0x10000;
//           if (rfreq << 0xf <= smpRemain) {
//             smpRemain = smpRemain + rfreq * -0x8000;
//             iVar2 = iVar2 + 0x8000;
//           }
//           if (rfreq << 0xe <= smpRemain) {
//             smpRemain = smpRemain + rfreq * -0x4000;
//             iVar2 = iVar2 + 0x4000;
//           }
//           if (rfreq << 0xd <= smpRemain) {
//             smpRemain = smpRemain + rfreq * -0x2000;
//             iVar2 = iVar2 + 0x2000;
//           }
//           if (rfreq << 0xc <= smpRemain) {
//             smpRemain = smpRemain + rfreq * -0x1000;
//             iVar2 = iVar2 + 0x1000;
//           }
//           if (rfreq << 0xb <= smpRemain) {
//             smpRemain = smpRemain + rfreq * -0x800;
//             iVar2 = iVar2 + 0x800;
//           }
//           if (rfreq << 10 <= smpRemain) {
//             smpRemain = smpRemain + rfreq * -0x400;
//             iVar2 = iVar2 + 0x400;
//           }
//           if (rfreq << 9 <= smpRemain) {
//             smpRemain = smpRemain + rfreq * -0x200;
//             iVar2 = iVar2 + 0x200;
//           }
//           if (rfreq << 8 <= smpRemain) {
//             smpRemain = smpRemain + rfreq * -0x100;
//             iVar2 = iVar2 + 0x100;
//           }
//           if (rfreq << 7 <= smpRemain) {
//             smpRemain = smpRemain + rfreq * -0x80;
//             iVar2 = iVar2 + 0x80;
//           }
//           if (rfreq << 6 <= smpRemain) {
//             smpRemain = smpRemain + rfreq * -0x40;
//             iVar2 = iVar2 + 0x40;
//           }
//           if (rfreq << 5 <= smpRemain) {
//             smpRemain = smpRemain + rfreq * -0x20;
//             iVar2 = iVar2 + 0x20;
//           }
//           if (rfreq << 4 <= smpRemain) {
//             smpRemain = smpRemain + rfreq * -0x10;
//             iVar2 = iVar2 + 0x10;
//           }
//           if (rfreq << 3 <= smpRemain) {
//             smpRemain = smpRemain + rfreq * -8;
//             iVar2 = iVar2 + 8;
//           }
//           if (rfreq << 2 <= smpRemain) {
//             smpRemain = smpRemain + rfreq * -4;
//             iVar2 = iVar2 + 4;
//           }
//           if (rfreq << 1 <= smpRemain) {
//             smpRemain = smpRemain + rfreq * -2;
//             iVar2 = iVar2 + 2;
//           }
//           if (rfreq <= smpRemain) {
//             smpRemain = smpRemain - rfreq;
//             iVar2 = iVar2 + 1;
//           }
//           iVar2 = iVar2 + (mm_word)(smpRemain != 0);
//           sampleReadCnt = puVar8[-1];
//           iVar5 = rMixCC - iVar2;
//           rMixCC = iVar2;
//         }
//         else {
//           iVar5 = 0;
//         }
// .mpm_mix_short:
//         puVar8[-1] = (mm_word)rchan;
//         puVar8[-2] = rVolA;
//         puVar10 = puVar8 + -3;
//         *puVar10 = iVar5;
//         if (rMixCC != 0) {
//           if ((int)rfreq < 0x1780) {
//             puVar8[-4] = (mm_word)rchan;
//             puVar8[-5] = rVolA;
//             puVar8[-6] = (mm_word)rsrc;
//             puVar8[-7] = rfreq;
//             puVar8[-8] = (mm_word)puVar7;
//             puVar8[-9] = rread;
//             puVar8[-10] = rVolR;
//             puVar8[-0xb] = rVolL;
//             puVar8[-0xc] = iVar5;
//             puVar8[-0xd] = rMixCC;
//             writeDest = &mm_fetch;
//             srcSmp = (byte *)((mm_word)((int)&rsrc->length + (rread >> 0xc)) & 0xfffffffc);
//             readCnt2 = sampleReadCnt + 0x4000;
//             sampleReadCnt = sampleReadCnt - 0x24000;
//             while (bVar16 = 0x27fff < readCnt2, puVar7 = writeDest, readCnt2 = sampleReadCnt,
//                   puVar10 = (mm_word *)srcSmp, bVar16) {
//               fetch1 = *(mm_word *)((int)srcSmp + 4);
//               fetch2 = *(mm_word *)((int)srcSmp + 8);
//               fetch3 = *(mm_word *)((int)srcSmp + 0xc);
//               fetch4 = *(mm_word *)((int)srcSmp + 0x10);
//               fetch5 = *(mm_word *)((int)srcSmp + 0x14);
//               fetch6 = *(mm_word *)((int)srcSmp + 0x18);
//               fetch7 = *(mm_word *)((int)srcSmp + 0x1c);
//               fetch8 = *(mm_word *)((int)srcSmp + 0x20);
//               fetch9 = *(mm_word *)((int)srcSmp + 0x24);
//               puVar10 = (mm_word *)((int)srcSmp + 0x28);
//               *writeDest = *(mm_word *)srcSmp;
//               writeDest[1] = fetch1;
//               writeDest[2] = fetch2;
//               writeDest[3] = fetch3;
//               writeDest[4] = fetch4;
//               writeDest[5] = fetch5;
//               writeDest[6] = fetch6;
//               writeDest[7] = fetch7;
//               writeDest[8] = fetch8;
//               writeDest[9] = fetch9;
//               puVar7 = writeDest + 10;
//               readCnt2 = sampleReadCnt - 0x28000;
//               if (sampleReadCnt < 0x28000) break;
//               fetch2_0 = *puVar10;
//               fetch2_1 = *(mm_word *)((int)srcSmp + 0x2c);
//               fetch2_2 = *(mm_word *)((int)srcSmp + 0x30);
//               fetch2_3 = *(mm_word *)((int)srcSmp + 0x34);
//               fetch2_4 = *(mm_word *)((int)srcSmp + 0x38);
//               fetch2_5 = *(mm_word *)((int)srcSmp + 0x3c);
//               fetch2_6 = *(mm_word *)((int)srcSmp + 0x40);
//               fetch2_7 = *(mm_word *)((int)srcSmp + 0x44);
//               fetch2_8 = *(mm_word *)((int)srcSmp + 0x48);
//               fetch2_9 = *(mm_word *)((int)srcSmp + 0x4c);
//               puVar10 = (mm_word *)((int)srcSmp + 0x50);
//               *puVar7 = fetch2_0;
//               writeDest[0xb] = fetch2_1;
//               writeDest[0xc] = fetch2_2;
//               writeDest[0xd] = fetch2_3;
//               writeDest[0xe] = fetch2_4;
//               writeDest[0xf] = fetch2_5;
//               writeDest[0x10] = fetch2_6;
//               writeDest[0x11] = fetch2_7;
//               writeDest[0x12] = fetch2_8;
//               writeDest[0x13] = fetch2_9;
//               puVar7 = writeDest + 0x14;
//               bVar16 = readCnt2 < 0x28000;
//               readCnt2 = sampleReadCnt - 0x50000;
//               if (bVar16) break;
//               rVolA = *(mm_word *)((int)srcSmp + 0x54);
//               rfreq = *(mm_word *)((int)srcSmp + 0x58);
//               rVolL = *(mm_word *)((int)srcSmp + 0x5c);
//               rVolR = *(mm_word *)((int)srcSmp + 0x60);
//               rread = *(mm_word *)((int)srcSmp + 100);
//               smpRemain = *(mm_word *)((int)srcSmp + 0x68);
//               uVar9 = *(mm_word *)((int)srcSmp + 0x6c);
//               uVar12 = *(mm_word *)((int)srcSmp + 0x70);
//               uVar13 = *(mm_word *)((int)srcSmp + 0x74);
//               srcSmp = (byte *)((int)srcSmp + 0x78);
//               *puVar7 = *puVar10;
//               writeDest[0x15] = rVolA;
//               writeDest[0x16] = rfreq;
//               writeDest[0x17] = rVolL;
//               writeDest[0x18] = rVolR;
//               writeDest[0x19] = rread;
//               writeDest[0x1a] = smpRemain;
//               writeDest[0x1b] = uVar9;
//               writeDest[0x1c] = uVar12;
//               writeDest[0x1d] = uVar13;
//               writeDest = writeDest + 0x1e;
//               sampleReadCnt = sampleReadCnt - 0x78000;
//             }
//             rfreq = readCnt2 + 0x10000;
//             puVar1 = puVar7;
//             rVolA = rfreq;
//             puVar11 = puVar10;
//             if (-1 < (int)rfreq) {
//               do {
//                 rVolA = puVar10[1];
//                 rVolL = puVar10[2];
//                 rVolR = puVar10[3];
//                 rread = puVar10[4];
//                 sampleReadCnt = puVar10[5];
//                 *puVar7 = *puVar10;
//                 puVar7[1] = rVolA;
//                 puVar7[2] = rVolL;
//                 puVar7[3] = rVolR;
//                 puVar7[4] = rread;
//                 puVar7[5] = sampleReadCnt;
//                 puVar1 = puVar7 + 6;
//                 rVolA = rfreq - 0x18000;
//                 puVar11 = puVar10 + 6;
//                 if (rfreq < 0x18000) break;
//                 rVolA = puVar10[7];
//                 rVolL = puVar10[8];
//                 rVolR = puVar10[9];
//                 rread = puVar10[10];
//                 sampleReadCnt = puVar10[0xb];
//                 puVar11 = puVar10 + 0xc;
//                 puVar7[6] = puVar10[6];
//                 puVar7[7] = rVolA;
//                 puVar7[8] = rVolL;
//                 puVar7[9] = rVolR;
//                 puVar7[10] = rread;
//                 puVar7[0xb] = sampleReadCnt;
//                 rVolL = rfreq - 0x30000;
//                 puVar1 = puVar7 + 0xc;
//                 rVolA = rVolL;
//                 if (rfreq - 0x18000 < 0x18000) break;
//                 rVolA = puVar10[0xd];
//                 rVolR = puVar10[0xe];
//                 rread = puVar10[0xf];
//                 sampleReadCnt = puVar10[0x10];
//                 smpRemain = puVar10[0x11];
//                 puVar10 = puVar10 + 0x12;
//                 puVar7[0xc] = *puVar11;
//                 puVar7[0xd] = rVolA;
//                 puVar7[0xe] = rVolR;
//                 puVar7[0xf] = rread;
//                 puVar7[0x10] = sampleReadCnt;
//                 puVar7[0x11] = smpRemain;
//                 puVar7 = puVar7 + 0x12;
//                 rfreq = rfreq - 0x48000;
//                 puVar1 = puVar7;
//                 rVolA = rfreq;
//                 puVar11 = puVar10;
//               } while (0x17fff < rVolL);
//             }
//             iVar2 = rVolA + 0x18000;
//             if (-1 < iVar2) {
//               do {
//                 puVar10 = puVar11 + 1;
//                 puVar7 = puVar1 + 1;
//                 *puVar1 = *puVar11;
//                 iVar5 = iVar2 + -0x4000;
//                 if (iVar2 < 0x4001) break;
//                 puVar11 = puVar11 + 2;
//                 puVar1 = puVar1 + 2;
//                 *puVar7 = *puVar10;
//                 iVar2 = iVar2 + -0x8000;
//               } while (iVar2 != 0 && 0x3fff < iVar5);
//             }
//             rMixCC = puVar8[-0xd];
//             rVolL = puVar8[-0xb];
//             rVolR = puVar8[-10];
//             puVar7 = (mm_word *)puVar8[-8];
//             rfreq = puVar8[-7];
//             rVolA = puVar8[-9] >> 0xc;
//             puVar8[-4] = puVar8[-6];
//             puVar10 = puVar8 + -5;
//             *puVar10 = rVolA;
//             rread = puVar8[-9] & ~(rVolA << 0xc);
//             rsrc = (mm_mas_gba_sample *)((rVolA & 3) + 0x10000);
//           }
//           if (((mm_word)puVar7 & 3) != 0) {
//             rVolA = (mm_word)*(byte *)((int)&rsrc->length + (rread >> 0xc));
//             rread = rread + rfreq;
//             puVar8 = puVar7 + 1;
//             *(short *)puVar7 = (short)*puVar7 + (short)(rVolA * rVolL >> 5);
//             puVar7 = (mm_word *)((int)puVar7 + 6);
//             *(short *)puVar8 = (short)*puVar8 + (short)(rVolA * rVolR >> 5);
//             rMixCC = rMixCC + -1;
//           }
//           if (rVolL == rVolR) {
//             if (rVolL == 0) {
//               rread = rread + rMixCC * rfreq;
//             }
//             else {
//               while (iVar2 = rMixCC + -6, -1 < iVar2) {
//                 rVolA = rread + rfreq + rfreq;
//                 smpRemain = ((mm_word)*(byte *)((int)&rsrc->length + (rread >> 0xc)) |
//                             (mm_word)*(byte *)((int)&rsrc->length + (rread + rfreq >> 0xc)) << 0x10) *
//                             rVolL & 0xffe0ffff;
//                 rVolR = rVolA + rfreq;
//                 sampleReadCnt = rVolR + rfreq;
//                 rVolR = ((mm_word)*(byte *)((int)&rsrc->length + (rVolA >> 0xc)) |
//                         (mm_word)*(byte *)((int)&rsrc->length + (rVolR >> 0xc)) << 0x10) * rVolL &
//                         0xffe0ffff;
//                 rVolA = sampleReadCnt + rfreq;
//                 rread = rVolA + rfreq;
//                 rVolA = ((mm_word)*(byte *)((int)&rsrc->length + (sampleReadCnt >> 0xc)) |
//                         (mm_word)*(byte *)((int)&rsrc->length + (rVolA >> 0xc)) << 0x10) * rVolL &
//                         0xffe0ffff;
//                 *puVar7 = *puVar7 + (smpRemain >> 5);
//                 puVar7[1] = puVar7[1] + (smpRemain >> 5);
//                 puVar7[2] = puVar7[2] + (rVolR >> 5);
//                 puVar7[3] = puVar7[3] + (rVolR >> 5);
//                 puVar7[4] = puVar7[4] + (rVolA >> 5);
//                 puVar7[5] = puVar7[5] + (rVolA >> 5);
//                 puVar7 = puVar7 + 6;
//                 rMixCC = iVar2;
//               }
//               rVolR = rVolL;
//               if (rMixCC != 0 && -7 < iVar2) goto mmMix_Remainder;
//             }
//           }
//           else {
//             cVar15 = (int)rVolL < 0;
//             bVar16 = rVolL == 0;
//             cVar14 = '\0';
//             if (bVar16) {
//               mmMix_SingleChannel();
//               rVolL = 0;
//               rMixCC = extraout_r3_00;
//               if (!bVar16 && cVar15 == cVar14) goto mmMix_Remainder;
//             }
//             else {
//               cVar15 = (int)rVolR < 0;
//               bVar16 = rVolR == 0;
//               cVar14 = '\0';
//               if (bVar16) {
//                 mmMix_SingleChannel();
//                 rMixCC = extraout_r3;
//                 if (!bVar16 && cVar15 == cVar14) goto mmMix_Remainder;
//               }
//               else {
//                 while (iVar2 = rMixCC + -10, -1 < iVar2) {
//                   sampleReadCnt = rread + rfreq + rfreq;
//                   rVolA = (mm_word)*(byte *)((int)&rsrc->length + (rread >> 0xc)) |
//                           (mm_word)*(byte *)((int)&rsrc->length + (rread + rfreq >> 0xc)) << 0x10;
//                   rread = sampleReadCnt + rfreq;
//                   smpRemain = rread + rfreq;
//                   rread = (mm_word)*(byte *)((int)&rsrc->length + (sampleReadCnt >> 0xc)) |
//                           (mm_word)*(byte *)((int)&rsrc->length + (rread >> 0xc)) << 0x10;
//                   sampleReadCnt = smpRemain + rfreq;
//                   uVar9 = sampleReadCnt + rfreq;
//                   sampleReadCnt =
//                        (mm_word)*(byte *)((int)&rsrc->length + (smpRemain >> 0xc)) |
//                        (mm_word)*(byte *)((int)&rsrc->length + (sampleReadCnt >> 0xc)) << 0x10;
//                   *puVar7 = *puVar7 + ((rVolA * rVolL & 0xffe0ffff) >> 5);
//                   puVar7[1] = puVar7[1] + ((rVolA * rVolR & 0xffe0ffff) >> 5);
//                   puVar7[2] = puVar7[2] + ((rread * rVolL & 0xffe0ffff) >> 5);
//                   puVar7[3] = puVar7[3] + ((rread * rVolR & 0xffe0ffff) >> 5);
//                   puVar7[4] = puVar7[4] + ((sampleReadCnt * rVolL & 0xffe0ffff) >> 5);
//                   rVolA = uVar9 + rfreq;
//                   smpRemain = rVolA + rfreq;
//                   rVolA = (mm_word)*(byte *)((int)&rsrc->length + (uVar9 >> 0xc)) |
//                           (mm_word)*(byte *)((int)&rsrc->length + (rVolA >> 0xc)) << 0x10;
//                   uVar9 = smpRemain + rfreq;
//                   rread = uVar9 + rfreq;
//                   smpRemain = (mm_word)*(byte *)((int)&rsrc->length + (smpRemain >> 0xc)) |
//                               (mm_word)*(byte *)((int)&rsrc->length + (uVar9 >> 0xc)) << 0x10;
//                   puVar7[5] = puVar7[5] + ((sampleReadCnt * rVolR & 0xffe0ffff) >> 5);
//                   puVar7[6] = puVar7[6] + ((rVolA * rVolL & 0xffe0ffff) >> 5);
//                   puVar7[7] = puVar7[7] + ((rVolA * rVolR & 0xffe0ffff) >> 5);
//                   puVar7[8] = puVar7[8] + ((smpRemain * rVolL & 0xffe0ffff) >> 5);
//                   puVar7[9] = puVar7[9] + ((smpRemain * rVolR & 0xffe0ffff) >> 5);
//                   puVar7 = puVar7 + 10;
//                   rMixCC = iVar2;
//                 }
//                 if (rMixCC != 0 && -0xb < iVar2) {
// mmMix_Remainder:
//                   rVolA = rVolL | rVolR << 0x10;
//                   puVar8 = puVar7;
//                   do {
//                     sampleReadCnt = rread >> 0xc;
//                     rread = rread + rfreq;
//                     sampleReadCnt = rVolA * *(byte *)((int)&rsrc->length + sampleReadCnt);
//                     puVar7 = (mm_word *)((int)puVar8 + 2);
//                     *(short *)puVar8 = (short)*puVar8 + (short)((sampleReadCnt & 0xff00ffff) >> 5);
//                     *(ushort *)(puVar8 + 1) = (short)puVar8[1] + (ushort)(sampleReadCnt >> 0x15);
//                     iVar2 = rMixCC + -2;
//                     if (rMixCC < 2) break;
//                     sampleReadCnt = rread >> 0xc;
//                     rread = rread + rfreq;
//                     sampleReadCnt = rVolA * *(byte *)((int)&rsrc->length + sampleReadCnt);
//                     *(short *)puVar7 = *(short *)puVar7 + (short)((sampleReadCnt & 0xff00ffff) >> 5)
//                     ;
//                     puVar7 = puVar8 + 2;
//                     *(short *)((int)puVar8 + 6) =
//                          *(short *)((int)puVar8 + 6) + (ushort)(sampleReadCnt >> 0x15);
//                     bVar16 = 1 < rMixCC;
//                     rMixCC = iVar2;
//                     puVar8 = puVar7;
//                   } while (iVar2 != 0 && bVar16);
//                 }
//               }
//             }
//           }
//         }
//         if ((int)rfreq < 0x1780) {
//           iVar2 = *puVar10;
//           rsrc = (mm_mas_gba_sample *)puVar10[1];
//           puVar10 = puVar10 + 2;
//           rread = rread + iVar2 * 0x1000;
//         }
//         rMixCC = *puVar10;
//         rVolA = puVar10[1];
//         rchan = (mm_mixer_channel *)puVar10[2];
//         puVar8 = puVar10 + 3;
//         if ((int)rread < (int)(rsrc[-1].length * 0x1000)) goto .mpm_channelfinished;
//         if ((int)rsrc[-1].loop_length < 0) {
//           rchan->src = 0x80000000;
//           rsrc = (mm_mas_gba_sample *)&mpm_nullsample;
//           rfreq = 0;
//           rread = 0;
//           if (rMixCC < 1) goto .mpm_channelfinished;
//           iVar5 = 0;
//           sampleReadCnt = 0;
//           goto .mpm_mix_short;
//         }
//         rread = rread + rsrc[-1].loop_length * -0x1000;
//         if (0 < rMixCC) goto .mpm_remix_test;
// .mpm_channelfinished:
//       } while (rMixCC != 0);
//       rchan->read = rread;
//     }
//     rchan = rchan + 1;
//     if (rchan == mm_mixch_end) {
//       rfreq = (rVolA & 0xffff) >> 1;
//       iVar2 = *puVar8 + -1;
//       puVar7 = mm_mixbuffer;
//       puVar3 = mp_writepos;
//       puVar4 = mp_writepos + mm_mixlen;
//       if (iVar2 != 0 && 0 < (int)*puVar8) {
//         do {
//           puVar8 = puVar7 + 1;
//           iVar5 = (int)((*puVar7 + rfreq * -8) * 0x10000) >> 0x13;
//           if (iVar5 < -0x80) {
//             iVar5 = -0x80;
//           }
//           if (0x7f < iVar5) {
//             iVar5 = 0x7f;
//           }
//           iVar6 = (int)((*puVar7 >> 0x10) + rfreq * -8) >> 3;
//           if (iVar6 < -0x80) {
//             iVar6 = -0x80;
//           }
//           if (0x7f < iVar6) {
//             iVar6 = 0x7f;
//           }
//           mp_writepos = puVar3 + 1;
//           *puVar3 = (ushort)iVar5 & 0xff | (ushort)(iVar6 << 8);
//           puVar7 = puVar7 + 2;
//           iVar5 = (int)((*puVar8 + (rVolA >> 0x11) * -8) * 0x10000) >> 0x13;
//           if (iVar5 < -0x80) {
//             iVar5 = -0x80;
//           }
//           if (0x7f < iVar5) {
//             iVar5 = 0x7f;
//           }
//           iVar6 = (int)((*puVar8 >> 0x10) + (rVolA >> 0x11) * -8) >> 3;
//           if (iVar6 < -0x80) {
//             iVar6 = -0x80;
//           }
//           if (0x7f < iVar6) {
//             iVar6 = 0x7f;
//           }
//           *puVar4 = (ushort)iVar5 & 0xff | (ushort)(iVar6 << 8);
//           iVar5 = iVar2 + -2;
//           bVar16 = 1 < iVar2;
//           puVar3 = mp_writepos;
//           puVar4 = puVar4 + 1;
//           iVar2 = iVar5;
//         } while (iVar5 != 0 && bVar16);
//       }
//       return;
//     }
//   } while( true );
// }

void mmMixerMix(mm_word samples_count)
{
    // exit function if samples == 0
    // it will malfunction.
    if (samples_count == 0) return;

    // r0 = mixbuf
    mm_word *mixbuf = (mm_word *)mm_mixbuffer;
    for (mm_word i = samples_count>>3; i != 0; --i)
    {
        // clear 32 bytes/write
        memset(mixbuf, 0, 32);
        mixbuf += 8;
    }

    // clear remainder
    for (mm_word i = samples_count & 7; i != 0; --i)
    {
        *(mixbuf++) = 0;
    }

    // BEGIN MIXING ROUTINE

    mm_mixer_channel *rchan = mm_mix_channels; // (r12)
    // r11 = 0 // volume addition
    int rsrc = (int)rchan->src; // (r10)
    mm_word rfreq = rchan->freq; // (r9)
    if (rsrc >= 0 && rfreq != 0)
    {
        // r0 = mm_ratescale
        rfreq = (rfreq * mm_ratescale) >> 14;
        // load mixing buffers
        // rmixb = mm_mixbuffer // (r8)

        // get read position
        // rread = rchan->read; // (r7)

        // calculate volume
        // rvolR = rchan->vol // (r6) volume 0-255
        // r0 = rchan->pan // pan = 0.255
        int rvolL = ((256 - (int)rchan->pan) * (int)rchan->vol) >> 8;
        int rvolA = 0 + rvolL; // add to volume counter
        
    }
}