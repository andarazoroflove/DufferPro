#include "gba_hw.h"

/* Offscreen Backbuffer in 256KB EWRAM (76,800 bytes) - strictly 4-byte aligned */
__attribute__((aligned(4))) static uint16_t s_backbuffer[GBA_SCREEN_W * GBA_SCREEN_H];

/* Key states */
static uint16_t s_prev_keys = 0x03FF;
static uint16_t s_curr_keys = 0x03FF;
static uint16_t s_pressed_keys = 0;

void gba_hw_init(void) {
    /* 1. Disable and acknowledge all interrupts for clean startup */
    *REG_IME = 0;
    *REG_IE = 0;
    *REG_IF = 0xFFFF;

    /* 2. Set Video Mode 3 (240x160 15-bit color bitmap) + BG2 layer enable */
    *REG_DISPCNT = 0x0403;

    /* 3. Initialize Sound Hardware (Master Enable + 100% Volume on all PSG channels) */
    *REG_SOUNDCNT_X = 0x0080;
    *REG_SOUNDCNT_L = 0xFF77;
    *REG_SOUNDCNT_H = 0x0002;

    /* 4. Clear VRAM and Backbuffer safely with native 16-bit stores */
    for (int i = 0; i < (GBA_SCREEN_W * GBA_SCREEN_H); ++i) {
        VRAM[i] = 0;
        s_backbuffer[i] = 0;
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
    if (club_type == 0) {
        /* Driver: Dual-channel crack + explosive sub-bass cavity punch */
        *REG_SOUND4CNT_L = 0xF100; /* Noise Channel 4: Max volume titanium face crack */
        *REG_SOUND4CNT_H = 0x8011;
        *REG_SOUND1CNT_L = 0x003A; /* Sweep down, time 3, shift 2 */
        *REG_SOUND1CNT_H = 0xF280; /* 50% duty, vol 15, punch decay */
        *REG_SOUND1CNT_X = 0x8640; /* Low punch dropping down */
    } else if (club_type == 4) {
        /* Putter: Soft click tap on noise + gentle chime on Channel 2 */
        *REG_SOUND4CNT_L = 0x6100;
        *REG_SOUND4CNT_H = 0x8045;
        *REG_SOUND2CNT_L = 0xB180;
        *REG_SOUND2CNT_H = 0x8540;
    } else if (club_type == 2) {
        /* Wedge: High face groove zip on Channel 2 + turf divot slice on Channel 4 */
        *REG_SOUND4CNT_L = 0xC100;
        *REG_SOUND4CNT_H = 0x8023;
        *REG_SOUND2CNT_L = 0xD180;
        *REG_SOUND2CNT_H = 0x8750;
    } else {
        /* Iron: Forged steel ping on Channel 2 + solid turf shear on Channel 4 */
        *REG_SOUND4CNT_L = 0xD100;
        *REG_SOUND4CNT_H = 0x8014;
        *REG_SOUND2CNT_L = 0xE180;
        *REG_SOUND2CNT_H = 0x8600;
    }
}

void gba_sfx_ball_landing(void) {
    /* Initial Ball Impact on Turf: Heavy downward acoustic thud + ground rustle */
    *REG_SOUND1CNT_L = 0x002A; /* Downward sweep (bit 3=1), time 2, shifts 2 */
    *REG_SOUND1CNT_H = 0xE180; /* 50% duty, vol 14, quick punch decay */
    *REG_SOUND1CNT_X = 0x8460; /* Low thump: ~250 Hz down to 80 Hz */
    *REG_SOUND4CNT_L = 0x9100; /* Low-frequency turf contact rustle */
    *REG_SOUND4CNT_H = 0x8052;
}

void gba_sfx_pin_ding(void) {
    /* Channel 2: High crystal bell ding on placing aim flag (C7 chime: ~2093 Hz) */
    *REG_SOUND2CNT_L = 0xE280; /* 50% duty, vol 14, decay 2 */
    *REG_SOUND2CNT_H = 0x8772; /* C7 crystal bell frequency */
}

void gba_sfx_bounce(void) {
    /* Secondary hop / UI bounce on Channel 1 */
    *REG_SOUND1CNT_L = 0x001A; /* Sweep down, time 1 */
    *REG_SOUND1CNT_H = 0xB180; /* 50% duty, vol 11, quick decay */
    *REG_SOUND1CNT_X = 0x8540; /* ~700 Hz */
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

void gba_sfx_burp(void) {
    /* Channel 1: Deep guttural burp downward sweep */
    *REG_SOUND1CNT_L = 0x007F; /* Maximum steep downward sweep */
    *REG_SOUND1CNT_H = 0xF580; /* Volume 15, low pitch decay */
    *REG_SOUND1CNT_X = 0x8200; /* Low bass guttural burp ~120 Hz */
}

void gba_sfx_boomstick(void) {
    /* Ash Shotgun Boomstick: Channel 4 explosive noise blast + Channel 1 punch */
    *REG_SOUND4CNT_L = 0xF100; /* Max volume */
    *REG_SOUND4CNT_H = 0x8031; /* Explosive wideband noise */
    *REG_SOUND1CNT_L = 0x003A; /* Fast downward sweep */
    *REG_SOUND1CNT_H = 0xF380; /* Vol 15, punch */
    *REG_SOUND1CNT_X = 0x8500;
}

void gba_sfx_play_ambient(int ambient_type) {
    (void)ambient_type;
}

/* ========================================================================= */
/* CADDYSHACK THEME MUSIC SEQUENCER ("I'm Alright" by Kenny Loggins)         */
/* ========================================================================= */
/* CADDYSHACK THEME MUSIC SEQUENCER ("I'm Alright" Chorus by Kenny Loggins)  */
/* ========================================================================= */

#define GBA_NOTE_REST  0
#define GBA_NOTE_C3    1113
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

/* Iconic "I'm Alright (Theme from Caddyshack)" Chorus Hook (64 steps) */
static const uint16_t s_melody_lead[64] = {
    /* Bar 1: "I'm alright... nobody worry 'bout me" */
    GBA_NOTE_B4,   GBA_NOTE_B4,   GBA_NOTE_B4,   GBA_NOTE_REST,
    GBA_NOTE_B4,   GBA_NOTE_REST, GBA_NOTE_A4,   GBA_NOTE_REST,
    GBA_NOTE_G4,   GBA_NOTE_REST, GBA_NOTE_E4,   GBA_NOTE_REST,
    GBA_NOTE_REST, GBA_NOTE_REST, GBA_NOTE_REST, GBA_NOTE_REST,

    /* Bar 2: "Nobody worry 'bout me..." */
    GBA_NOTE_REST, GBA_NOTE_REST, GBA_NOTE_E4,   GBA_NOTE_REST,
    GBA_NOTE_G4,   GBA_NOTE_REST, GBA_NOTE_A4,   GBA_NOTE_REST,
    GBA_NOTE_G4,   GBA_NOTE_REST, GBA_NOTE_E4,   GBA_NOTE_REST,
    GBA_NOTE_REST, GBA_NOTE_REST, GBA_NOTE_REST, GBA_NOTE_REST,

    /* Bar 3: "Why you got to give me a fight..." */
    GBA_NOTE_B4,   GBA_NOTE_B4,   GBA_NOTE_B4,   GBA_NOTE_REST,
    GBA_NOTE_B4,   GBA_NOTE_REST, GBA_NOTE_A4,   GBA_NOTE_REST,
    GBA_NOTE_G4,   GBA_NOTE_REST, GBA_NOTE_E4,   GBA_NOTE_REST,
    GBA_NOTE_REST, GBA_NOTE_REST, GBA_NOTE_REST, GBA_NOTE_REST,

    /* Bar 4: "Can't you just let it be..." */
    GBA_NOTE_E4,   GBA_NOTE_REST, GBA_NOTE_G4,   GBA_NOTE_REST,
    GBA_NOTE_A4,   GBA_NOTE_REST, GBA_NOTE_B4,   GBA_NOTE_REST,
    GBA_NOTE_D5,   GBA_NOTE_REST, GBA_NOTE_B4,   GBA_NOTE_REST,
    GBA_NOTE_A4,   GBA_NOTE_G4,   GBA_NOTE_E4,   GBA_NOTE_REST
};

/* Funk slap bassline supporting chorus progression */
static const uint16_t s_melody_bass[64] = {
    /* Bar 1: Em slap groove */
    GBA_NOTE_E3,   GBA_NOTE_REST, GBA_NOTE_E3,   GBA_NOTE_REST,
    GBA_NOTE_REST, GBA_NOTE_REST, GBA_NOTE_G3,   GBA_NOTE_REST,
    GBA_NOTE_E3,   GBA_NOTE_REST, GBA_NOTE_REST, GBA_NOTE_REST,
    GBA_NOTE_D4,   GBA_NOTE_REST, GBA_NOTE_E3,   GBA_NOTE_REST,

    /* Bar 2: Am groove */
    GBA_NOTE_E3,   GBA_NOTE_REST, GBA_NOTE_E3,   GBA_NOTE_REST,
    GBA_NOTE_REST, GBA_NOTE_REST, GBA_NOTE_A3,   GBA_NOTE_REST,
    GBA_NOTE_E3,   GBA_NOTE_REST, GBA_NOTE_REST, GBA_NOTE_REST,
    GBA_NOTE_B3,   GBA_NOTE_REST, GBA_NOTE_E3,   GBA_NOTE_REST,

    /* Bar 3: Em slap groove */
    GBA_NOTE_E3,   GBA_NOTE_REST, GBA_NOTE_E3,   GBA_NOTE_REST,
    GBA_NOTE_REST, GBA_NOTE_REST, GBA_NOTE_G3,   GBA_NOTE_REST,
    GBA_NOTE_E3,   GBA_NOTE_REST, GBA_NOTE_REST, GBA_NOTE_REST,
    GBA_NOTE_D4,   GBA_NOTE_REST, GBA_NOTE_E3,   GBA_NOTE_REST,

    /* Bar 4: Turnaround */
    GBA_NOTE_C3,   GBA_NOTE_REST, GBA_NOTE_C3,   GBA_NOTE_REST,
    GBA_NOTE_D4,   GBA_NOTE_REST, GBA_NOTE_D4,   GBA_NOTE_REST,
    GBA_NOTE_E3,   GBA_NOTE_REST, GBA_NOTE_G3,   GBA_NOTE_REST,
    GBA_NOTE_A3,   GBA_NOTE_REST, GBA_NOTE_B3,   GBA_NOTE_REST
};

void gba_audio_tick_splash(int tick) {
    /* 16th note triggers every 7 frames at 60 FPS (~128.5 BPM, exact tempo of Caddyshack theme) */
    if (tick % 7 != 0) return;
    int step = (tick / 7) % 64;

    /* Lead Synth on Channel 2 (50% duty, full volume 15, crisp decay) */
    uint16_t lead_freq = s_melody_lead[step];
    if (lead_freq > 0) {
        *REG_SOUND2CNT_L = 0xF280; /* 50% duty, vol 15, decay */
        *REG_SOUND2CNT_H = 0x8000 | lead_freq;
    } else {
        *REG_SOUND2CNT_L = 0; /* Clean rest cut for crisp syncopation */
    }

    /* Slap Bass on Channel 1 */
    uint16_t bass_freq = s_melody_bass[step];
    if (bass_freq > 0) {
        *REG_SOUND1CNT_L = 0x0000;
        *REG_SOUND1CNT_H = 0xE280; /* 50% duty, vol 14, decay */
        *REG_SOUND1CNT_X = 0x8000 | bass_freq;
    } else {
        *REG_SOUND1CNT_H = 0; /* Clean rest cut */
    }

    /* Percussion Beat on Channel 4 */
    if ((step & 3) == 2) {
        *REG_SOUND4CNT_L = 0xE100; /* Snare drum */
        *REG_SOUND4CNT_H = 0x8021;
    } else if ((step & 3) == 0) {
        *REG_SOUND4CNT_L = 0xB100; /* Kick / Hi-hat */
        *REG_SOUND4CNT_H = 0x8011;
    }
}

/* Ambient Nature Soundscape on GBA (intermittent bird chirps & light breeze with intervals of quietude) */
static int s_ambient_timer = 60 * 4; /* First ambient sound in ~4s */
static int s_ambient_counter = 0;

void gba_audio_tick_ambient(void) {
    s_ambient_timer--;
    if (s_ambient_timer <= 0) {
        s_ambient_counter++;
        if ((s_ambient_counter & 1) == 0) {
            /* Sweet little high bird chirp on Channel 1 */
            *REG_SOUND1CNT_L = 0x0011; /* Slight upward chirp */
            *REG_SOUND1CNT_H = 0x8180; /* Gentle volume 8, quick decay */
            *REG_SOUND1CNT_X = 0x8740; /* High natural frequency ~2400 Hz */
            /* Quiet interval of silence: ~7 to 14 seconds */
            s_ambient_timer = 60 * 7 + ((s_ambient_counter * 37) % (60 * 7));
        } else {
            /* Gentle light breeze whisper through trees on Channel 4 */
            *REG_SOUND4CNT_L = 0x4300; /* Soft volume 4, slow decay */
            *REG_SOUND4CNT_H = 0x8043; /* Soft broad-spectrum airy rustle */
            /* Quiet interval of silence: ~8 to 16 seconds */
            s_ambient_timer = 60 * 8 + ((s_ambient_counter * 43) % (60 * 8));
        }
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

