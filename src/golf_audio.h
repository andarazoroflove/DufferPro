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
    SFX_FOOTSTEP_WOOD = 12,
    SFX_CUP_SINK = 13,
    SFX_SWEETSPOT = 14,
    SFX_APPLAUSE_GOLF = 15,
    SFX_CROWD_CHEER = 16,
    SFX_CROWD_OOH = 17,
    SFX_BUILD_PLACE = 18,
    SFX_TERRAIN_SCULPT = 19,
    SFX_BEVERAGE_WHISTLE = 20,
    SFX_GOPHER_CHUCKLE = 21,
    SFX_GOPHER_GULP = 22,
    SFX_CARL_EXPLOSION = 23,
    SFX_CARL_HOSE = 24,
    SFX_SIREN = 25,
    SFX_SHOTGUN = 26,
    SFX_CHAINSAW = 27,
    SFX_ZOMBIE = 28,
    SFX_SEAMONSTER = 29,
    SFX_BURP = 30,
    SFX_GOLFER_JUMP = 31,
    SFX_COUNT
} GolfSfxType;

/* Audio Lifecycle */
bool golf_audio_init(void);
void golf_audio_shutdown(void);

/* Procedural Intro Melody */
void golf_audio_start_intro_music(void);
void golf_audio_stop_intro_music(void);

/* Play one-shot procedural sound effect */
void golf_audio_play_sfx(GolfSfxType sfx);

/* Update dynamic ambient soundscape (called in main loop) */
void golf_audio_update_ambient(fixed wind_speed, int water_proximity_percent);
void golf_audio_update_ambient_pro(fixed wind_speed, int water_proximity_percent, int clubhouse_proximity_percent);

#endif /* GOLF_AUDIO_H */
