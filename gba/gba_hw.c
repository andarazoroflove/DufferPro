#include "gba_hw.h"

/* Offscreen Backbuffer in 256KB EWRAM (76,800 bytes) */
static uint16_t s_backbuffer[GBA_SCREEN_W * GBA_SCREEN_H];

/* Key states */
static uint16_t s_prev_keys = 0x03FF;
static uint16_t s_curr_keys = 0x03FF;
static uint16_t s_pressed_keys = 0;

void gba_hw_init(void) {
    /* Disable and acknowledge all interrupts for clean startup */
    *REG_IME = 0;
    *REG_IE = 0;
    *REG_IF = 0xFFFF;

    /* Safe 4-2 waitstates for SuperCard SDRAM / flashcart compatibility */
    *REG_WAITCNT = 0x0000;

    /* Set Video Mode 3 (240x160 15-bit color bitmap) + BG2 layer enable */
    *REG_DISPCNT = 0x0403;

    /* Initialize Sound Hardware (Master Enable + 100% Volume on all PSG channels) */
    *REG_SOUNDCNT_X = 0x0080;
    *REG_SOUNDCNT_L = 0x7777;
    *REG_SOUNDCNT_H = 0x0002;

    /* Clear VRAM (volatile 32-bit stores prevent illegal 8-bit STRB) and Backbuffer */
    volatile uint32_t *vram32 = (volatile uint32_t*)VRAM;
    uint32_t *bb32 = (uint32_t*)s_backbuffer;
    for (int i = 0; i < (GBA_SCREEN_W * GBA_SCREEN_H) / 2; ++i) {
        vram32[i] = 0;
        bb32[i] = 0;
    }
}

void gba_vsync(void) {
    /* Wait for VBlank scanline 160..227 */
    while (*REG_VCOUNT >= 160);
    while (*REG_VCOUNT < 160);
}

void gba_flip(void) {
    gba_vsync();

    /* Fast 32-bit copy from EWRAM backbuffer to VRAM */
    const uint32_t *src = (const uint32_t*)s_backbuffer;
    volatile uint32_t *dst = (volatile uint32_t*)VRAM;
    
    for (int i = 0; i < (GBA_SCREEN_W * GBA_SCREEN_H) / 2; i += 8) {
        dst[i + 0] = src[i + 0];
        dst[i + 1] = src[i + 1];
        dst[i + 2] = src[i + 2];
        dst[i + 3] = src[i + 3];
        dst[i + 4] = src[i + 4];
        dst[i + 5] = src[i + 5];
        dst[i + 6] = src[i + 6];
        dst[i + 7] = src[i + 7];
    }
}

uint16_t* gba_get_backbuffer(void) {
    return s_backbuffer;
}

void gba_poll_keys(void) {
    s_prev_keys = s_curr_keys;
    /* Invert active-low bits so 1 = pressed */
    s_curr_keys = (~(*REG_KEYINPUT)) & 0x03FF;
    s_pressed_keys = s_curr_keys & (~s_prev_keys);
}

bool gba_key_held(uint16_t key) {
    return (s_curr_keys & key) != 0;
}

bool gba_key_pressed(uint16_t key) {
    return (s_pressed_keys & key) != 0;
}

/* ========================================================================= */
/* GBA HARDWARE SOUND EFFECTS (Direct PSG Synthesis via Audio Registers)      */
/* ========================================================================= */

void gba_sfx_swing_hit(int club_type) {
    /* Noise Channel 4: Club Strike Crack */
    if (club_type == 0) {
        /* Driver: Heavy crack + resonance */
        *REG_SOUND4CNT_L = 0xF100; /* Max volume, envelope decay 1 */
        *REG_SOUND4CNT_H = 0x8023; /* Initial trigger, 7-stage noise */
    } else if (club_type == 4) {
        /* Putter: Soft click tap */
        *REG_SOUND4CNT_L = 0x7100;
        *REG_SOUND4CNT_H = 0x8035;
    } else {
        /* Iron / Wedge */
        *REG_SOUND4CNT_L = 0xC100;
        *REG_SOUND4CNT_H = 0x8014;
    }
}

void gba_sfx_bounce(void) {
    /* Channel 1: Short acoustic pitch drop chirp */
    *REG_SOUND1CNT_L = 0x0052; /* Sweep down, time 5 */
    *REG_SOUND1CNT_H = 0x8200; /* 50% duty, decay */
    *REG_SOUND1CNT_X = 0x8520; /* Trigger, frequency ~700 Hz */
}

void gba_sfx_cup_sink(void) {
    /* Channel 2: Two resonant chime tones dropping into cup */
    *REG_SOUND2CNT_L = 0xB200;
    *REG_SOUND2CNT_H = 0x8680; /* High tone */
}

void gba_sfx_water_splash(void) {
    /* Channel 4: White noise water splash */
    *REG_SOUND4CNT_L = 0xE300; /* High volume, slower decay */
    *REG_SOUND4CNT_H = 0x8052; /* Rumbling noise */
}

void gba_sfx_gopher_chuckle(void) {
    /* Channel 1: High-pitched Caddyshack chirp */
    *REG_SOUND1CNT_L = 0x0011;
    *REG_SOUND1CNT_H = 0xC100;
    *REG_SOUND1CNT_X = 0x8760; /* ~1800 Hz chirp */
}

void gba_sfx_gopher_gulp(void) {
    /* Channel 1: Comical subterranean suction drop */
    *REG_SOUND1CNT_L = 0x0074; /* Steep downward sweep */
    *REG_SOUND1CNT_H = 0xF200;
    *REG_SOUND1CNT_X = 0x8600;
}

void gba_sfx_carl_explosion(void) {
    /* Carl Spackler Massive TNT Explosion */
    /* Channel 4: Detonation rumble */
    *REG_SOUND4CNT_L = 0xF500; /* Max volume, long rumble */
    *REG_SOUND4CNT_H = 0x8071; /* Heavy low-frequency noise */
    /* Channel 1: Sub-bass boom drop */
    *REG_SOUND1CNT_L = 0x0077; /* Deep sweep */
    *REG_SOUND1CNT_H = 0xF400;
    *REG_SOUND1CNT_X = 0x8180;
}

void gba_sfx_carl_hose(void) {
    /* Carl High-Pressure Water Hose Spray */
    *REG_SOUND4CNT_L = 0xB100;
    *REG_SOUND4CNT_H = 0x8043; /* Continuous turbulent hiss */
}

void gba_sfx_whistle(void) {
    /* Beverage Girl Whistle on Channel 2 */
    *REG_SOUND2CNT_L = 0xA200;
    *REG_SOUND2CNT_H = 0x8740; /* High clear whistle */
}

/* ========================================================================= */
/* CADDYSHACK THEME MUSIC SEQUENCER ("I'm Alright" by Kenny Loggins)         */
/* ========================================================================= */

#define GBA_NOTE_REST  0
#define GBA_NOTE_E3    1254
#define GBA_NOTE_G3    1379
#define GBA_NOTE_A3    1452
#define GBA_NOTE_B3    1517
#define GBA_NOTE_D4    1602
#define GBA_NOTE_E4    1651
#define GBA_NOTE_G4    1714
#define GBA_NOTE_A4    1750
#define GBA_NOTE_B4    1783
#define GBA_NOTE_D5    1825
#define GBA_NOTE_E5    1849

static const uint16_t s_melody_lead[64] = {
    GBA_NOTE_E4, GBA_NOTE_REST, GBA_NOTE_REST, GBA_NOTE_G4,
    GBA_NOTE_A4, GBA_NOTE_REST, GBA_NOTE_B4, GBA_NOTE_REST,
    GBA_NOTE_A4, GBA_NOTE_REST, GBA_NOTE_G4, GBA_NOTE_REST,
    GBA_NOTE_E4, GBA_NOTE_REST, GBA_NOTE_REST, GBA_NOTE_REST,

    GBA_NOTE_E4, GBA_NOTE_REST, GBA_NOTE_G4, GBA_NOTE_REST,
    GBA_NOTE_A4, GBA_NOTE_REST, GBA_NOTE_REST, GBA_NOTE_REST,
    GBA_NOTE_D5, GBA_NOTE_REST, GBA_NOTE_REST, GBA_NOTE_REST,
    GBA_NOTE_B4, GBA_NOTE_REST, GBA_NOTE_REST, GBA_NOTE_REST,

    GBA_NOTE_B4, GBA_NOTE_REST, GBA_NOTE_A4, GBA_NOTE_REST,
    GBA_NOTE_G4, GBA_NOTE_REST, GBA_NOTE_E4, GBA_NOTE_REST,
    GBA_NOTE_D4, GBA_NOTE_REST, GBA_NOTE_E4, GBA_NOTE_REST,
    GBA_NOTE_REST, GBA_NOTE_REST, GBA_NOTE_REST, GBA_NOTE_REST,

    GBA_NOTE_E4, GBA_NOTE_REST, GBA_NOTE_REST, GBA_NOTE_REST,
    GBA_NOTE_G4, GBA_NOTE_REST, GBA_NOTE_A4, GBA_NOTE_REST,
    GBA_NOTE_B4, GBA_NOTE_REST, GBA_NOTE_REST, GBA_NOTE_REST,
    GBA_NOTE_E5, GBA_NOTE_REST, GBA_NOTE_REST, GBA_NOTE_REST
};

static const uint16_t s_melody_bass[64] = {
    GBA_NOTE_E3, GBA_NOTE_REST, GBA_NOTE_REST, GBA_NOTE_REST,
    GBA_NOTE_E3, GBA_NOTE_REST, GBA_NOTE_G3, GBA_NOTE_REST,
    GBA_NOTE_E3, GBA_NOTE_REST, GBA_NOTE_REST, GBA_NOTE_REST,
    GBA_NOTE_D4, GBA_NOTE_REST, GBA_NOTE_E3, GBA_NOTE_REST,

    GBA_NOTE_E3, GBA_NOTE_REST, GBA_NOTE_REST, GBA_NOTE_REST,
    GBA_NOTE_A3, GBA_NOTE_REST, GBA_NOTE_REST, GBA_NOTE_REST,
    GBA_NOTE_D4, GBA_NOTE_REST, GBA_NOTE_REST, GBA_NOTE_REST,
    GBA_NOTE_B3, GBA_NOTE_REST, GBA_NOTE_REST, GBA_NOTE_REST,

    GBA_NOTE_B3, GBA_NOTE_REST, GBA_NOTE_A3, GBA_NOTE_REST,
    GBA_NOTE_G3, GBA_NOTE_REST, GBA_NOTE_E3, GBA_NOTE_REST,
    GBA_NOTE_D4, GBA_NOTE_REST, GBA_NOTE_E3, GBA_NOTE_REST,
    GBA_NOTE_REST, GBA_NOTE_REST, GBA_NOTE_REST, GBA_NOTE_REST,

    GBA_NOTE_E3, GBA_NOTE_REST, GBA_NOTE_REST, GBA_NOTE_REST,
    GBA_NOTE_G3, GBA_NOTE_REST, GBA_NOTE_A3, GBA_NOTE_REST,
    GBA_NOTE_B3, GBA_NOTE_REST, GBA_NOTE_REST, GBA_NOTE_REST,
    GBA_NOTE_E3, GBA_NOTE_REST, GBA_NOTE_REST, GBA_NOTE_REST
};

void gba_audio_tick_splash(int tick) {
    /* 16th note triggers every 7 frames (~128 BPM) */
    if (tick % 7 != 0) return;
    int step = (tick / 7) % 64;

    /* Lead Melody on Channel 2 */
    uint16_t lead_freq = s_melody_lead[step];
    if (lead_freq > 0) {
        *REG_SOUND2CNT_L = 0xB200; /* 50% duty, volume 11, decay */
        *REG_SOUND2CNT_H = 0x8000 | lead_freq;
    }

    /* Slap Bassline on Channel 1 */
    uint16_t bass_freq = s_melody_bass[step];
    if (bass_freq > 0) {
        *REG_SOUND1CNT_L = 0x0000;
        *REG_SOUND1CNT_H = 0x9300; /* Volume 9 */
        *REG_SOUND1CNT_X = 0x8000 | bass_freq;
    }

    /* Percussion Beat on Channel 4 */
    if (step % 8 == 4) {
        *REG_SOUND4CNT_L = 0x6100; /* Crisp snare */
        *REG_SOUND4CNT_H = 0x8021;
    } else if (step % 8 == 0) {
        *REG_SOUND4CNT_L = 0x3100; /* Soft hi-hat */
        *REG_SOUND4CNT_H = 0x8011;
    }
}

void gba_audio_stop_music(void) {
    *REG_SOUND1CNT_H = 0;
    *REG_SOUND2CNT_L = 0;
    *REG_SOUND4CNT_L = 0;
}

/* ========================================================================= */
/* BARE-METAL C RUNTIME SUPPORT                                              */
/* ========================================================================= */

void *memcpy(void *dest, const void *src, size_t n) {
    char *d = (char*)dest;
    const char *s = (const char*)src;
    while (n--) *d++ = *s++;
    return dest;
}

void *memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char*)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++)
        dest[i] = src[i];
    for ( ; i < n; i++)
        dest[i] = '\0';
    return dest;
}

int abs(int j) {
    return (j < 0) ? -j : j;
}

size_t strlen(const char *s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

