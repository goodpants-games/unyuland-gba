#include "sound_table.h"

#define SNDCMD_END 0x0000

#define SNDCMD_CH_SQR1  0
#define SNDCMD_CH_SQR2  1
#define SNDCMD_CH_WAVE  2
#define SNDCMD_CH_NOISE 3

#define SNDCMD_CH_SQR_DUTY2     0
#define SNDCMD_CH_SQR_DUTY4     1
#define SNDCMD_CH_SQR_DUTY8     2
#define SNDCMD_CH_WAVE_TRIANGLE 0
#define SNDCMD_CH_WAVE_NOISE    1

#define SNDCMD_PRIO_PLAYER  1
#define SNDCMD_PRIO_DEFAULT 0

#define SNDCMD_SET_CH(idx, mode) (0x0001 | ((idx) << 4) | ((mode) << 6))
#define SNDCMD_ARP2(ofs) (0x0002 | ((ofs) << 4))
#define SNDCMD_ARP3(ofs1, ofs2) (0x0003 | ((ofs1) << 4) | ((ofs2 << 8)))
#define SNDCMD_PITCH(i, key) (0x0004 | ((key) << 4) | ((i) << 12))
#define SNDCMD_PLAY_ENV(len, start, end) (0x0005 | ((len) << 4) | ((start) << 10) | ((end) << 13))
#define SNDCMD_PLAY_ENV_SWP(len, start, end) (0x0006 | ((len) << 4) | ((start) << 10) | ((end) << 13))
#define SNDCMD_PLAY(len) SNDCMD_PLAY_ENV(len, 7, 7)
#define SNDCMD_PLAY_SWP(len) SNDCMD_PLAY_ENV_SWP(len, 7, 7)
#define SNDCMD_VIBRATO(spd, pow) (0x0007 | ((spd) << 4) | ((pow) << 10))
#define SNDCMD_PRIO(prio) (0x0008 | ((prio) << 4))

#define SNDCMD_KEY(key, octave) (SND_KEY_##key + ((octave) - 2) * 12)

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

static const snd_cmd sound_player_death[] = {
    SNDCMD_PRIO(SNDCMD_PRIO_PLAYER),
    SNDCMD_SET_CH(SNDCMD_CH_WAVE, SNDCMD_CH_WAVE_TRIANGLE),
    SNDCMD_PITCH(0, SNDCMD_KEY(Db, 6)),
    SNDCMD_PITCH(1, SNDCMD_KEY(C, 5)),
    SNDCMD_ARP2(10),
    SNDCMD_PLAY_SWP(20),
    SNDCMD_END,
};

static const snd_cmd sound_checkpoint[] = {
    SNDCMD_PRIO(SNDCMD_PRIO_PLAYER),
    SNDCMD_SET_CH(SNDCMD_CH_WAVE, SNDCMD_CH_WAVE_TRIANGLE),
    SNDCMD_PITCH(0, SNDCMD_KEY(C, 4)),
    SNDCMD_PITCH(1, SNDCMD_KEY(A, 5)),
    SNDCMD_ARP2(4),
    SNDCMD_PLAY_SWP(16),
    SNDCMD_END,
};

static const snd_cmd sound_enemy_spit[] = {
    SNDCMD_PRIO(SNDCMD_PRIO_DEFAULT),
    SNDCMD_SET_CH(SNDCMD_CH_SQR2, SNDCMD_CH_SQR_DUTY2),
    SNDCMD_PITCH(0, SNDCMD_KEY(C, 3)),
    SNDCMD_PITCH(1, SNDCMD_KEY(A, 2)),
    SNDCMD_PLAY_SWP(3),
    SNDCMD_PITCH(1, SNDCMD_KEY(A, 4)),
    SNDCMD_PLAY_SWP(3),
    SNDCMD_END,
};

static const snd_cmd sound_enemy_hurt[] = {
    SNDCMD_PRIO(SNDCMD_PRIO_DEFAULT),
    SNDCMD_SET_CH(SNDCMD_CH_SQR2, SNDCMD_CH_SQR_DUTY4),
    SNDCMD_PITCH(0, SNDCMD_KEY(E, 3)),
    SNDCMD_PITCH(1, SNDCMD_KEY(A, 2)),
    SNDCMD_PLAY_SWP(6),
    SNDCMD_END
};

static const snd_cmd sound_enemy_death[] = {
    SNDCMD_PRIO(SNDCMD_PRIO_PLAYER),
    SNDCMD_SET_CH(SNDCMD_CH_SQR2, SNDCMD_CH_SQR_DUTY4),
    SNDCMD_PITCH(0, SNDCMD_KEY(Gb, 3)),
    SNDCMD_PITCH(1, SNDCMD_KEY(A, 2)),
    SNDCMD_PLAY_SWP(6),
    SNDCMD_PITCH(1, SNDCMD_KEY(G, 2)),
    SNDCMD_PLAY_SWP(8),
    SNDCMD_END,
};

static const snd_cmd sound_boss_hit[] = {
    SNDCMD_PRIO(SNDCMD_PRIO_DEFAULT),
    SNDCMD_SET_CH(SNDCMD_CH_SQR2, SNDCMD_CH_SQR_DUTY2),
    SNDCMD_PITCH(0, SNDCMD_KEY(A,3)),
    SNDCMD_PITCH(1, SNDCMD_KEY(C,2)),
    SNDCMD_PLAY_SWP(6),
    SNDCMD_PITCH(0, SNDCMD_KEY(C,3)),
    SNDCMD_PITCH(1, SNDCMD_KEY(C,2)),
    SNDCMD_PLAY_SWP(6),
    SNDCMD_END
};

static const snd_cmd sound_boss_land[] = {
    SNDCMD_PRIO(SNDCMD_PRIO_PLAYER),
    SNDCMD_SET_CH(SNDCMD_CH_SQR2, SNDCMD_CH_SQR_DUTY2),
    SNDCMD_PITCH(0, SNDCMD_KEY(A,4)),
    SNDCMD_PITCH(1, SNDCMD_KEY(C,2)),
    SNDCMD_PLAY_ENV_SWP(4, 6, 6),
    SNDCMD_END
};

static const snd_cmd sound_boss_dash_windup[] = {
    SNDCMD_PRIO(SNDCMD_PRIO_DEFAULT),
    SNDCMD_SET_CH(SNDCMD_CH_SQR2, SNDCMD_CH_SQR_DUTY2),
    SNDCMD_PITCH(0, SNDCMD_KEY(C,2)),
    SNDCMD_PITCH(1, SNDCMD_KEY(A,4)),
    SNDCMD_ARP2(7),
    SNDCMD_PLAY_SWP(40),
    SNDCMD_END
};

static const snd_cmd sound_boss_dash[] = {
    SNDCMD_PRIO(SNDCMD_PRIO_DEFAULT),
    SNDCMD_SET_CH(SNDCMD_CH_SQR2, SNDCMD_CH_SQR_DUTY2),
    SNDCMD_PITCH(0, SNDCMD_KEY(A,4)),
    SNDCMD_PITCH(1, SNDCMD_KEY(C,2)),
    SNDCMD_ARP3(7, 12),
    SNDCMD_PLAY_ENV_SWP(40, 7, 2),
    SNDCMD_END
};

static const snd_cmd sound_boss_jump_windup[] = {
    SNDCMD_PRIO(SNDCMD_PRIO_DEFAULT),
    SNDCMD_SET_CH(SNDCMD_CH_SQR2, SNDCMD_CH_SQR_DUTY4),
    SNDCMD_PITCH(0, SNDCMD_KEY(D,2)),
    SNDCMD_PITCH(1, SNDCMD_KEY(C,4)),
    SNDCMD_ARP2(12),
    SNDCMD_PLAY_SWP(63),
    SNDCMD_END
};

static const snd_cmd sound_boss_jump[] = {
    SNDCMD_PRIO(SNDCMD_PRIO_DEFAULT),
    SNDCMD_SET_CH(SNDCMD_CH_SQR2, SNDCMD_CH_SQR_DUTY2),
    SNDCMD_PITCH(0, SNDCMD_KEY(A,4)),
    SNDCMD_PITCH(1, SNDCMD_KEY(E,2)),
    // SNDCMD_ARP3(7, 12),
    SNDCMD_VIBRATO(5, 6),
    SNDCMD_PLAY_ENV_SWP(40, 7, 2),
    SNDCMD_END
};

static const snd_cmd sound_spring[] = {
    SNDCMD_PRIO(SNDCMD_PRIO_DEFAULT),
    SNDCMD_SET_CH(SNDCMD_CH_SQR2, SNDCMD_CH_SQR_DUTY8),
    SNDCMD_PITCH(0, SNDCMD_KEY(Eb, 4)),
    SNDCMD_PITCH(1, SNDCMD_KEY(Gb, 5)),
    SNDCMD_VIBRATO(5, 6),
    SNDCMD_PLAY_SWP(12),
    SNDCMD_PLAY_ENV(24, 7, 0),
    SNDCMD_END
};

static const snd_cmd sound_platform_place[] = {
    SNDCMD_PRIO(SNDCMD_PRIO_PLAYER),
    SNDCMD_SET_CH(SNDCMD_CH_WAVE, SNDCMD_CH_WAVE_NOISE),
    SNDCMD_PITCH(0, SNDCMD_KEY(C, 6)),
    SNDCMD_PITCH(1, SNDCMD_KEY(C, 2)),
    SNDCMD_PLAY_SWP(4),
    SNDCMD_END,
};

static const snd_cmd sound_menu_move[] = {
    SNDCMD_PRIO(SNDCMD_PRIO_DEFAULT),
    SNDCMD_SET_CH(SNDCMD_CH_SQR2, SNDCMD_CH_SQR_DUTY2),
    SNDCMD_PITCH(0, SNDCMD_KEY(G, 6)),
    SNDCMD_PLAY_ENV(3, 7, 0),
    SNDCMD_END,
};

static const snd_cmd sound_menu_select[] = {
    SNDCMD_PRIO(SNDCMD_PRIO_DEFAULT),
    SNDCMD_SET_CH(SNDCMD_CH_WAVE, SNDCMD_CH_WAVE_TRIANGLE),
    SNDCMD_PITCH(0, SNDCMD_KEY(A, 5)),
    SNDCMD_ARP2(4),
    SNDCMD_PLAY(4),
    SNDCMD_END,
};

static const snd_cmd sound_menu_back[] = {
    SNDCMD_PRIO(SNDCMD_PRIO_DEFAULT),
    SNDCMD_SET_CH(SNDCMD_CH_WAVE, SNDCMD_CH_WAVE_TRIANGLE),
    SNDCMD_PITCH(0, SNDCMD_KEY(A, 5)),
    SNDCMD_PLAY(2),
    SNDCMD_PITCH(0, SNDCMD_KEY(E, 5)),
    SNDCMD_PLAY(2),
    SNDCMD_END,
};

void snd_init_table(void)
{
    snd_sounds[SND_ID_PLAYER_JUMP]      = sound_player_jump;
    snd_sounds[SND_ID_PLAYER_SHOOT]     = sound_player_shoot;
    snd_sounds[SND_ID_PLAYER_SPIT]      = sound_player_spit;
    snd_sounds[SND_ID_PLATFORM_PLACE]   = sound_platform_place;
    snd_sounds[SND_ID_PLAYER_DIE]       = sound_player_death;
    snd_sounds[SND_ID_CHECKPOINT]       = sound_checkpoint;
    snd_sounds[SND_ID_SPRING]           = sound_spring;
    snd_sounds[SND_ID_ENEMY_SPIT]       = sound_enemy_spit;
    snd_sounds[SND_ID_ENEMY_HURT]       = sound_enemy_hurt;
    snd_sounds[SND_ID_ENEMY_DIE]        = sound_enemy_death;
    snd_sounds[SND_ID_BOSS_HIT]         = sound_boss_hit;
    snd_sounds[SND_ID_BOSS_LAND]        = sound_boss_land;
    snd_sounds[SND_ID_BOSS_DASH_WINDUP] = sound_boss_dash_windup;
    snd_sounds[SND_ID_BOSS_DASH]        = sound_boss_dash;
    snd_sounds[SND_ID_BOSS_JUMP_WINDUP] = sound_boss_jump_windup;
    snd_sounds[SND_ID_BOSS_JUMP]        = sound_boss_jump;
    snd_sounds[SND_ID_BOSS_DIE]         = NULL;
    snd_sounds[SND_ID_MENU_MOVE]        = sound_menu_move;
    snd_sounds[SND_ID_MENU_SELECT]      = sound_menu_select;
    snd_sounds[SND_ID_MENU_BACK]        = sound_menu_back;
}