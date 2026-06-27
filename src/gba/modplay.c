#include <modplay.h>
#include <tonc.h>
#include <maxmod.h>
#include <platutil.h>

#include <data/mm_soundbank.h>
#include <data/mm_soundbank_bin.h>

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

static mplay_event_handler_f s_ev_handler = NULL;


void mplay_init(void)
{
    mm_gba_system mm_sys = (mm_gba_system)
    {
        .mixing_mode       = MM_MIX_16KHZ,
        .mod_channel_count = MM_MODCH_COUNT,
        .mix_channel_count = MM_MIXCH_COUNT,
        .module_channels   = (mm_addr) mm_memory.mod_ch,
        .active_channels   = (mm_addr) mm_memory.act_ch,
        .mixing_channels   = (mm_addr) mm_memory.mix_ch,
        .mixing_memory     = (mm_addr) mm_mixing_buf,
        .wave_memory       = (mm_addr) mm_memory.wave,
        .soundbank         = (mm_addr) mm_soundbank_bin
    };

    mmInit(&mm_sys);
}

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
ARM_FUNC void mplay_vblank_handler(void)
{
    mmVBlank();
    REG_IME = 1; // enable nested interrupts
    mmFrame();
}

void mplay_start(mp_uint module_id, mp_bool loop)
{
    mmStart(module_id, loop ? MM_PLAY_LOOP : MM_PLAY_ONCE);
}

void mplay_pause(void)
{
    mmPause();
}

void mplay_resume(void)
{
    mmResume();
}

void mplay_stop(void)
{
    mmStop();
}

bool mplay_is_active(void)
{
    return mmActive();
}

void mplay_set_volume(mp_uint volume)
{
    mmSetModuleVolume(volume);
}

void mplay_sub_start(mp_uint module_id)
{
    mmJingle(module_id);
}

void mplay_set_sub_volume(mp_uint volume)
{
    mmSetJingleVolume(volume);
}

static mm_word mm_event_handler(mm_word msg, mm_word param)
{
    switch (msg)
    {
    case MMCB_SONGFINISHED:
        s_ev_handler(MP_MSG_SONG_FINISHED, param);
        break;
    }

    return 0;
}

void mplay_set_event_handler(mplay_event_handler_f handler)
{
    s_ev_handler = handler;

    if (handler)
        mmSetEventHandler(mm_event_handler);
    else
        mmSetEventHandler(NULL);
}