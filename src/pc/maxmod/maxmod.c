#include <maxmod.h>
 
void mmInitDefault( mm_addr soundbank, mm_word number_of_channels ) {}
 
void mmInit( mm_gba_system* setup ) {}

void mmSetEventHandler( mm_callback handler ) {}
 
void mmFrame( void ) {}



/****************************************************************************
 *
 * Module Playback
 *
 ****************************************************************************/





void mmStart( mm_word id, mm_pmode mode ) {}
 
void mmPause( void ) {}
 
void mmResume( void ) {}

void mmStop( void ) {}

mm_word mmGetPositionTick( void ) { return 0; }

mm_word mmGetPositionRow( void ) { return 0; }

mm_word mmGetPosition( void ) { return 0; }

void mmPosition( mm_word position ) {}

int  mmActive( void ) { return 0; }
 
void mmJingle( mm_word module_ID ) {}

int  mmActiveSub( void ) { return 0; }

void mmSetModuleVolume( mm_word volume ) {}
void mmSetJingleVolume( mm_word volume ) {}

void mmSetModuleTempo( mm_word tempo ) {}
 
void mmSetModulePitch( mm_word pitch ) {}
 
void mmPlayModule( mm_word address, mm_word mode, mm_word layer ) {}



/****************************************************************************
 *
 * Sound Effects
 *
 ****************************************************************************/


mm_sfxhand mmEffect( mm_word sample_ID ) { return 0; }

mm_sfxhand mmEffectEx( mm_sound_effect* sound ) { return 0; }
 
void mmEffectVolume( mm_sfxhand handle, mm_word volume ) {}
 
void mmEffectPanning( mm_sfxhand handle, mm_byte panning ) {}
 
void mmEffectRate( mm_sfxhand handle, mm_word rate ) {}
 
void mmEffectScaleRate( mm_sfxhand handle, mm_word factor ) {}

mm_bool mmEffectActive( mm_sfxhand handle ) { return 0; }

void mmEffectCancel( mm_sfxhand handle ) {}
 
void mmEffectRelease( mm_sfxhand handle ) {}

void mmSetEffectsVolume( mm_word volume ) {}
 
void mmEffectCancelAll();



#ifdef __cplusplus
}
#endif

/****************************************************************************
 * etc...
 ****************************************************************************/

mm_byte	mp_mix_seg;			// current mixing segment
mm_word	mp_writepos;		// mixer's write position
