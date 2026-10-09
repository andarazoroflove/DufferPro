#ifndef GOLF_AUDIO_H
#define GOLF_AUDIO_H

#include <stdint.h>
#include <stdbool.h>
#include "fixed_math.h"

/* Procedural Sound Effect Types */
typedef enum {
    SFX_NONE = 0,
    SFX_SWING_DRIVER = 1,
    SFX_SWING_IRON = 2,
    SFX_SWING_WEDGE = 3,
    SFX_SWING_PUTTER = 4,
    SFX_BALL_BOUNCE = 5,
    SFX_TREE_HIT = 6,
    SFX_ROCK_HIT = 7,
    SFX_WATER_SPLASH = 8,
    SFX_FOOTSTEP_GRASS = 9,
    SFX_FOOTSTEP_SAND = 10,
    SFX_FOOTSTEP_ROUGH = 11,
    SFX_CUP_SINK = 12,
    SFX_SWEETSPOT = 13,
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

