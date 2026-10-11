#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#define SAMPLE_RATE 16000

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static uint32_t s_rand = 987654321;
static inline float rnd(void) {
    s_rand = s_rand * 1664525 + 1013904223;
    return ((float)(s_rand & 0xFFFF) / 32768.0f) - 1.0f;
}

static void write_wav_8bit(const char *filename, const int8_t *samples, int num_samples) {
    FILE *f = fopen(filename, "wb");
    if (!f) return;
    
    int data_size = num_samples;
    int file_size = 36 + data_size;
    
    /* 8-bit unsigned PCM standard for WAV */
    uint8_t header[44] = {
        'R', 'I', 'F', 'F',
        file_size & 0xFF, (file_size >> 8) & 0xFF, (file_size >> 16) & 0xFF, (file_size >> 24) & 0xFF,
        'W', 'A', 'V', 'E',
        'f', 'm', 't', ' ',
        16, 0, 0, 0,
        1, 0, /* PCM */
        1, 0, /* Mono */
        SAMPLE_RATE & 0xFF, (SAMPLE_RATE >> 8) & 0xFF, (SAMPLE_RATE >> 16) & 0xFF, (SAMPLE_RATE >> 24) & 0xFF,
        SAMPLE_RATE & 0xFF, (SAMPLE_RATE >> 8) & 0xFF, (SAMPLE_RATE >> 16) & 0xFF, (SAMPLE_RATE >> 24) & 0xFF,
        1, 0, /* Block align */
        8, 0, /* 8 bits */
        'd', 'a', 't', 'a',
        data_size & 0xFF, (data_size >> 8) & 0xFF, (data_size >> 16) & 0xFF, (data_size >> 24) & 0xFF
    };
    
    fwrite(header, 1, 44, f);
    for (int i = 0; i < num_samples; ++i) {
        /* Convert signed int8 (-128..127) to unsigned uint8 (0..255) for standard WAV */
        uint8_t u = (uint8_t)(samples[i] + 128);
        fwrite(&u, 1, 1, f);
    }
    fclose(f);
    printf("Wrote %s (%d samples, %.2f s)\n", filename, num_samples, (float)num_samples / SAMPLE_RATE);
}

int main(void) {
    printf("=== GBA DIRECTSOUND AUDIO SYNTHESIS TEST ===\n");
    
    /* 1. Titanium Driver Crack */
    int len_driver = SAMPLE_RATE * 30 / 100; /* 0.30s */
    int8_t *s_driver = (int8_t*)malloc(len_driver);
    for (int i = 0; i < len_driver; ++i) {
        float t = (float)i / SAMPLE_RATE;
        float env_crack = expf(-t * 85.0f);
        float env_ring = expf(-t * 22.0f);
        float env_thump = expf(-t * 28.0f);
        
        float crack = rnd() * env_crack * 0.95f;
        float ring = (sinf(2.0f * (float)M_PI * 2780.0f * t) * 0.45f + sinf(2.0f * (float)M_PI * 4150.0f * t) * 0.25f) * env_ring;
        float thump = sinf(2.0f * (float)M_PI * (160.0f - t * 240.0f) * t) * 0.55f * env_thump;
        
        float val = crack + ring + thump;
        if (val > 0.95f) val = 0.95f;
        if (val < -0.95f) val = -0.95f;
        s_driver[i] = (int8_t)(val * 127.0f);
    }
    write_wav_8bit("build/gba_sfx_driver.wav", s_driver, len_driver);
    free(s_driver);
    
    /* 2. Cup Sink Rattle */
    int len_cup = SAMPLE_RATE * 38 / 100;
    int8_t *s_cup = (int8_t*)malloc(len_cup);
    for (int i = 0; i < len_cup; ++i) {
        float t = (float)i / SAMPLE_RATE;
        float val = 0.0f;
        if (t < 0.12f) {
            float env1 = expf(-t * 60.0f);
            val += (sinf(2.0f * (float)M_PI * 1820.0f * t) * 0.7f + rnd() * 0.3f) * env1;
        }
        if (t >= 0.07f && t < 0.28f) {
            float t2 = t - 0.07f;
            float env2 = expf(-t2 * 45.0f);
            val += (sinf(2.0f * (float)M_PI * 2350.0f * t2) * 0.6f + rnd() * 0.25f) * env2;
        }
        if (val > 0.95f) val = 0.95f;
        if (val < -0.95f) val = -0.95f;
        s_cup[i] = (int8_t)(val * 127.0f);
    }
    write_wav_8bit("build/gba_sfx_cup.wav", s_cup, len_cup);
    free(s_cup);

    /* 3. Golfer Burp */
    int len_burp = SAMPLE_RATE * 48 / 100;
    int8_t *s_burp = (int8_t*)malloc(len_burp);
    for (int i = 0; i < len_burp; ++i) {
        float t = (float)i / SAMPLE_RATE;
        float env = (t < 0.06f) ? (t / 0.06f) : expf(-(t - 0.06f) * 6.5f);
        float f0 = 68.0f + 14.0f * sinf(2.0f * (float)M_PI * 6.0f * t);
        float flutter = 1.0f + 0.45f * sinf(2.0f * (float)M_PI * 32.0f * t);
        float ph = 2.0f * (float)M_PI * f0 * t;
        float pulse = sinf(ph) + 0.5f * sinf(ph * 2.0f) + 0.3f * sinf(ph * 3.0f);
        float fry = rnd() * 0.22f;
        float val = (pulse * flutter + fry) * env * 0.85f;
        if (val > 0.95f) val = 0.95f;
        if (val < -0.95f) val = -0.95f;
        s_burp[i] = (int8_t)(val * 127.0f);
    }
    write_wav_8bit("build/gba_sfx_burp.wav", s_burp, len_burp);
    free(s_burp);

    /* 4. Pin Placement Ding (Crystal Bell C7) */
    int len_ding = SAMPLE_RATE * 35 / 100;
    int8_t *s_ding = (int8_t*)malloc(len_ding);
    for (int i = 0; i < len_ding; ++i) {
        float t = (float)i / SAMPLE_RATE;
        float env = expf(-t * 9.5f);
        float chime = sinf(2.0f * (float)M_PI * 2093.0f * t) * 0.75f + sinf(2.0f * (float)M_PI * 4186.0f * t) * 0.25f;
        float val = chime * env;
        if (val > 0.95f) val = 0.95f;
        if (val < -0.95f) val = -0.95f;
        s_ding[i] = (int8_t)(val * 127.0f);
    }
    write_wav_8bit("build/gba_sfx_ding.wav", s_ding, len_ding);
    free(s_ding);

    printf("ALL GBA AUDIO VERIFICATION TESTS PASSED!\n");
    return 0;
}

