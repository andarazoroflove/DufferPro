#include "golf_audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#if defined(USE_WIN32_GDI)
/* Fallback dummy audio for GDI build */
bool golf_audio_init(void) { return true; }
void golf_audio_shutdown(void) {}
void golf_audio_play_sfx(GolfSfxType sfx) { (void)sfx; }
void golf_audio_start_intro_music(void) {}
void golf_audio_stop_intro_music(void) {}
void golf_audio_update_ambient(fixed wind_speed, int water_proximity_percent) { (void)wind_speed; (void)water_proximity_percent; }
void golf_audio_update_ambient_pro(fixed wind_speed, int water_proximity_percent, int clubhouse_proximity_percent) { (void)wind_speed; (void)water_proximity_percent; (void)clubhouse_proximity_percent; }
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>

#define AUDIO_SAMPLE_RATE    44100
#define AUDIO_CHANNELS       2
#define AUDIO_BUFFER_SAMPLES 1024
#define MAX_ACTIVE_VOICES    24

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Voice descriptor for polyphonic SFX */
typedef struct {
    bool active;
    GolfSfxType type;
    uint32_t sample_pos;
    uint32_t total_samples;
    float volume;
    float pan; /* -1.0 (left) to +1.0 (right) */
} SfxVoice;

/* Fast PRNG for audio noise synthesis */
static uint32_t s_audio_rand = 54321;
static inline float audio_noise(void) {
    s_audio_rand = s_audio_rand * 1664525 + 1013904223;
    return ((float)(s_audio_rand & 0xFFFF) / 32768.0f) - 1.0f;
}

/* Ambient System State */
static float s_water_target_vol = 0.0f;
static float s_water_current_vol = 0.0f;
static float s_water_phase1 = 0.0f;
static float s_water_phase2 = 0.0f;

static float s_clubhouse_target_vol = 0.0f;
static float s_clubhouse_current_vol = 0.0f;

/* Bird Chirp Generator */
static int32_t s_bird_timer_samples = 44100 * 2; /* First chirp in 2s */
static bool s_bird_active = false;
static uint32_t s_bird_pos = 0;
static uint32_t s_bird_dur = 0;
static float s_bird_base_freq = 2800.0f;
static float s_bird_pan = 0.0f;
static float s_bird_phase = 0.0f;

/* Polyphonic SFX Pool */
static SfxVoice s_voices[MAX_ACTIVE_VOICES];
static SDL_AudioDeviceID s_audio_device = 0;
static bool s_audio_ready = false;

/* Procedural Intro Melody Synthesizer: Instrumental "I'm Alright (Theme from Caddyshack)" (128 BPM) */
static bool s_intro_music_active = false;
static float s_intro_music_vol = 0.0f;
static float s_intro_music_target_vol = 0.0f;
static uint32_t s_music_sample_clock = 0;
static float s_m_bass_phase = 0.0f;
static float s_m_chord_phase[3] = { 0.0f, 0.0f, 0.0f };
static float s_m_lead_phase = 0.0f;
static float s_m_kick_phase = 0.0f;

/* Chord triads for 8 measures: Em, D, Em, C, D, Em, C, D */
static const float s_caddy_chords[8][3] = {
    { 329.63f, 392.00f, 493.88f }, /* 0: Em   (E4, G4, B4) */
    { 293.66f, 369.99f, 440.00f }, /* 1: D    (D4, F#4, A4) */
    { 329.63f, 392.00f, 493.88f }, /* 2: Em   (E4, G4, B4) */
    { 261.63f, 329.63f, 392.00f }, /* 3: C    (C4, E4, G4) */
    { 293.66f, 369.99f, 440.00f }, /* 4: D    (D4, F#4, A4) */
    { 329.63f, 392.00f, 493.88f }, /* 5: Em   (E4, G4, B4) */
    { 261.63f, 329.63f, 392.00f }, /* 6: C    (C4, E4, G4) */
    { 293.66f, 369.99f, 440.00f }  /* 7: D    (D4, F#4, A4) */
};

/* 128-step chord stab trigger mask (stabs on syncopated 16th beats) */
static const uint8_t s_caddy_chord_stabs[128] = {
    /* Meas 1 */ 0,0,1,0, 0,0,1,0, 1,0,0,1, 0,0,1,0,
    /* Meas 2 */ 0,0,1,0, 0,0,1,0, 1,0,0,1, 0,0,1,0,
    /* Meas 3 */ 0,0,1,0, 0,0,1,0, 1,0,0,1, 0,0,1,0,
    /* Meas 4 */ 0,0,1,0, 0,0,1,0, 1,0,0,1, 0,0,1,0,
    /* Meas 5 */ 1,0,0,1, 0,0,1,0, 0,1,0,1, 0,0,1,0,
    /* Meas 6 */ 1,0,0,1, 0,0,1,0, 0,1,0,1, 0,0,1,0,
    /* Meas 7 */ 0,0,1,0, 0,0,1,0, 1,0,0,1, 0,0,1,0,
    /* Meas 8 */ 0,0,1,0, 0,0,1,0, 1,0,1,0, 1,1,1,1
};

/* Driving 80s Slap/Synth Bassline in E minor (128 steps) */
static const float s_caddy_bass[128] = {
    /* Meas 1: E2 . E2 . E2 . D3 . E2 . G2 . A2 . E2 D2 */
    82.41f, 0.0f, 82.41f, 0.0f,  82.41f, 0.0f, 146.83f, 0.0f,
    82.41f, 0.0f, 98.00f, 0.0f, 110.00f, 0.0f, 82.41f, 146.83f,
    /* Meas 2: E2 . E2 . E2 . D3 . B2 . A2 . G2 . E2 . */
    82.41f, 0.0f, 82.41f, 0.0f,  82.41f, 0.0f, 146.83f, 0.0f,
    123.47f,0.0f, 110.00f,0.0f,  98.00f, 0.0f, 82.41f, 0.0f,
    /* Meas 3: E2 . E2 . E2 . D3 . E2 . G2 . A2 . E2 D2 */
    82.41f, 0.0f, 82.41f, 0.0f,  82.41f, 0.0f, 146.83f, 0.0f,
    82.41f, 0.0f, 98.00f, 0.0f, 110.00f, 0.0f, 82.41f, 146.83f,
    /* Meas 4: C3 . C3 . C3 . C3 . D3 . D3 . D3 . B2 . */
    130.81f,0.0f, 130.81f,0.0f, 130.81f,0.0f, 130.81f,0.0f,
    146.83f,0.0f, 146.83f,0.0f, 146.83f,0.0f, 123.47f,0.0f,
    /* Meas 5: E2 . E2 . E2 . E3 . G2 . A2 . B2 . D3 . (Chorus) */
    82.41f, 0.0f, 82.41f, 0.0f,  82.41f, 0.0f, 164.81f, 0.0f,
    98.00f, 0.0f, 110.00f,0.0f, 123.47f,0.0f, 146.83f, 0.0f,
    /* Meas 6: E2 . E2 . E2 . E3 . B2 . A2 . G2 . E2 . */
    82.41f, 0.0f, 82.41f, 0.0f,  82.41f, 0.0f, 164.81f, 0.0f,
    123.47f,0.0f, 110.00f,0.0f,  98.00f, 0.0f, 82.41f, 0.0f,
    /* Meas 7: C3 . C3 . C3 . C3 . D3 . D3 . D3 . D3 . */
    130.81f,0.0f, 130.81f,0.0f, 130.81f,0.0f, 130.81f,0.0f,
    146.83f,0.0f, 146.83f,0.0f, 146.83f,0.0f, 146.83f,0.0f,
    /* Meas 8: E2 . E2 . G2 . A2 . B2 . A2 . G2 . D2 . (Turnaround) */
    82.41f, 0.0f, 82.41f, 0.0f,  98.00f, 0.0f, 110.00f, 0.0f,
    123.47f,0.0f, 110.00f,0.0f,  98.00f, 0.0f, 73.42f,  0.0f
};

/* Unmistakable Lead Synth Hook: "I'm Alright (Theme from Caddyshack)" (128 steps) */
static const float s_caddy_lead[128] = {
    /* Meas 1: "I'm alright... nobody worry 'bout me" */
    493.88f, 0.0f, 493.88f, 0.0f,  493.88f, 0.0f, 440.00f, 0.0f,
    392.00f, 0.0f, 329.63f, 0.0f,  0.0f,    0.0f, 0.0f,    0.0f,
    /* Meas 2: "Nobody worry 'bout me..." */
    0.0f,    0.0f, 329.63f, 0.0f,  392.00f, 0.0f, 440.00f, 0.0f,
    392.00f, 0.0f, 329.63f, 0.0f,  0.0f,    0.0f, 0.0f,    0.0f,
    /* Meas 3: "Why you got to give me a fight..." */
    493.88f, 0.0f, 493.88f, 0.0f,  493.88f, 0.0f, 440.00f, 0.0f,
    392.00f, 0.0f, 329.63f, 0.0f,  0.0f,    0.0f, 0.0f,    0.0f,
    /* Meas 4: "Can't you just let it be..." */
    329.63f, 0.0f, 392.00f, 0.0f,  440.00f, 0.0f, 493.88f, 0.0f,
    440.00f, 0.0f, 392.00f, 0.0f,  329.63f, 0.0f, 0.0f,    0.0f,
    /* Meas 5: "I'm al-right!" (Chorus Hook!) */
    659.25f, 659.25f, 659.25f, 0.0f,  587.33f, 0.0f, 493.88f, 493.88f,
    493.88f, 0.0f,    0.0f,    0.0f,  0.0f,    0.0f, 0.0f,    0.0f,
    /* Meas 6: "I'm al-right!" */
    659.25f, 659.25f, 659.25f, 0.0f,  587.33f, 0.0f, 493.88f, 493.88f,
    493.88f, 0.0f,    0.0f,    0.0f,  0.0f,    0.0f, 0.0f,    0.0f,
    /* Meas 7: "Nobody worry 'bout me..." */
    440.00f, 0.0f, 392.00f, 0.0f,  440.00f, 0.0f, 493.88f, 0.0f,
    392.00f, 0.0f, 329.63f, 0.0f,  293.66f, 0.0f, 329.63f, 0.0f,
    /* Meas 8: Signature synth turnaround flourish! */
    329.63f, 0.0f, 392.00f, 0.0f,  440.00f, 0.0f, 493.88f, 0.0f,
    587.33f, 0.0f, 493.88f, 0.0f,  440.00f, 0.0f, 392.00f, 0.0f
};

/* Synthesis helper for single SFX sample */
static float synthesize_sfx_sample(SfxVoice *v) {
    float t = (float)v->sample_pos / (float)AUDIO_SAMPLE_RATE;
    float progress = (float)v->sample_pos / (float)v->total_samples;
    float out = 0.0f;
    
    switch (v->type) {
        case SFX_SWING_DRIVER: {
            /* Aerodynamic club whoosh transitioning into explosive titanium face crack and sub-thump */
            float whoosh = 0.0f;
            if (t < 0.07f) {
                float w_env = sinf((t / 0.07f) * (float)M_PI);
                whoosh = audio_noise() * w_env * 0.75f;
            }
            float hit_t = (t >= 0.035f) ? (t - 0.035f) : 0.0f;
            float hit_env = expf(-hit_t * 14.0f);
            float freq = 175.0f * expf(-hit_t * 28.0f) + 42.0f;
            float thump = sinf(2.0f * (float)M_PI * freq * hit_t) * hit_env * 0.95f;
            float crack_env = (hit_t < 0.07f) ? expf(-hit_t * 35.0f) : 0.0f;
            float crack = audio_noise() * crack_env * 1.35f;
            out = (whoosh * 0.6f + thump * 0.85f + crack * 1.0f) * v->volume;
            break;
        }
        
        case SFX_SWING_IRON: {
            /* Club swoosh into crisp metallic steel ping (920 Hz & 2760 Hz) and firm impact snap */
            float whoosh = 0.0f;
            if (t < 0.05f) {
                whoosh = audio_noise() * sinf((t / 0.05f) * (float)M_PI) * 0.55f;
            }
            float hit_t = (t >= 0.025f) ? (t - 0.025f) : 0.0f;
            float hit_env = expf(-hit_t * 16.0f);
            float ping = (sinf(2.0f * (float)M_PI * 920.0f * hit_t) * 0.75f +
                          sinf(2.0f * (float)M_PI * 2760.0f * hit_t) * 0.35f) * hit_env;
            float snap = audio_noise() * expf(-hit_t * 45.0f) * 1.0f;
            out = (whoosh * 0.5f + ping * 0.85f + snap * 0.9f) * v->volume;
            break;
        }
        
        case SFX_SWING_WEDGE: {
            /* Aerodynamic swish into crisp turf slice and firm 250 Hz ball strike */
            float whoosh = 0.0f;
            if (t < 0.05f) {
                whoosh = audio_noise() * sinf((t / 0.05f) * (float)M_PI) * 0.6f;
            }
            float hit_t = (t >= 0.02f) ? (t - 0.02f) : 0.0f;
            float thump = sinf(2.0f * (float)M_PI * 250.0f * hit_t) * expf(-hit_t * 18.0f) * 0.75f;
            float turf = audio_noise() * expf(-hit_t * 32.0f) * 1.05f;
            out = (whoosh * 0.5f + thump * 0.75f + turf * 0.85f) * v->volume;
            break;
        }
        
        case SFX_SWING_PUTTER: {
            /* Putting stroke swoosh into rich hollow composite tock */
            float whoosh = 0.0f;
            if (t < 0.04f) {
                whoosh = audio_noise() * sinf((t / 0.04f) * (float)M_PI) * 0.35f;
            }
            float hit_t = (t >= 0.015f) ? (t - 0.015f) : 0.0f;
            float env = expf(-hit_t * 22.0f);
            float tock = sinf(2.0f * (float)M_PI * 340.0f * hit_t) * env * 0.9f;
            float sub = sinf(2.0f * (float)M_PI * 170.0f * hit_t) * env * 0.55f;
            float tap = audio_noise() * expf(-hit_t * 40.0f) * 0.5f;
            out = (whoosh * 0.4f + tock + sub + tap) * v->volume;
            break;
        }
        
        case SFX_BALL_BOUNCE: {
            /* Ground impact thud */
            float env = expf(-progress * 22.0f);
            float thud = sinf(2.0f * (float)M_PI * 110.0f * t) * env;
            float tap = audio_noise() * expf(-progress * 30.0f) * 0.4f;
            out = (thud + tap) * v->volume * 0.7f;
            break;
        }
        
        case SFX_TREE_HIT: {
            /* Wood crack + leafy branch rustle */
            float wood_env = (t < 0.05f) ? (1.0f - t / 0.05f) : 0.0f;
            float wood = (sinf(2.0f * (float)M_PI * 180.0f * t) + audio_noise() * 0.7f) * wood_env;
            float leaf_env = expf(-progress * 6.0f);
            float leaf = audio_noise() * leaf_env * 0.45f;
            out = (wood * 0.8f + leaf) * v->volume;
            break;
        }
        
        case SFX_ROCK_HIT: {
            /* Heavy stone clang */
            float env = expf(-progress * 12.0f);
            float clang = (sinf(2.0f * (float)M_PI * 1320.0f * t) * 0.6f +
                           sinf(2.0f * (float)M_PI * 440.0f * t) * 0.4f) * env;
            float snap = audio_noise() * expf(-progress * 25.0f) * 0.6f;
            out = (clang + snap) * v->volume * 0.85f;
            break;
        }
        
        case SFX_WATER_SPLASH: {
            /* Water splash with descending bubble chirp */
            float env = expf(-progress * 4.5f);
            float bubble_freq = 650.0f * expf(-t * 6.0f) + 180.0f;
            float bubble = sinf(2.0f * (float)M_PI * bubble_freq * t) * env * 0.5f;
            float spray = audio_noise() * env * 0.65f;
            out = (bubble + spray) * v->volume;
            break;
        }
        
        case SFX_FOOTSTEP_GRASS: {
            /* Soft, crisp grass step */
            float env = expf(-progress * 18.0f);
            float rustle = audio_noise() * env * 0.45f;
            float step_sub = sinf(2.0f * (float)M_PI * 90.0f * t) * env * 0.35f;
            out = (rustle + step_sub) * v->volume * 0.5f;
            break;
        }
        
        case SFX_FOOTSTEP_SAND: {
            /* Gritty sand scuff */
            float env = expf(-progress * 14.0f);
            float scuff = audio_noise() * env * 0.6f;
            out = scuff * v->volume * 0.55f;
            break;
        }
        
        case SFX_FOOTSTEP_ROUGH: {
            /* Heather / heavy rough grass brush */
            float env = expf(-progress * 12.0f);
            float brush = audio_noise() * env * 0.55f;
            float low = sinf(2.0f * (float)M_PI * 75.0f * t) * env * 0.4f;
            out = (brush + low) * v->volume * 0.55f;
            break;
        }
        
        case SFX_FOOTSTEP_WOOD: {
            /* Hollow oak plank thud + gentle creak on bridge */
            float env = expf(-progress * 20.0f);
            float plank = sinf(2.0f * (float)M_PI * 140.0f * t) * env * 0.7f;
            float creak = sinf(2.0f * (float)M_PI * 420.0f * t) * expf(-progress * 35.0f) * 0.35f;
            float tap = audio_noise() * env * 0.35f;
            out = (plank + creak + tap) * v->volume * 0.65f;
            break;
        }
        
        case SFX_CUP_SINK: {
            /* Rich authentic golf cup sink: hollow plop -> plastic liner bounce -> pin flag rattle */
            if (t < 0.12f) {
                float p_env = expf(-t * 22.0f);
                float freq = 320.0f * expf(-t * 20.0f) + 150.0f;
                float plop = sinf(2.0f * (float)M_PI * freq * t) * p_env * 1.1f;
                float thud = audio_noise() * p_env * 0.45f;
                out = (plop + thud);
            } else if (t < 0.28f) {
                float rel_t = t - 0.12f;
                float r_env = expf(-rel_t * 22.0f);
                float clatter = (sinf(2.0f * (float)M_PI * 860.0f * rel_t) * 0.7f +
                                 sinf(2.0f * (float)M_PI * 1440.0f * rel_t) * 0.45f) * r_env;
                float tap = audio_noise() * r_env * 0.5f;
                out = (clatter + tap) * 1.15f;
            } else {
                float rel_t = t - 0.28f;
                float s_env = expf(-rel_t * 16.0f);
                float rattle = (sinf(2.0f * (float)M_PI * 680.0f * rel_t) * 0.6f +
                                sinf(2.0f * (float)M_PI * 1280.0f * rel_t) * 0.35f) * s_env;
                float click = audio_noise() * s_env * 0.35f;
                out = (rattle + click) * 0.9f;
            }
            out *= v->volume * 1.35f;
            break;
        }
        
        case SFX_SWEETSPOT: {
            /* Cheerful two-tone chime (1200 Hz & 1800 Hz) */
            float env = expf(-progress * 7.0f);
            float chime = (sinf(2.0f * (float)M_PI * 1200.0f * t) * 0.6f +
                           sinf(2.0f * (float)M_PI * 1800.0f * t) * 0.4f) * env;
            out = chime * v->volume * 0.7f;
            break;
        }
        
        case SFX_APPLAUSE_GOLF: {
            /* Polite, rhythmic golf gallery applause */
            float clap_pulse1 = expf(-fmodf(t * 8.5f, 1.0f) * 12.0f);
            float clap_pulse2 = expf(-fmodf((t + 0.06f) * 7.8f, 1.0f) * 14.0f);
            float clap_pulse3 = expf(-fmodf((t + 0.11f) * 9.2f, 1.0f) * 11.0f);
            float envelope = (t < 0.2f) ? (t / 0.2f) : expf(-(t - 0.2f) * 1.8f);
            float noise = audio_noise();
            float bandpass = sinf(2.0f * (float)M_PI * 1100.0f * t) * noise;
            out = (clap_pulse1 * 0.45f + clap_pulse2 * 0.35f + clap_pulse3 * 0.4f) * (noise * 0.6f + bandpass * 0.4f) * envelope * v->volume * 1.25f;
            break;
        }
        
        case SFX_CROWD_CHEER: {
            /* Enthusiastic gallery cheer & roar for birdie / hole-in-one */
            float env = (t < 0.35f) ? (t / 0.35f) : expf(-(t - 0.35f) * 1.6f);
            float roar_mod = sinf(2.0f * (float)M_PI * 6.5f * t) * 0.15f + 1.0f;
            float vocal1 = sinf(2.0f * (float)M_PI * (480.0f + 60.0f * sinf(t * 4.0f)) * t);
            float vocal2 = sinf(2.0f * (float)M_PI * 720.0f * t) * 0.5f;
            float noise = audio_noise() * 0.65f;
            float clap = expf(-fmodf(t * 11.0f, 1.0f) * 10.0f) * audio_noise() * 0.5f;
            out = ((vocal1 + vocal2) * 0.45f + noise + clap) * env * roar_mod * v->volume * 1.35f;
            break;
        }
        
        case SFX_CROWD_OOH: {
            /* Collective gallery gasp / groan */
            float env = (t < 0.15f) ? (t / 0.15f) : expf(-(t - 0.15f) * 2.5f);
            float freq = 340.0f * expf(-t * 2.2f) + 180.0f;
            float vocal = sinf(2.0f * (float)M_PI * freq * t) * 0.6f +
                          sinf(2.0f * (float)M_PI * (freq * 1.5f) * t) * 0.35f;
            float breath = audio_noise() * expf(-t * 3.0f) * 0.3f;
            out = (vocal + breath) * env * v->volume * 1.1f;
            break;
        }
        
        case SFX_BUILD_PLACE: {
            /* Crisp wooden mallet / construction placement tap */
            float env = expf(-progress * 28.0f);
            float pop = sinf(2.0f * (float)M_PI * 380.0f * t) * env * 0.85f;
            float snap = audio_noise() * expf(-progress * 45.0f) * 0.65f;
            out = (pop + snap) * v->volume * 0.95f;
            break;
        }
        
        case SFX_TERRAIN_SCULPT: {
            /* Earth/stone shift and rumble when raising/lowering elevation */
            float env = sinf(progress * (float)M_PI);
            float rumble_freq = 65.0f + 25.0f * sinf(t * 30.0f);
            float rumble = sinf(2.0f * (float)M_PI * rumble_freq * t) * 0.75f;
            float gravel = audio_noise() * 0.45f;
            out = (rumble + gravel) * env * v->volume * 0.9f;
            break;
        }
        
        case SFX_BEVERAGE_WHISTLE: {
            /* Cheerful 2-tone melodic whistle: Note 1 (A6/1760Hz), brief gap, Note 2 (C#7->E7 glissando) */
            float w_freq = 0.0f;
            float w_env = 0.0f;
            if (t < 0.20f) {
                /* First note: 1760 Hz with soft attack and decay */
                w_freq = 1760.0f;
                w_env = sinf((t / 0.20f) * (float)M_PI);
            } else if (t >= 0.24f && t < 0.62f) {
                /* Second note: 2200 Hz gliding up to 2640 Hz with vibrato */
                float t2 = (t - 0.24f) / 0.38f;
                w_freq = 2200.0f + t2 * 440.0f + sinf(t2 * 26.0f) * 16.0f;
                w_env = sinf(t2 * (float)M_PI);
            }
            if (w_env > 0.0f) {
                float phase = 2.0f * (float)M_PI * w_freq * t;
                float tone = sinf(phase) + sinf(phase * 2.0f) * 0.08f;
                float breath = audio_noise() * 0.035f;
                out = (tone + breath) * w_env * v->volume * 1.15f;
            }
            break;
        }
        
        case SFX_GOPHER_CHUCKLE: {
            /* Caddyshack mischievous gopher squeak/chuckle: 4 rapid high-pitched squeaks ("Heh-heh-heh-heh!") */
            float chirp_dur = 0.085f;
            float gap_dur = 0.035f;
            float cycle = chirp_dur + gap_dur;
            int chirp_idx = (int)(t / cycle);
            float t_in_cycle = fmodf(t, cycle);
            if (chirp_idx < 4 && t_in_cycle < chirp_dur) {
                float progress = t_in_cycle / chirp_dur;
                /* Pitch sweeps up 1400 Hz to 2400 Hz with playful vibrato */
                float base_f = 1400.0f + (float)chirp_idx * 160.0f;
                float g_freq = base_f + progress * 700.0f + sinf(progress * 42.0f) * 80.0f;
                float g_env = sinf(progress * (float)M_PI);
                float phase = 2.0f * (float)M_PI * g_freq * t;
                float tone = sinf(phase) + 0.35f * sinf(phase * 2.0f) + 0.15f * sinf(phase * 3.0f);
                out = tone * g_env * v->volume * 1.25f;
            }
            break;
        }
        
        case SFX_GOPHER_GULP: {
            /* Comical subterranean suction gulp ("FWUMP-bloop!") as ball vanishes down gopher hole */
            if (t < 0.12f) {
                /* Downward suction woosh/pop (540 Hz down to 90 Hz) */
                float progress = t / 0.12f;
                float f_drop = 540.0f * expf(-progress * 3.2f) + 90.0f;
                float phase = 2.0f * (float)M_PI * f_drop * t;
                float env = sinf(progress * (float)M_PI);
                float air = audio_noise() * 0.25f * (1.0f - progress);
                out = (sinf(phase) * 0.75f + air) * env * v->volume * 1.40f;
            } else if (t < 0.32f) {
                /* Subterranean resonant bubble bloop (180 Hz) */
                float t2 = t - 0.12f;
                float f_bloop = 220.0f * expf(-t2 * 14.0f) + 120.0f;
                float phase = 2.0f * (float)M_PI * f_bloop * t2;
                float env2 = expf(-t2 * 18.0f);
                out = sinf(phase) * env2 * v->volume * 1.30f;
            }
            break;
        }
        
        case SFX_CARL_EXPLOSION: {
            /* Carl Spackler Massive TNT Dynamite Detonation!
             * Sub-bass boom falling 130 Hz down to 26 Hz + noise blast & falling clods */
            float sub_bass = 0.0f;
            if (t < 0.45f) {
                float k_freq = 130.0f * expf(-t * 12.0f) + 26.0f;
                float phase = 2.0f * (float)M_PI * k_freq * t;
                sub_bass = sinf(phase) * expf(-t * 8.0f) * 0.95f;
            }
            float noise_blast = audio_noise() * expf(-t * 6.5f) * 0.75f;
            float dirt_rumble = audio_noise() * expf(-t * 3.2f) * 0.35f;
            out = (sub_bass + noise_blast + dirt_rumble) * v->volume * 1.35f;
            if (out > 1.0f) out = 1.0f;
            if (out < -1.0f) out = -1.0f;
            break;
        }
        
        case SFX_CARL_HOSE: {
            /* Carl Spackler High-Pressure Water Hose Spray into burrow */
            float hiss = audio_noise() * 0.50f;
            float flow = sinf(t * 18.0f) * 0.15f + 0.85f;
            float gurgle = sinf(2.0f * (float)M_PI * (160.0f + sinf(t * 32.0f) * 40.0f) * t) * 0.25f;
            float env = (t < 0.1f) ? (t / 0.1f) : ((t > 0.9f) ? (1.0f - (t - 0.9f) / 0.3f) : 1.0f);
            out = (hiss * flow + gurgle) * env * v->volume * 1.10f;
            break;
        }

        case SFX_SIREN: {
            /* Police Cart Siren: Wailing electronic siren sweeping 650 Hz to 950 Hz */
            float lfo = sinf(2.0f * (float)M_PI * 2.2f * t);
            float f_siren = 780.0f + lfo * 160.0f;
            float phase = 2.0f * (float)M_PI * f_siren * t;
            float tone = sinf(phase) + 0.35f * sinf(phase * 2.0f) + 0.15f * sinf(phase * 3.0f);
            float env = (t < 0.05f) ? (t / 0.05f) : ((progress > 0.85f) ? (1.0f - progress) / 0.15f : 1.0f);
            out = tone * env * v->volume * 1.05f;
            break;
        }

        case SFX_SHOTGUN: {
            /* Ash Williams 12-Gauge Double Barrel Shotgun: Thunderous blast + metallic pump */
            if (t < 0.30f) {
                float blast_t = t;
                float low_f = 160.0f * expf(-blast_t * 18.0f) + 32.0f;
                float thump = sinf(2.0f * (float)M_PI * low_f * blast_t) * expf(-blast_t * 12.0f);
                float crack = audio_noise() * expf(-blast_t * 9.0f) * 1.4f;
                out = (thump * 0.9f + crack * 1.1f) * v->volume * 1.4f;
            } else if (t >= 0.35f && t < 0.65f) {
                /* Pump action metal clack */
                float pt = t - 0.35f;
                float clack_t = (pt < 0.15f) ? pt : (pt - 0.15f);
                float clack_env = expf(-clack_t * 45.0f);
                float clack_tone = sinf(2.0f * (float)M_PI * 1850.0f * clack_t) * 0.6f +
                                   sinf(2.0f * (float)M_PI * 3400.0f * clack_t) * 0.4f;
                out = (clack_tone + audio_noise() * 0.35f) * clack_env * v->volume * 0.85f;
            }
            break;
        }

        case SFX_CHAINSAW: {
            /* Ash Williams Chainsaw: Gritty 2-stroke engine revving and tearing teeth */
            float rev = 110.0f + 55.0f * sinf(2.0f * (float)M_PI * 3.5f * t) + 15.0f * sinf(t * 40.0f);
            float saw_phase = 2.0f * (float)M_PI * rev * t;
            float saw = (fmodf(saw_phase, 2.0f * (float)M_PI) / (float)M_PI) - 1.0f;
            float sub = sinf(saw_phase * 0.5f) * 0.4f;
            float grit = audio_noise() * 0.35f;
            float env = (t < 0.08f) ? (t / 0.08f) : ((progress > 0.88f) ? (1.0f - progress) / 0.12f : 1.0f);
            out = (saw * 0.7f + sub + grit) * env * v->volume * 1.15f;
            break;
        }

        case SFX_ZOMBIE: {
            /* Undead Groan: Low guttural formant slide ("Braaaains...") */
            float z_freq = 85.0f * expf(-t * 0.45f) + 45.0f + sinf(t * 14.0f) * 6.0f;
            float z_phase = 2.0f * (float)M_PI * z_freq * t;
            float z_pulse = (sinf(z_phase) > 0.1f) ? 0.7f : -0.7f;
            float z_formant = sinf(z_phase * 4.2f) * 0.4f + sinf(z_phase * 7.5f) * 0.25f;
            float rasp = audio_noise() * 0.25f;
            float env = sinf(progress * (float)M_PI);
            out = (z_pulse * 0.5f + z_formant + rasp) * env * v->volume * 1.25f;
            break;
        }

        case SFX_SEAMONSTER: {
            /* Sea Monster Eruption & Leviathan Roar: Huge surge splash + cavernous growl */
            float water_surge = audio_noise() * expf(-t * 2.2f) * 0.75f;
            float roar_freq = 62.0f + sinf(t * 12.0f) * 16.0f;
            float roar_phase = 2.0f * (float)M_PI * roar_freq * t;
            float roar = (sinf(roar_phase) + 0.45f * sinf(roar_phase * 2.0f) + 0.2f * sinf(roar_phase * 3.0f));
            float env = sinf(progress * (float)M_PI);
            out = (water_surge * 0.6f + roar * 0.8f) * env * v->volume * 1.35f;
            break;
        }

        default:
            break;
    }
    
    v->sample_pos++;
    if (v->sample_pos >= v->total_samples) {
        v->active = false;
    }
    return out;
}

/* Synthesize single music sample for Intro Melody: "I'm Alright (Theme from Caddyshack)" (128 BPM) */
static void synthesize_music_sample(uint32_t clock, float vol, float *out_l, float *out_r) {
    uint32_t step_samples = 5168; /* 128 BPM 16th note at 44100 Hz */
    /* Start 2 bars before chorus (Bar 3, step 32) and loop 2 bars after chorus (through Bar 8, step 127).
     * Total loop = 6 bars = 96 steps. */
    uint32_t loop_steps = 96;
    uint32_t total_loop = step_samples * loop_steps;
    uint32_t loop_pos = clock % total_loop;
    uint32_t step = 32 + ((loop_pos / step_samples) % loop_steps);
    uint32_t step_pos = loop_pos % step_samples;
    float t_step = (float)step_pos / (float)AUDIO_SAMPLE_RATE;
    
    /* 1. Rhythm Section (Driving 80s Rock Drum Beat) */
    float drum = 0.0f;
    uint32_t meas_step = step % 16;
    
    /* Kick drum on beats 1, 2, 3, 4 and syncopated pickup */
    bool is_kick = (meas_step == 0 || meas_step == 4 || meas_step == 8 || meas_step == 12 || meas_step == 10);
    if (is_kick && t_step < 0.12f) {
        float k_freq = 135.0f * expf(-t_step * 38.0f) + 42.0f;
        s_m_kick_phase += 2.0f * (float)M_PI * k_freq / (float)AUDIO_SAMPLE_RATE;
        if (s_m_kick_phase > 2.0f * (float)M_PI) s_m_kick_phase -= 2.0f * (float)M_PI;
        drum += sinf(s_m_kick_phase) * expf(-t_step * 24.0f) * 0.45f;
    }
    
    /* Snare on backbeats 2 and 4 (meas_step 4 and 12) + measure 8 fill */
    bool is_snare = (meas_step == 4 || meas_step == 12 || (step >= 124 && step <= 127));
    if (is_snare && t_step < 0.18f) {
        float s_body = sinf(2.0f * (float)M_PI * 185.0f * t_step) * expf(-t_step * 26.0f);
        float s_noise = audio_noise() * expf(-t_step * 18.0f);
        drum += (s_body * 0.35f + s_noise * 0.65f) * 0.36f;
    }
    
    /* Hi-hat on 8th notes */
    if ((meas_step % 2) == 0 && t_step < 0.05f) {
        drum += audio_noise() * expf(-t_step * 68.0f) * 0.10f;
    }
    
    /* 2. Driving 80s Slap/Synth Bass ("I'm Alright" bassline) */
    float bass = 0.0f;
    float b_freq = s_caddy_bass[step];
    if (b_freq > 0.0f) {
        s_m_bass_phase += 2.0f * (float)M_PI * b_freq / (float)AUDIO_SAMPLE_RATE;
        if (s_m_bass_phase > 2.0f * (float)M_PI) s_m_bass_phase -= 2.0f * (float)M_PI;
        float b_env = expf(-t_step * 13.0f);
        bass = (sinf(s_m_bass_phase) + 0.42f * sinf(s_m_bass_phase * 2.0f) + 0.15f * sinf(s_m_bass_phase * 3.0f)) * b_env * 0.38f;
    }
    
    /* 3. Punchy 80s Synth Brass Stabs */
    float chord = 0.0f;
    if (s_caddy_chord_stabs[step]) {
        float c_env = expf(-t_step * 7.5f);
        for (int k = 0; k < 3; ++k) {
            float cf = s_caddy_chords[step / 16][k];
            s_m_chord_phase[k] += 2.0f * (float)M_PI * cf / (float)AUDIO_SAMPLE_RATE;
            if (s_m_chord_phase[k] > 2.0f * (float)M_PI) s_m_chord_phase[k] -= 2.0f * (float)M_PI;
            float saw = sinf(s_m_chord_phase[k]) + 0.35f * sinf(s_m_chord_phase[k] * 2.0f) + 0.15f * sinf(s_m_chord_phase[k] * 3.0f);
            chord += saw * c_env * 0.085f;
        }
    }
    
    /* 4. Lead Synthesizer ("I'm Alright" Theme Melody) */
    float lead = 0.0f;
    float m_freq = s_caddy_lead[step];
    if (m_freq > 0.0f) {
        float l_env = (t_step < 0.012f) ? (t_step / 0.012f) : expf(-t_step * 2.8f);
        float vibrato = (t_step > 0.06f) ? (sinf((t_step - 0.06f) * 36.0f) * 0.025f) : 0.0f;
        s_m_lead_phase += 2.0f * (float)M_PI * (m_freq * (1.0f + vibrato)) / (float)AUDIO_SAMPLE_RATE;
        if (s_m_lead_phase > 2.0f * (float)M_PI) s_m_lead_phase -= 2.0f * (float)M_PI;
        float l_wave = sinf(s_m_lead_phase) + 0.28f * sinf(s_m_lead_phase * 2.0f) + 0.12f * sinf(s_m_lead_phase * 4.0f);
        lead = l_wave * l_env * 0.35f;
    }
    
    /* Stereo Mix: Drums & Bass Center, Chords slightly Left, Lead slightly Right */
    *out_l = (drum * 0.5f + bass * 0.5f + chord * 0.65f + lead * 0.45f) * vol;
    *out_r = (drum * 0.5f + bass * 0.5f + chord * 0.35f + lead * 0.55f) * vol;
}

/* Audio Callback (Runs on audio thread) */
static void audio_callback(void *userdata, Uint8 *stream, int len) {
    (void)userdata;
    int16_t *buffer = (int16_t *)stream;
    int num_samples = len / (sizeof(int16_t) * AUDIO_CHANNELS);
    
    for (int i = 0; i < num_samples; ++i) {
        /* 1. Update smooth ambient volume transitions */
        s_water_current_vol += (s_water_target_vol - s_water_current_vol) * 0.0005f;
        s_clubhouse_current_vol += (s_clubhouse_target_vol - s_clubhouse_current_vol) * 0.0005f;
        
        /* 2. Water Ambient (gentle ripple when near water) */
        float water_sample = 0.0f;
        if (s_water_current_vol > 0.001f) {
            s_water_phase1 += 0.0035f;
            if (s_water_phase1 > 2.0f * (float)M_PI) s_water_phase1 -= 2.0f * (float)M_PI;
            s_water_phase2 += 0.008f;
            if (s_water_phase2 > 2.0f * (float)M_PI) s_water_phase2 -= 2.0f * (float)M_PI;
            float w_sine = (sinf(s_water_phase1) * 0.5f + sinf(s_water_phase2) * 0.3f);
            water_sample = w_sine * s_water_current_vol * 0.35f;
        }
        
        /* 3. Clubhouse Ambient (soft patio murmur & faint glasses) */
        float clubhouse_sample = 0.0f;
        if (s_clubhouse_current_vol > 0.001f) {
            float murmur = audio_noise() * 0.12f;
            clubhouse_sample = murmur * s_clubhouse_current_vol * 0.4f;
        }
        
        /* 4. Procedural Bird Chirps */
        float bird_left = 0.0f;
        float bird_right = 0.0f;
        
        if (s_bird_active) {
            float b_prog = (float)s_bird_pos / (float)s_bird_dur;
            float freq_mod = sinf(b_prog * 4.0f * (float)M_PI) * 500.0f;
            float freq = s_bird_base_freq + freq_mod;
            s_bird_phase += 2.0f * (float)M_PI * freq / (float)AUDIO_SAMPLE_RATE;
            if (s_bird_phase > 2.0f * (float)M_PI) s_bird_phase -= 2.0f * (float)M_PI;
            
            float b_env = sinf(b_prog * (float)M_PI);
            float chirp = sinf(s_bird_phase) * b_env * 0.09f;
            
            float pan_l = 0.5f - (s_bird_pan * 0.4f);
            float pan_r = 0.5f + (s_bird_pan * 0.4f);
            bird_left = chirp * pan_l;
            bird_right = chirp * pan_r;
            
            s_bird_pos++;
            if (s_bird_pos >= s_bird_dur) {
                s_bird_active = false;
                s_bird_timer_samples = AUDIO_SAMPLE_RATE * 2 + (s_audio_rand % (AUDIO_SAMPLE_RATE * 4));
            }
        } else {
            s_bird_timer_samples--;
            if (s_bird_timer_samples <= 0) {
                s_bird_active = true;
                s_bird_pos = 0;
                s_bird_dur = AUDIO_SAMPLE_RATE / 6 + (s_audio_rand % (AUDIO_SAMPLE_RATE / 8));
                s_bird_base_freq = 2400.0f + (float)(s_audio_rand % 1200);
                s_bird_pan = audio_noise();
                s_bird_phase = 0.0f;
            }
        }
        
        /* 5. Mix Active SFX Voices */
        float sfx_mix_left = 0.0f;
        float sfx_mix_right = 0.0f;
        
        for (int v = 0; v < MAX_ACTIVE_VOICES; ++v) {
            if (s_voices[v].active) {
                float samp = synthesize_sfx_sample(&s_voices[v]);
                float pan_l = 0.5f - (s_voices[v].pan * 0.4f);
                float pan_r = 0.5f + (s_voices[v].pan * 0.4f);
                sfx_mix_left += samp * pan_l;
                sfx_mix_right += samp * pan_r;
            }
        }
        
        /* 6. Procedural Intro Screen Melody */
        float music_left = 0.0f;
        float music_right = 0.0f;
        if (s_intro_music_active || s_intro_music_vol > 0.001f) {
            s_intro_music_vol += (s_intro_music_target_vol - s_intro_music_vol) * 0.0004f;
            if (s_intro_music_vol < 0.001f && s_intro_music_target_vol == 0.0f) {
                s_intro_music_active = false;
            } else {
                synthesize_music_sample(s_music_sample_clock, s_intro_music_vol, &music_left, &music_right);
                s_music_sample_clock++;
            }
        }
        
        /* Master Mix */
        float left = (water_sample * 0.6f + clubhouse_sample * 0.5f + bird_left + sfx_mix_left + music_left);
        float right = (water_sample * 0.6f + clubhouse_sample * 0.5f + bird_right + sfx_mix_right + music_right);
        
        /* Soft Limiting */
        if (left > 0.95f) left = 0.95f;
        if (left < -0.95f) left = -0.95f;
        if (right > 0.95f) right = 0.95f;
        if (right < -0.95f) right = -0.95f;
        
        buffer[i * 2 + 0] = (int16_t)(left * 32000.0f);
        buffer[i * 2 + 1] = (int16_t)(right * 32000.0f);
    }
}

bool golf_audio_init(void) {
    if (s_audio_ready) return true;
    
    memset(s_voices, 0, sizeof(s_voices));
    
    SDL_AudioSpec desired;
    SDL_AudioSpec obtained;
    memset(&desired, 0, sizeof(desired));
    
    desired.freq = AUDIO_SAMPLE_RATE;
    desired.format = AUDIO_S16SYS;
    desired.channels = AUDIO_CHANNELS;
    desired.samples = AUDIO_BUFFER_SAMPLES;
    desired.callback = audio_callback;
    desired.userdata = NULL;
    
    s_audio_device = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0);
    if (s_audio_device == 0) {
        if (SDL_OpenAudio(&desired, &obtained) < 0) {
            fprintf(stderr, "SDL Audio Init Warning: %s\n", SDL_GetError());
            s_audio_ready = false;
            return false;
        }
        SDL_PauseAudio(0);
    } else {
        SDL_PauseAudioDevice(s_audio_device, 0);
    }
    
    s_audio_ready = true;
    return true;
}

void golf_audio_shutdown(void) {
    if (!s_audio_ready) return;
    if (s_audio_device != 0) {
        SDL_CloseAudioDevice(s_audio_device);
        s_audio_device = 0;
    } else {
        SDL_CloseAudio();
    }
    s_audio_ready = false;
}

void golf_audio_play_sfx(GolfSfxType sfx) {
    if (!s_audio_ready) return;
    
    if (s_audio_device != 0) {
        SDL_LockAudioDevice(s_audio_device);
    } else {
        SDL_LockAudio();
    }
    
    /* Find free voice slot or oldest voice */
    int best_slot = -1;
    uint32_t max_pos = 0;
    
    for (int i = 0; i < MAX_ACTIVE_VOICES; ++i) {
        if (!s_voices[i].active) {
            best_slot = i;
            break;
        }
        if (s_voices[i].sample_pos > max_pos) {
            max_pos = s_voices[i].sample_pos;
            best_slot = i;
        }
    }
    
    if (best_slot >= 0) {
        SfxVoice *v = &s_voices[best_slot];
        v->active = true;
        v->type = sfx;
        v->sample_pos = 0;
        v->volume = 1.0f;
        v->pan = 0.0f;
        
        switch (sfx) {
            case SFX_SWING_DRIVER:   v->total_samples = AUDIO_SAMPLE_RATE * 38 / 100; v->volume = 1.35f; break;
            case SFX_SWING_IRON:     v->total_samples = AUDIO_SAMPLE_RATE * 30 / 100; v->volume = 1.25f; break;
            case SFX_SWING_WEDGE:    v->total_samples = AUDIO_SAMPLE_RATE * 26 / 100; v->volume = 1.20f; break;
            case SFX_SWING_PUTTER:   v->total_samples = AUDIO_SAMPLE_RATE * 18 / 100; v->volume = 1.15f; break;
            case SFX_BALL_BOUNCE:    v->total_samples = AUDIO_SAMPLE_RATE * 12 / 100; v->volume = 0.80f; break;
            case SFX_TREE_HIT:       v->total_samples = AUDIO_SAMPLE_RATE * 42 / 100; v->volume = 1.25f; break;
            case SFX_ROCK_HIT:       v->total_samples = AUDIO_SAMPLE_RATE * 38 / 100; v->volume = 1.25f; break;
            case SFX_WATER_SPLASH:   v->total_samples = AUDIO_SAMPLE_RATE * 55 / 100; v->volume = 1.20f; break;
            case SFX_FOOTSTEP_GRASS: v->total_samples = AUDIO_SAMPLE_RATE * 10 / 100; v->volume = 0.50f; break;
            case SFX_FOOTSTEP_SAND:  v->total_samples = AUDIO_SAMPLE_RATE * 13 / 100; v->volume = 0.55f; break;
            case SFX_FOOTSTEP_ROUGH: v->total_samples = AUDIO_SAMPLE_RATE * 15 / 100; v->volume = 0.55f; break;
            case SFX_FOOTSTEP_WOOD:  v->total_samples = AUDIO_SAMPLE_RATE * 12 / 100; v->volume = 0.65f; break;
            case SFX_CUP_SINK:       v->total_samples = AUDIO_SAMPLE_RATE * 55 / 100; v->volume = 1.45f; break;
            case SFX_SWEETSPOT:      v->total_samples = AUDIO_SAMPLE_RATE * 40 / 100; v->volume = 0.95f; break;
            case SFX_APPLAUSE_GOLF:  v->total_samples = AUDIO_SAMPLE_RATE * 160 / 100; v->volume = 1.25f; break;
            case SFX_CROWD_CHEER:    v->total_samples = AUDIO_SAMPLE_RATE * 220 / 100; v->volume = 1.35f; break;
            case SFX_CROWD_OOH:      v->total_samples = AUDIO_SAMPLE_RATE * 90 / 100; v->volume = 1.15f; break;
            case SFX_BUILD_PLACE:    v->total_samples = AUDIO_SAMPLE_RATE * 14 / 100; v->volume = 0.95f; break;
            case SFX_TERRAIN_SCULPT: v->total_samples = AUDIO_SAMPLE_RATE * 28 / 100; v->volume = 0.90f; break;
            case SFX_BEVERAGE_WHISTLE: v->total_samples = AUDIO_SAMPLE_RATE * 65 / 100; v->volume = 1.30f; break;
            case SFX_GOPHER_CHUCKLE:   v->total_samples = AUDIO_SAMPLE_RATE * 50 / 100; v->volume = 1.30f; break;
            case SFX_GOPHER_GULP:      v->total_samples = AUDIO_SAMPLE_RATE * 35 / 100; v->volume = 1.40f; break;
            case SFX_CARL_EXPLOSION:   v->total_samples = AUDIO_SAMPLE_RATE * 180 / 100; v->volume = 1.60f; break;
            case SFX_CARL_HOSE:        v->total_samples = AUDIO_SAMPLE_RATE * 140 / 100; v->volume = 1.25f; break;
            case SFX_SIREN:            v->total_samples = AUDIO_SAMPLE_RATE * 120 / 100; v->volume = 1.20f; break;
            case SFX_SHOTGUN:          v->total_samples = AUDIO_SAMPLE_RATE * 80 / 100;  v->volume = 1.60f; break;
            case SFX_CHAINSAW:         v->total_samples = AUDIO_SAMPLE_RATE * 100 / 100; v->volume = 1.30f; break;
            case SFX_ZOMBIE:           v->total_samples = AUDIO_SAMPLE_RATE * 120 / 100; v->volume = 1.35f; break;
            case SFX_SEAMONSTER:       v->total_samples = AUDIO_SAMPLE_RATE * 150 / 100; v->volume = 1.45f; break;
            default: v->total_samples = AUDIO_SAMPLE_RATE / 4; break;
        }
    }
    
    if (s_audio_device != 0) {
        SDL_UnlockAudioDevice(s_audio_device);
    } else {
        SDL_UnlockAudio();
    }
}

void golf_audio_start_intro_music(void) {
    if (!s_audio_ready) return;
    if (s_audio_device != 0) SDL_LockAudioDevice(s_audio_device);
    else SDL_LockAudio();
    
    s_intro_music_active = true;
    s_intro_music_target_vol = 0.85f;
    s_music_sample_clock = 0;
    
    if (s_audio_device != 0) SDL_UnlockAudioDevice(s_audio_device);
    else SDL_UnlockAudio();
}

void golf_audio_stop_intro_music(void) {
    if (!s_audio_ready) return;
    if (s_audio_device != 0) SDL_LockAudioDevice(s_audio_device);
    else SDL_LockAudio();
    
    s_intro_music_target_vol = 0.0f;
    
    if (s_audio_device != 0) SDL_UnlockAudioDevice(s_audio_device);
    else SDL_UnlockAudio();
}

void golf_audio_update_ambient(fixed wind_speed, int water_proximity_percent) {
    golf_audio_update_ambient_pro(wind_speed, water_proximity_percent, 0);
}

void golf_audio_update_ambient_pro(fixed wind_speed, int water_proximity_percent, int clubhouse_proximity_percent) {
    if (!s_audio_ready) return;
    (void)wind_speed;
    
    float w_prox = (float)water_proximity_percent / 100.0f;
    if (w_prox < 0.0f) w_prox = 0.0f;
    if (w_prox > 1.0f) w_prox = 1.0f;
    s_water_target_vol = w_prox * 0.15f;
    
    float c_prox = (float)clubhouse_proximity_percent / 100.0f;
    if (c_prox < 0.0f) c_prox = 0.0f;
    if (c_prox > 1.0f) c_prox = 1.0f;
    s_clubhouse_target_vol = c_prox * 0.22f;
}

#endif /* !USE_WIN32_GDI */
