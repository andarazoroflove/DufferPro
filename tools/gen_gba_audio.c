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

static void write_c_array(FILE *f, const char *name, const int8_t *samples, int count) {
    fprintf(f, "/* %s: %d samples (%.2f s) at 16,000 Hz */\n", name, count, (float)count / SAMPLE_RATE);
    fprintf(f, "static const int8_t %s[%d] __attribute__((aligned(4))) = {\n    ", name, count);
    for (int i = 0; i < count; ++i) {
        fprintf(f, "%d%s", samples[i], (i == count - 1) ? "" : ",");
        if ((i + 1) % 24 == 0 && i < count - 1) fprintf(f, "\n    ");
    }
    fprintf(f, "\n};\n\n");
}

int main(void) {
    printf("Generating gba/gba_sound_data.h...\n");
    FILE *out = fopen("gba/gba_sound_data.h", "w");
    if (!out) {
        printf("Failed to open gba/gba_sound_data.h for writing\n");
        return 1;
    }

    fprintf(out, "#ifndef GBA_SOUND_DATA_H\n");
    fprintf(out, "#define GBA_SOUND_DATA_H\n\n");
    fprintf(out, "#include <stdint.h>\n\n");
    fprintf(out, "#define GBA_AUDIO_SAMPLE_RATE 16000\n\n");

    /* 1. Titanium Driver Crack (0.30s) */
    int len_driver = SAMPLE_RATE * 30 / 100;
    int8_t *s_driver = (int8_t*)malloc(len_driver);
    for (int i = 0; i < len_driver; ++i) {
        float t = (float)i / SAMPLE_RATE;
        float env_crack = expf(-t * 85.0f);
        float env_ring = expf(-t * 22.0f);
        float env_thump = expf(-t * 28.0f);
        float crack = rnd() * env_crack * 0.92f;
        float ring = (sinf(2.0f * (float)M_PI * 2780.0f * t) * 0.45f + sinf(2.0f * (float)M_PI * 4150.0f * t) * 0.25f) * env_ring;
        float thump = sinf(2.0f * (float)M_PI * (160.0f - t * 240.0f) * t) * 0.55f * env_thump;
        float val = crack + ring + thump;
        if (val > 0.95f) val = 0.95f; if (val < -0.95f) val = -0.95f;
        s_driver[i] = (int8_t)(val * 127.0f);
    }
    write_c_array(out, "g_sfx_driver", s_driver, len_driver);
    free(s_driver);

    /* 2. Iron Impact (0.22s) */
    int len_iron = SAMPLE_RATE * 22 / 100;
    int8_t *s_iron = (int8_t*)malloc(len_iron);
    for (int i = 0; i < len_iron; ++i) {
        float t = (float)i / SAMPLE_RATE;
        float env = expf(-t * 35.0f);
        float ping = sinf(2.0f * (float)M_PI * 1850.0f * t) * 0.55f + sinf(2.0f * (float)M_PI * 3100.0f * t) * 0.25f;
        float turf = rnd() * expf(-t * 50.0f) * 0.65f;
        float val = (ping + turf) * env;
        if (val > 0.95f) val = 0.95f; if (val < -0.95f) val = -0.95f;
        s_iron[i] = (int8_t)(val * 127.0f);
    }
    write_c_array(out, "g_sfx_iron", s_iron, len_iron);
    free(s_iron);

    /* 3. Wedge Zip (0.20s) */
    int len_wedge = SAMPLE_RATE * 20 / 100;
    int8_t *s_wedge = (int8_t*)malloc(len_wedge);
    for (int i = 0; i < len_wedge; ++i) {
        float t = (float)i / SAMPLE_RATE;
        float env = expf(-t * 40.0f);
        float zip = sinf(2.0f * (float)M_PI * (3400.0f - t * 4000.0f) * t) * 0.6f;
        float divot = rnd() * expf(-t * 60.0f) * 0.7f;
        float val = (zip + divot) * env;
        if (val > 0.95f) val = 0.95f; if (val < -0.95f) val = -0.95f;
        s_wedge[i] = (int8_t)(val * 127.0f);
    }
    write_c_array(out, "g_sfx_wedge", s_wedge, len_wedge);
    free(s_wedge);

    /* 4. Putter Tap (0.12s) */
    int len_putter = SAMPLE_RATE * 12 / 100;
    int8_t *s_putter = (int8_t*)malloc(len_putter);
    for (int i = 0; i < len_putter; ++i) {
        float t = (float)i / SAMPLE_RATE;
        float env = expf(-t * 65.0f);
        float click = sinf(2.0f * (float)M_PI * 650.0f * t) * 0.8f + rnd() * 0.2f * expf(-t * 120.0f);
        float val = click * env;
        if (val > 0.95f) val = 0.95f; if (val < -0.95f) val = -0.95f;
        s_putter[i] = (int8_t)(val * 127.0f);
    }
    write_c_array(out, "g_sfx_putter", s_putter, len_putter);
    free(s_putter);

    /* 5. Ball Landing Heavy Turf Thud (0.24s) */
    int len_thud = SAMPLE_RATE * 24 / 100;
    int8_t *s_thud = (int8_t*)malloc(len_thud);
    for (int i = 0; i < len_thud; ++i) {
        float t = (float)i / SAMPLE_RATE;
        float env = expf(-t * 32.0f);
        float f0 = 135.0f - t * 280.0f;
        if (f0 < 45.0f) f0 = 45.0f;
        float thud = sinf(2.0f * (float)M_PI * f0 * t) * 0.85f + rnd() * expf(-t * 60.0f) * 0.35f;
        float val = thud * env;
        if (val > 0.95f) val = 0.95f; if (val < -0.95f) val = -0.95f;
        s_thud[i] = (int8_t)(val * 127.0f);
    }
    write_c_array(out, "g_sfx_ball_thud", s_thud, len_thud);
    free(s_thud);

    /* 6. Cup Sink Rattle (0.38s) */
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
        if (val > 0.95f) val = 0.95f; if (val < -0.95f) val = -0.95f;
        s_cup[i] = (int8_t)(val * 127.0f);
    }
    write_c_array(out, "g_sfx_cup_sink", s_cup, len_cup);
    free(s_cup);

    /* 7. Pin Placement Crystal Bell Ding (0.35s) */
    int len_ding = SAMPLE_RATE * 35 / 100;
    int8_t *s_ding = (int8_t*)malloc(len_ding);
    for (int i = 0; i < len_ding; ++i) {
        float t = (float)i / SAMPLE_RATE;
        float env = expf(-t * 9.5f);
        float chime = sinf(2.0f * (float)M_PI * 2093.0f * t) * 0.75f + sinf(2.0f * (float)M_PI * 4186.0f * t) * 0.25f;
        float val = chime * env;
        if (val > 0.95f) val = 0.95f; if (val < -0.95f) val = -0.95f;
        s_ding[i] = (int8_t)(val * 127.0f);
    }
    write_c_array(out, "g_sfx_pin_ding", s_ding, len_ding);
    free(s_ding);

    /* 8. Beverage Cart Whistle (0.35s) */
    int len_whistle = SAMPLE_RATE * 35 / 100;
    int8_t *s_whistle = (int8_t*)malloc(len_whistle);
    for (int i = 0; i < len_whistle; ++i) {
        float t = (float)i / SAMPLE_RATE;
        float env = (t < 0.05f) ? (t / 0.05f) : expf(-(t - 0.05f) * 6.0f);
        float f = (t < 0.15f) ? 1420.0f : 1780.0f;
        float vib = sinf(2.0f * (float)M_PI * 8.0f * t) * 25.0f;
        float tone = sinf(2.0f * (float)M_PI * (f + vib) * t);
        float val = tone * env * 0.85f;
        if (val > 0.95f) val = 0.95f; if (val < -0.95f) val = -0.95f;
        s_whistle[i] = (int8_t)(val * 127.0f);
    }
    write_c_array(out, "g_sfx_bev_whistle", s_whistle, len_whistle);
    free(s_whistle);

    /* 9. Golfer Burp (0.45s) */
    int len_burp = SAMPLE_RATE * 45 / 100;
    int8_t *s_burp = (int8_t*)malloc(len_burp);
    for (int i = 0; i < len_burp; ++i) {
        float t = (float)i / SAMPLE_RATE;
        float env = (t < 0.06f) ? (t / 0.06f) : expf(-(t - 0.06f) * 7.0f);
        float f0 = 68.0f + 14.0f * sinf(2.0f * (float)M_PI * 6.0f * t);
        float flutter = 1.0f + 0.45f * sinf(2.0f * (float)M_PI * 32.0f * t);
        float ph = 2.0f * (float)M_PI * f0 * t;
        float pulse = sinf(ph) + 0.5f * sinf(ph * 2.0f) + 0.3f * sinf(ph * 3.0f);
        float fry = rnd() * 0.22f;
        float val = (pulse * flutter + fry) * env * 0.85f;
        if (val > 0.95f) val = 0.95f; if (val < -0.95f) val = -0.95f;
        s_burp[i] = (int8_t)(val * 127.0f);
    }
    write_c_array(out, "g_sfx_golfer_burp", s_burp, len_burp);
    free(s_burp);

    /* 10. Ash Boomstick Shotgun Blast & Formant (0.50s) */
    int len_boom = SAMPLE_RATE * 50 / 100;
    int8_t *s_boom = (int8_t*)malloc(len_boom);
    for (int i = 0; i < len_boom; ++i) {
        float t = (float)i / SAMPLE_RATE;
        float blast = rnd() * expf(-t * 24.0f) * 0.95f;
        float roar = sinf(2.0f * (float)M_PI * (95.0f - t * 90.0f) * t) * expf(-t * 14.0f) * 0.7f;
        float val = blast + roar;
        if (val > 0.95f) val = 0.95f; if (val < -0.95f) val = -0.95f;
        s_boom[i] = (int8_t)(val * 127.0f);
    }
    write_c_array(out, "g_sfx_boomstick", s_boom, len_boom);
    free(s_boom);

    /* 11. Gopher Chuckle (0.30s) */
    int len_chuckle = SAMPLE_RATE * 30 / 100;
    int8_t *s_chuckle = (int8_t*)malloc(len_chuckle);
    for (int i = 0; i < len_chuckle; ++i) {
        float t = (float)i / SAMPLE_RATE;
        float chirp_env = fabsf(sinf(2.0f * (float)M_PI * 18.0f * t));
        float f0 = 1750.0f + 350.0f * sinf(2.0f * (float)M_PI * 36.0f * t);
        float tone = sinf(2.0f * (float)M_PI * f0 * t);
        float val = tone * chirp_env * expf(-t * 4.0f) * 0.85f;
        if (val > 0.95f) val = 0.95f; if (val < -0.95f) val = -0.95f;
        s_chuckle[i] = (int8_t)(val * 127.0f);
    }
    write_c_array(out, "g_sfx_gopher_chuckle", s_chuckle, len_chuckle);
    free(s_chuckle);

    /* 12. Gopher Gulp (0.24s) */
    int len_gulp = SAMPLE_RATE * 24 / 100;
    int8_t *s_gulp = (int8_t*)malloc(len_gulp);
    for (int i = 0; i < len_gulp; ++i) {
        float t = (float)i / SAMPLE_RATE;
        float env = expf(-t * 12.0f);
        float f = 650.0f - t * 1800.0f;
        if (f < 80.0f) f = 80.0f;
        float tone = sinf(2.0f * (float)M_PI * f * t);
        float val = tone * env * 0.85f;
        if (val > 0.95f) val = 0.95f; if (val < -0.95f) val = -0.95f;
        s_gulp[i] = (int8_t)(val * 127.0f);
    }
    write_c_array(out, "g_sfx_gopher_gulp", s_gulp, len_gulp);
    free(s_gulp);

    /* 13. Carl Explosion (0.55s) */
    int len_exp = SAMPLE_RATE * 55 / 100;
    int8_t *s_exp = (int8_t*)malloc(len_exp);
    for (int i = 0; i < len_exp; ++i) {
        float t = (float)i / SAMPLE_RATE;
        float env = expf(-t * 8.0f);
        float rumble = sinf(2.0f * (float)M_PI * (75.0f - t * 45.0f) * t) * 0.65f;
        float blast = rnd() * 0.75f;
        float val = (rumble + blast) * env;
        if (val > 0.95f) val = 0.95f; if (val < -0.95f) val = -0.95f;
        s_exp[i] = (int8_t)(val * 127.0f);
    }
    write_c_array(out, "g_sfx_carl_explosion", s_exp, len_exp);
    free(s_exp);

    /* 14. Water Splash (0.35s) */
    int len_splash = SAMPLE_RATE * 35 / 100;
    int8_t *s_splash = (int8_t*)malloc(len_splash);
    for (int i = 0; i < len_splash; ++i) {
        float t = (float)i / SAMPLE_RATE;
        float env = expf(-t * 12.0f);
        float noise = rnd() * 0.85f;
        float plop = sinf(2.0f * (float)M_PI * (350.0f - t * 600.0f) * t) * 0.45f * expf(-t * 28.0f);
        float val = (noise + plop) * env;
        if (val > 0.95f) val = 0.95f; if (val < -0.95f) val = -0.95f;
        s_splash[i] = (int8_t)(val * 127.0f);
    }
    write_c_array(out, "g_sfx_water_splash", s_splash, len_splash);
    free(s_splash);

    /* 15. Ambient Wind Breeze (0.50s seamless loopable) */
    int len_wind = SAMPLE_RATE * 50 / 100;
    int8_t *s_wind = (int8_t*)malloc(len_wind);
    float wind_filter = 0.0f;
    for (int i = 0; i < len_wind; ++i) {
        float raw = rnd() * 0.35f;
        wind_filter += 0.08f * (raw - wind_filter);
        float env = 0.8f + 0.2f * sinf(2.0f * (float)M_PI * (float)i / (float)len_wind);
        float val = wind_filter * env;
        if (val > 0.5f) val = 0.5f; if (val < -0.5f) val = -0.5f;
        s_wind[i] = (int8_t)(val * 127.0f);
    }
    write_c_array(out, "g_sfx_ambient_wind", s_wind, len_wind);
    free(s_wind);

    /* 16. Ambient Water Brook (0.50s seamless loopable) */
    int len_brook = SAMPLE_RATE * 50 / 100;
    int8_t *s_brook = (int8_t*)malloc(len_brook);
    float brook_filter = 0.0f;
    for (int i = 0; i < len_brook; ++i) {
        float raw = rnd() * 0.4f;
        brook_filter += 0.15f * (raw - brook_filter);
        float bub = sinf(2.0f * (float)M_PI * 4.0f * (float)i / (float)SAMPLE_RATE) * 0.15f;
        float val = (brook_filter + bub);
        if (val > 0.5f) val = 0.5f; if (val < -0.5f) val = -0.5f;
        s_brook[i] = (int8_t)(val * 127.0f);
    }
    write_c_array(out, "g_sfx_ambient_water", s_brook, len_brook);
    free(s_brook);

    fprintf(out, "#endif /* GBA_SOUND_DATA_H */\n");
    fclose(out);
    printf("Successfully generated gba/gba_sound_data.h!\n");
    return 0;
}

