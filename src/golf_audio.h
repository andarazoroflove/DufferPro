#ifndef GOLF_AUDIO_H
#define GOLF_AUDIO_H

#include <stdint.h>
#include <stdbool.h>
#include "fixed_math.h"

/* Procedural Sound Effect Types */
typedef enum {
    SFX_SWING_DRIVER = 0,
    SFX_SWING_IRON,
    SFX_SWING_WEDGE,
    SFX_SWING_PUTTER,
    SFX_BALL_BOUNCE,
    SFX_TREE_HIT,
    SFX_ROCK_HIT,
    SFX_WATER_SPLASH,
    SFX_FOOTSTEP_GRASS,
    SFX_FOOTSTEP_SAND,
    SFX_FOOTSTEP_ROUGH,
    SFX_CUP_SINK,
    SFX_SWEETSPOT,
    SFX_COUNT
} GolfSfxType;

/* Audio Lifecycle */
bool golf_audio_init(void);
void golf_audio_shutdown(void);

/* Play one-shot procedural sound effect */
void golf_audio_play_sfx(GolfSfxType sfx);

/* Update dynamic ambient soundscape (called in main loop) */
void golf_audio_update_ambient(fixed wind_speed, int water_proximity_percent);

#endif /* GOLF_AUDIO_H */

