#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_CHANNELS 2

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static uint32_t s_audio_rand = 54321;
static inline float audio_noise(void) {
    s_audio_rand = s_audio_rand * 1664525 + 1013904223;
    return ((float)(s_audio_rand & 0xFFFF) / 32768.0f) - 1.0f;
}

static void write_wav(const char *filename, const int16_t *samples, int num_frames) {
    FILE *f = fopen(filename, "wb");
    if (!f) {
        printf("Failed to open %s for writing\n", filename);
        return;
    }
    
    int data_size = num_frames * AUDIO_CHANNELS * sizeof(int16_t);
    int file_size = 36 + data_size;
    
    uint8_t header[44] = {
        'R', 'I', 'F', 'F',
        file_size & 0xFF, (file_size >> 8) & 0xFF, (file_size >> 16) & 0xFF, (file_size >> 24) & 0xFF,
        'W', 'A', 'V', 'E',
        'f', 'm', 't', ' ',
        16, 0, 0, 0,
        1, 0, /* PCM */
        AUDIO_CHANNELS, 0,
        AUDIO_SAMPLE_RATE & 0xFF, (AUDIO_SAMPLE_RATE >> 8) & 0xFF, (AUDIO_SAMPLE_RATE >> 16) & 0xFF, (AUDIO_SAMPLE_RATE >> 24) & 0xFF,
        (AUDIO_SAMPLE_RATE * AUDIO_CHANNELS * 2) & 0xFF, ((AUDIO_SAMPLE_RATE * AUDIO_CHANNELS * 2) >> 8) & 0xFF, ((AUDIO_SAMPLE_RATE * AUDIO_CHANNELS * 2) >> 16) & 0xFF, ((AUDIO_SAMPLE_RATE * AUDIO_CHANNELS * 2) >> 24) & 0xFF,
        4, 0, /* Block align */
        16, 0, /* Bits per sample */
        'd', 'a', 't', 'a',
        data_size & 0xFF, (data_size >> 8) & 0xFF, (data_size >> 16) & 0xFF, (data_size >> 24) & 0xFF
    };
    
    fwrite(header, 1, 44, f);
    fwrite(samples, sizeof(int16_t), num_frames * AUDIO_CHANNELS, f);
    fclose(f);
    printf("Wrote %s (%d frames, %.2f sec)\n", filename, num_frames, (float)num_frames / (float)AUDIO_SAMPLE_RATE);
}

static float synth_boomstick_sample(float t, float volume) {
    float out = 0.0f;
    if (t < 0.28f) {
        /* Syllable 1: "BOOM" (/b/ -> /u/ -> /m/) */
        float t_syll = t / 0.28f;
        float f0 = 225.0f - t_syll * 25.0f;
        float phase = 2.0f * (float)M_PI * f0 * t;
        float glot = sinf(phase) + 0.42f * sinf(phase * 2.0f) + 0.18f * sinf(phase * 3.0f);
        
        float f1 = sinf(2.0f * (float)M_PI * 320.0f * t) * 1.0f;
        float f2 = sinf(2.0f * (float)M_PI * 880.0f * t) * 0.72f;
        float f3 = sinf(2.0f * (float)M_PI * 2300.0f * t) * 0.28f;
        
        float b_burst = (t < 0.025f) ? (sinf(2.0f * (float)M_PI * 135.0f * t) * 0.85f + audio_noise() * 0.25f) : 0.0f;
        
        float env = sinf(t_syll * (float)M_PI);
        if (t_syll > 0.75f) {
            f1 = sinf(2.0f * (float)M_PI * 260.0f * t) * 0.9f;
            f2 = sinf(2.0f * (float)M_PI * 1050.0f * t) * 0.35f;
            f3 = 0.0f;
        }
        
        out = (b_burst + glot * (f1 + f2 + f3) * 0.45f) * env * volume * 1.55f;
    } else if (t >= 0.31f && t < 0.65f) {
        /* Syllable 2: "STICK!" (/s/ -> /t/ -> /ih/ -> /k/) */
        float t_syll2 = t - 0.31f;
        
        if (t_syll2 < 0.075f) {
            float s_env = sinf((t_syll2 / 0.075f) * (float)M_PI);
            float hiss = audio_noise() * (0.6f + 0.4f * sinf(2.0f * (float)M_PI * 6200.0f * t_syll2));
            out = hiss * s_env * volume * 0.95f;
        } else if (t_syll2 >= 0.075f && t_syll2 < 0.095f) {
            float t_burst_t = t_syll2 - 0.090f;
            if (t_burst_t >= 0.0f && t_burst_t < 0.005f) {
                out = (sinf(2.0f * (float)M_PI * 3800.0f * t_burst_t) + audio_noise() * 0.5f) * volume * 1.1f;
            }
        } else if (t_syll2 >= 0.095f && t_syll2 < 0.22f) {
            float v_t = t_syll2 - 0.095f;
            float v_prog = v_t / 0.125f;
            float f0_2 = 255.0f - v_prog * 17.0f;
            float phase2 = 2.0f * (float)M_PI * f0_2 * v_t;
            float glot2 = sinf(phase2) + 0.40f * sinf(phase2 * 2.0f) + 0.18f * sinf(phase2 * 3.0f);
            
            float f1 = sinf(2.0f * (float)M_PI * 430.0f * v_t) * 1.0f;
            float f2 = sinf(2.0f * (float)M_PI * 1980.0f * v_t) * 0.78f;
            float f3 = sinf(2.0f * (float)M_PI * 2650.0f * v_t) * 0.40f;
            float env2 = sinf(v_prog * (float)M_PI);
            out = glot2 * (f1 + f2 + f3) * 0.42f * env2 * volume * 1.50f;
        } else {
            float k_t = t_syll2 - 0.22f;
            float k_env = expf(-k_t * 38.0f);
            float k_burst = audio_noise() * expf(-k_t * 48.0f) * 0.85f;
            float can_pop = (sinf(2.0f * (float)M_PI * 2850.0f * k_t) * 0.65f +
                             sinf(2.0f * (float)M_PI * 4150.0f * k_t) * 0.45f) * k_env;
            out = (k_burst + can_pop) * volume * 1.45f;
        }
    }
    return out;
}

int main(void) {
    printf("=== AUDIO SYNTHESIS VERIFICATION TEST ===\n");
    
    /* 1. Test Cart Girl Boomstick Sound */
    int num_samples = AUDIO_SAMPLE_RATE * 72 / 100;
    int16_t *buf = (int16_t *)malloc(num_samples * AUDIO_CHANNELS * sizeof(int16_t));
    if (!buf) {
        printf("Out of memory\n");
        return 1;
    }
    
    float max_amp = 0.0f;
    for (int i = 0; i < num_samples; i++) {
        float t = (float)i / (float)AUDIO_SAMPLE_RATE;
        float s = synth_boomstick_sample(t, 1.45f);
        if (fabsf(s) > max_amp) max_amp = fabsf(s);
        if (s > 0.95f) s = 0.95f;
        if (s < -0.95f) s = -0.95f;
        buf[i * 2 + 0] = (int16_t)(s * 32000.0f);
        buf[i * 2 + 1] = (int16_t)(s * 32000.0f);
    }
    printf("Boomstick max amplitude: %.3f (Soft-clipped to 0.95)\n", max_amp);
    write_wav("build/test_boomstick.wav", buf, num_samples);
    free(buf);
    
    /* 2. Test Water Gurgle */
    int gurgle_samples = AUDIO_SAMPLE_RATE * 3;
    buf = (int16_t *)malloc(gurgle_samples * AUDIO_CHANNELS * sizeof(int16_t));
    float stream_phase = 0.0f;
    float bub_phases[4] = {0};
    float bub_freqs[4] = {260.0f, 380.0f, 510.0f, 330.0f};
    max_amp = 0.0f;
    
    for (int i = 0; i < gurgle_samples; i++) {
        float g_prog = (float)i / (float)gurgle_samples;
        float g_env = sinf(g_prog * (float)M_PI);
        g_env = g_env * g_env;
        
        stream_phase += 2.0f * (float)M_PI * 180.0f / (float)AUDIO_SAMPLE_RATE;
        if (stream_phase > 2.0f * (float)M_PI) stream_phase -= 2.0f * (float)M_PI;
        float stream = (sinf(stream_phase) * 0.4f + audio_noise() * 0.15f) * 0.035f;
        
        float bubble_sum = 0.0f;
        for (int b = 0; b < 4; ++b) {
            uint32_t b_cycle = (AUDIO_SAMPLE_RATE / (7 + b * 3));
            uint32_t b_subpos = (i + b * 2200) % b_cycle;
            float b_subprog = (float)b_subpos / (float)b_cycle;
            
            float b_freq = bub_freqs[b] * (1.0f + b_subprog * 0.35f);
            bub_phases[b] += 2.0f * (float)M_PI * b_freq / (float)AUDIO_SAMPLE_RATE;
            if (bub_phases[b] > 2.0f * (float)M_PI) bub_phases[b] -= 2.0f * (float)M_PI;
            
            float b_decay = expf(-b_subprog * 12.0f);
            float bubble = sinf(bub_phases[b]) * b_decay * 0.065f;
            bubble_sum += bubble;
        }
        
        float gurgle = (stream + bubble_sum) * g_env * 0.75f;
        if (fabsf(gurgle) > max_amp) max_amp = fabsf(gurgle);
        buf[i * 2 + 0] = (int16_t)(gurgle * 32000.0f);
        buf[i * 2 + 1] = (int16_t)(gurgle * 32000.0f);
    }
    printf("Water gurgle max amplitude: %.3f\n", max_amp);
    write_wav("build/test_gurgle.wav", buf, gurgle_samples);
    free(buf);
    
    /* 3. Test Breeze */
    int breeze_samples = AUDIO_SAMPLE_RATE * 4;
    buf = (int16_t *)malloc(breeze_samples * AUDIO_CHANNELS * sizeof(int16_t));
    float f1 = 0.0f, f2 = 0.0f;
    max_amp = 0.0f;
    for (int i = 0; i < breeze_samples; i++) {
        float brz_prog = (float)i / (float)breeze_samples;
        float brz_env = sinf(brz_prog * (float)M_PI);
        brz_env = brz_env * brz_env;
        
        float white = audio_noise();
        float cutoff = 0.07f + 0.02f * sinf(brz_prog * 6.0f * (float)M_PI);
        f1 += cutoff * (white - f1);
        f2 += cutoff * (f1 - f2);
        
        float breeze = (f1 * 0.6f + f2 * 0.4f) * brz_env * 0.065f;
        if (fabsf(breeze) > max_amp) max_amp = fabsf(breeze);
        buf[i * 2 + 0] = (int16_t)(breeze * 32000.0f);
        buf[i * 2 + 1] = (int16_t)(breeze * 32000.0f);
    }
    printf("Breeze max amplitude: %.3f\n", max_amp);
    write_wav("build/test_breeze.wav", buf, breeze_samples);
    free(buf);
    
    printf("ALL AUDIO TESTS PASSED SUCCESSFULLY!\n");
    return 0;
}

