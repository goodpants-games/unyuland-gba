#include <tonc.h>
#include <maxmod.h>
#include "sound.h"
#include "soundbank.h"
#include "soundbank_bin.h"

#include "gfx.h"
#include "log.h"
#include "tonc_irq.h"
#include "gba_util.h"
#include "scenes.h"

// #define MAIN_PROFILE

#define MM_MODCH_COUNT 8
#define MM_MIXCH_COUNT 8

struct
{
    u8 mod_ch[MM_MODCH_COUNT * MM_SIZEOF_MODCH];
    u8 act_ch[MM_MIXCH_COUNT * MM_SIZEOF_ACTCH];
    u8 mix_ch[MM_MIXCH_COUNT * MM_SIZEOF_MIXCH];
    u8 wave[MM_MIXLEN_16KHZ] __attribute__((aligned(4)));
    u8 Filler_useless_data_to_make_sure_oob_memory_writes_of_a_source_i_cannot_ascertain_dont_mess_up_anything_important[512];
} static mm_memory EWRAM_DATA;

// Mixing buffer (globals should go in IWRAM)
// Mixing buffer SHOULD be in IWRAM, otherwise the CPU load
// will _drastially_ increase
__attribute((aligned(4)))
static u8 mm_mixing_buf[MM_MIXLEN_16KHZ];

// Fuck you maxmod documentation. Fuckign hell.
// Shows me code that runs mmFrame in the main loop before VBlankIntrWait. As if
// that's how you were supposed to do it. Guess what. I wasted two days of
// development trying to figure out why my game randomly crashed. Turns out it
// was maxmod writing out of bounds. Then spent the next three days of
// development trying to figure out why the hell maxmod was writing out of
// bounds. The documentation showed me this is how you were supposed to call
// it!! Why would it be wrong? Fucking hell. Turns out there was nothing wrong
// with my code after all. Maxmod is just being stupid. Why did the
// documentation not mention you probably aren't supposed to run mmFrame before
// VBlankIntrWait because if the frame takes too long mmFrame can overlap with
// vblank and wreak havoc. Is the developer aware of that fact. Where is the
// source code for the "GBA examples". What source code. What "GBA examples".
// The fuck?
// Also make mmFrame run in the interrupt instead of in main because why not.
ARM_FUNC static void vbl(void)
{
    mmVBlank();
    REG_IME = 1; // enable nested interrupts
    mmFrame();
}

int main(void)
{
    LOG_INIT();

    irq_init(NULL);
    irq_add(II_VBLANK, vbl);
    irq_add(II_HBLANK, snd_irq_hblank);

    gfx_init();

    mm_gba_system mm_sys = (mm_gba_system)
    {
        .mixing_mode = MM_MIX_16KHZ,
        .mod_channel_count = MM_MODCH_COUNT,
        .mix_channel_count = MM_MIXCH_COUNT,
        .module_channels   = (mm_addr) mm_memory.mod_ch,
        .active_channels   = (mm_addr) mm_memory.act_ch,
        .mixing_channels   = (mm_addr) mm_memory.mix_ch,
        .mixing_memory     = (mm_addr) mm_mixing_buf,
        .wave_memory       = (mm_addr) mm_memory.wave,
        .soundbank         = (mm_addr) soundbank_bin
    };

    mmInit(&mm_sys);
    snd_init();

    scenemgr_init(&scene_desc_menu, 0);

    while (true)
    {
        #ifdef MAIN_PROFILE
        uint frame_len = 0;
        profile_start();
        #endif

        // screen_print(&se_mat[GFX_BG0_INDEX][18][0], "Hello, world!");

        key_poll();

        scenemgr_frame();
        
        #ifdef MAIN_PROFILE
        frame_len = profile_stop();
        LOG_DBG("frame usage: %.1f%%", (float)frame_len / 280896.f * 100.f);
        #endif

        VBlankIntrWait();
        gfx_new_frame();
        snd_frame();
    }

    return 0;
}
