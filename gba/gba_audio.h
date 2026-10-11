#ifndef GBA_AUDIO_H
#define GBA_AUDIO_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    GBA_SFX_NONE = 0,
    GBA_SFX_DRIVER,
    GBA_SFX_IRON,
    GBA_SFX_WEDGE,
    GBA_SFX_PUTTER,
    GBA_SFX_BALL_THUD,
    GBA_SFX_CUP_SINK,
    GBA_SFX_PIN_DING,
    GBA_SFX_BEV_WHISTLE,
    GBA_SFX_GOLFER_BURP,
    GBA_SFX_BOOMSTICK,
    GBA_SFX_GOPHER_CHUCKLE,
    GBA_SFX_GOPHER_GULP,
    GBA_SFX_CARL_EXPLOSION,
    GBA_SFX_WATER_SPLASH,
    GBA_SFX_AMBIENT_WIND,
    GBA_SFX_AMBIENT_WATER
} GbaSfxId;

/* Initialize DirectSound A hardware, Timer 0 (16 kHz), and DMA Channel 1 */
void gba_audio_init(void);

/* Call once per frame (60 FPS) to mix pending voices into circular DMA buffer */
void gba_audio_update(void);

/* Trigger a digitized 8-bit PCM sound effect with volume (0..256) */
void gba_audio_play_sfx(GbaSfxId sfx_id, int volume);

/* Trigger a seamless looping ambient soundscape (wind, water stream) */
void gba_audio_play_ambient(GbaSfxId ambient_id, int volume);

/* Stop all active voices */
void gba_audio_stop_all(void);

#endif /* GBA_AUDIO_H */

