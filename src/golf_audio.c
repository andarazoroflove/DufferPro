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
void golf_audio_update_ambient(fixed wind_speed, int water_proximity_percent) { (void)wind_speed; (void)water_proximity_percent; }
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>

#define AUDIO_SAMPLE_RATE    44100
#define AUDIO_CHANNELS       2
#define AUDIO_BUFFER_SAMPLES 1024
#define MAX_ACTIVE_VOICES    16

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Voice descriptor for polyphonic SFX */
typedef struct {
    bool active;
    GolfSfxType type;
    uint32_t sample_pos;
    uint32_t total_samples;
    float param1;
    float param2;
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

/* Bird Chirp Generator */
static int32_t s_bird_timer_samples = 44100 * 3; /* First chirp in 3s */
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

/* Synthesis helper for single SFX sample */
static float synthesize_sfx_sample(SfxVoice *v) {
    float t = (float)v->sample_pos / (float)AUDIO_SAMPLE_RATE;
    float total_t = (float)v->total_samples / (float)AUDIO_SAMPLE_RATE;
    float progress = (float)v->sample_pos / (float)v->total_samples;
    float out = 0.0f;
    
    switch (v->type) {
        case SFX_SWING_DRIVER: {
            /* Punchy 130 Hz thump rapidly gliding to 40 Hz + crisp crack noise burst */
            float env = expf(-progress * 9.0f);
            float freq = 135.0f * expf(-t * 28.0f) + 42.0f;
            float thump = sinf(2.0f * (float)M_PI * freq * t) * env;
            
            float crack_env = (t < 0.06f) ? (1.0f - t / 0.06f) : 0.0f;
            float crack = audio_noise() * crack_env * 0.9f;
            
            /* High whoosh component */
            float whoosh = audio_noise() * expf(-progress * 5.0f) * 0.25f;
            out = (thump * 0.7f + crack * 0.8f + whoosh) * v->volume;
            break;
        }
        
        case SFX_SWING_IRON: {
            /* Sharp metallic ping (880 Hz with 2640 Hz overtone) + tight impact pop */
            float env = expf(-progress * 11.0f);
            float ping = (sinf(2.0f * (float)M_PI * 880.0f * t) * 0.7f +
                          sinf(2.0f * (float)M_PI * 2640.0f * t) * 0.3f) * env;
            float pop_env = (t < 0.03f) ? (1.0f - t / 0.03f) : 0.0f;
            float pop = audio_noise() * pop_env * 0.6f;
            out = (ping * 0.75f + pop) * v->volume;
            break;
        }
        
        case SFX_SWING_WEDGE: {
            /* Crisp turf slice + 220 Hz low thump */
            float env = expf(-progress * 10.0f);
            float thump = sinf(2.0f * (float)M_PI * 220.0f * t) * env;
            float turf_env = (t < 0.08f) ? (1.0f - t / 0.08f) : 0.0f;
            float turf = audio_noise() * turf_env * 0.8f;
            out = (thump * 0.5f + turf * 0.75f) * v->volume;
            break;
        }
        
        case SFX_SWING_PUTTER: {
            /* Soft hollow wooden "tock" */
            float env = expf(-progress * 16.0f);
            float tock = sinf(2.0f * (float)M_PI * 330.0f * t) * env;
            float sub = sinf(2.0f * (float)M_PI * 165.0f * t) * env * 0.4f;
            out = (tock + sub) * v->volume * 0.8f;
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
        
        case SFX_CUP_SINK: {
            /* Plop -> Cup rattle -> Settled */
            if (t < 0.12f) {
                float plop_env = expf(-t * 25.0f);
                out = sinf(2.0f * (float)M_PI * 280.0f * t) * plop_env * 0.8f;
            } else if (t < 0.30f) {
                float rel_t = t - 0.12f;
                float rattle = sinf(2.0f * (float)M_PI * 680.0f * rel_t) * expf(-rel_t * 18.0f) * 0.6f;
                float click = audio_noise() * expf(-rel_t * 30.0f) * 0.4f;
                out = (rattle + click) * 0.8f;
            } else {
                float rel_t = t - 0.30f;
                out = sinf(2.0f * (float)M_PI * 520.0f * rel_t) * expf(-rel_t * 22.0f) * 0.3f;
            }
            out *= v->volume;
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
        
        default:
            break;
    }
    
    (void)total_t;
    v->sample_pos++;
    if (v->sample_pos >= v->total_samples) {
        v->active = false;
    }
    return out;
}

/* Audio Callback (Runs on audio thread) */
static void audio_callback(void *userdata, Uint8 *stream, int len) {
    (void)userdata;
    int16_t *buffer = (int16_t *)stream;
    int num_samples = len / (sizeof(int16_t) * AUDIO_CHANNELS);
    
    for (int i = 0; i < num_samples; ++i) {
        /* 1. Update smooth ambient volume transitions */
        s_water_current_vol += (s_water_target_vol - s_water_current_vol) * 0.0005f;
        
        /* 2. Wind ambient removed per request (no helicopter flutter) */
        float wind_sample = 0.0f;
        
        /* 3. Synthesize Water Ambient (gentle subtle ripple when near water) */
        float water_sample = 0.0f;
        if (s_water_current_vol > 0.001f) {
            s_water_phase1 += 0.0035f;
            if (s_water_phase1 > 2.0f * (float)M_PI) s_water_phase1 -= 2.0f * (float)M_PI;
            s_water_phase2 += 0.008f;
            if (s_water_phase2 > 2.0f * (float)M_PI) s_water_phase2 -= 2.0f * (float)M_PI;
            float w_sine = (sinf(s_water_phase1) * 0.5f + sinf(s_water_phase2) * 0.3f);
            water_sample = w_sine * s_water_current_vol * 0.35f;
        }
        
        /* 4. Synthesize Procedural Bird Chirps */
        float bird_left = 0.0f;
        float bird_right = 0.0f;
        
        if (s_bird_active) {
            float b_prog = (float)s_bird_pos / (float)s_bird_dur;
            
            /* Two rapid frequency sweeps (chirp-chirp) */
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
                /* Next chirp in 2.5 to 6.5 seconds */
                s_bird_timer_samples = AUDIO_SAMPLE_RATE * 2 + (s_audio_rand % (AUDIO_SAMPLE_RATE * 4));
            }
        } else {
            s_bird_timer_samples--;
            if (s_bird_timer_samples <= 0) {
                s_bird_active = true;
                s_bird_pos = 0;
                s_bird_dur = AUDIO_SAMPLE_RATE / 6 + (s_audio_rand % (AUDIO_SAMPLE_RATE / 8)); /* ~0.18s to 0.28s */
                s_bird_base_freq = 2400.0f + (float)(s_audio_rand % 1200);
                s_bird_pan = audio_noise(); /* Panned across stereo field */
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
        
        /* Master Mix */
        float left = (wind_sample * 0.7f + water_sample * 0.6f + bird_left + sfx_mix_left);
        float right = (wind_sample * 0.7f + water_sample * 0.6f + bird_right + sfx_mix_right);
        
        /* Soft Limiting / Clipping Protection */
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
        /* Fallback: try legacy SDL_OpenAudio */
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
    
    SDL_LockAudio();
    
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
            case SFX_SWING_DRIVER: v->total_samples = AUDIO_SAMPLE_RATE * 35 / 100; v->volume = 1.1f; break;
            case SFX_SWING_IRON:   v->total_samples = AUDIO_SAMPLE_RATE * 28 / 100; v->volume = 1.0f; break;
            case SFX_SWING_WEDGE:  v->total_samples = AUDIO_SAMPLE_RATE * 22 / 100; v->volume = 0.95f; break;
            case SFX_SWING_PUTTER: v->total_samples = AUDIO_SAMPLE_RATE * 14 / 100; v->volume = 0.85f; break;
            case SFX_BALL_BOUNCE:  v->total_samples = AUDIO_SAMPLE_RATE * 10 / 100; v->volume = 0.70f; break;
            case SFX_TREE_HIT:     v->total_samples = AUDIO_SAMPLE_RATE * 40 / 100; v->volume = 1.1f; break;
            case SFX_ROCK_HIT:     v->total_samples = AUDIO_SAMPLE_RATE * 38 / 100; v->volume = 1.1f; break;
            case SFX_WATER_SPLASH: v->total_samples = AUDIO_SAMPLE_RATE * 55 / 100; v->volume = 1.05f; break;
            case SFX_FOOTSTEP_GRASS: v->total_samples = AUDIO_SAMPLE_RATE * 10 / 100; v->volume = 0.50f; break;
            case SFX_FOOTSTEP_SAND:  v->total_samples = AUDIO_SAMPLE_RATE * 13 / 100; v->volume = 0.55f; break;
            case SFX_FOOTSTEP_ROUGH: v->total_samples = AUDIO_SAMPLE_RATE * 15 / 100; v->volume = 0.55f; break;
            case SFX_CUP_SINK:     v->total_samples = AUDIO_SAMPLE_RATE * 48 / 100; v->volume = 1.15f; break;
            case SFX_SWEETSPOT:    v->total_samples = AUDIO_SAMPLE_RATE * 40 / 100; v->volume = 0.90f; break;
            default: v->total_samples = AUDIO_SAMPLE_RATE / 4; break;
        }
    }
    
    SDL_UnlockAudio();
}

void golf_audio_update_ambient(fixed wind_speed, int water_proximity_percent) {
    if (!s_audio_ready) return;
    (void)wind_speed;
    
    /* Water proximity: 0..100% -> subtle lake ambient 0.0 to 0.15 */
    float prox = (float)water_proximity_percent / 100.0f;
    if (prox < 0.0f) prox = 0.0f;
    if (prox > 1.0f) prox = 1.0f;
    s_water_target_vol = prox * 0.15f;
}

#endif /* !USE_WIN32_GDI */

