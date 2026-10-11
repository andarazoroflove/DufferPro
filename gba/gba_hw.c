#include "gba_hw.h"
#include "gba_audio.h"

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

    /* 2. Configure ROM waitstates and ENABLE HARDWARE PREFETCH BUFFER (0x4314: 3-1 cycle ROM + prefetch) */
    *REG_WAITCNT = 0x4314;

    /* 3. Set Video Mode 3 (240x160 15-bit color bitmap) + BG2 layer enable */
    *REG_DISPCNT = 0x0403;

    /* 4. Initialize Sound Hardware (Master Enable + 100% Volume on all PSG channels) */
    *REG_SOUNDCNT_X = 0x0080;
    *REG_SOUNDCNT_L = 0xFF77;
    *REG_SOUNDCNT_H = 0x0002;

    /* 5. Initialize DirectSound A 16 kHz DMA Audio Engine */
    gba_audio_init();

    /* 6. Clear VRAM and Backbuffer safely with native 16-bit stores */
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

    /* Hardware DMA3 Transfer: 19,200 32-bit words at silicon bus speed */
    *REG_DMA3CNT_H = 0; /* Clear previous DMA */
    *REG_DMA3SAD = (uint32_t)s_backbuffer;
    *REG_DMA3DAD = (uint32_t)VRAM;
    *REG_DMA3CNT_L = (GBA_SCREEN_W * GBA_SCREEN_H) / 2;
    *REG_DMA3CNT_H = 0x8400; /* Enable DMA3, 32-bit word transfer */

    /* Update DirectSound multi-channel audio mixer buffer for this frame */
    gba_audio_update();
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
        /* Driver: DirectSound 16kHz PCM titanium crack + PSG sub-bass punch */
        gba_audio_play_sfx(GBA_SFX_DRIVER, 256);
        *REG_SOUND4CNT_L = 0xF100;
        *REG_SOUND4CNT_H = 0x8011;
        *REG_SOUND1CNT_L = 0x003A;
        *REG_SOUND1CNT_H = 0xF280;
        *REG_SOUND1CNT_X = 0x8640;
    } else if (club_type == 4) {
        /* Putter: DirectSound 16kHz PCM hollow click + gentle chime */
        gba_audio_play_sfx(GBA_SFX_PUTTER, 220);
        *REG_SOUND4CNT_L = 0x6100;
        *REG_SOUND4CNT_H = 0x8045;
        *REG_SOUND2CNT_L = 0xB180;
        *REG_SOUND2CNT_H = 0x8540;
    } else if (club_type == 2) {
        /* Wedge: DirectSound 16kHz PCM groove zip + turf divot slice */
        gba_audio_play_sfx(GBA_SFX_WEDGE, 240);
        *REG_SOUND4CNT_L = 0xC100;
        *REG_SOUND4CNT_H = 0x8023;
        *REG_SOUND2CNT_L = 0xD180;
        *REG_SOUND2CNT_H = 0x8750;
    } else {
        /* Iron: DirectSound 16kHz PCM forged steel ping + turf shear */
        gba_audio_play_sfx(GBA_SFX_IRON, 250);
        *REG_SOUND4CNT_L = 0xD100;
        *REG_SOUND4CNT_H = 0x8014;
        *REG_SOUND2CNT_L = 0xE180;
        *REG_SOUND2CNT_H = 0x8710;
    }
}

void gba_sfx_ball_landing(void) {
    /* DirectSound 16kHz PCM heavy turf impact + PSG low thump */
    gba_audio_play_sfx(GBA_SFX_BALL_THUD, 256);
    *REG_SOUND1CNT_L = 0x002A;
    *REG_SOUND1CNT_H = 0xE180;
    *REG_SOUND1CNT_X = 0x8460;
    *REG_SOUND4CNT_L = 0x9100;
    *REG_SOUND4CNT_H = 0x8052;
}

void gba_sfx_pin_ding(void) {
    /* DirectSound 16kHz PCM crystal bell chime + Channel 2 ding */
    gba_audio_play_sfx(GBA_SFX_PIN_DING, 256);
    *REG_SOUND2CNT_L = 0xE280;
    *REG_SOUND2CNT_H = 0x8772;
}

void gba_sfx_bounce(void) {
    /* Secondary hop / UI bounce on Channel 1 */
    *REG_SOUND1CNT_L = 0x001A;
    *REG_SOUND1CNT_H = 0xB180;
    *REG_SOUND1CNT_X = 0x8540;
}

void gba_sfx_cup_sink(void) {
    /* DirectSound 16kHz PCM double cup drop rattle + Channel 2 tone */
    gba_audio_play_sfx(GBA_SFX_CUP_SINK, 256);
    *REG_SOUND2CNT_L = 0xB200;
    *REG_SOUND2CNT_H = 0x8680;
}

void gba_sfx_water_splash(void) {
    /* DirectSound 16kHz PCM whitewater splash + Channel 4 noise */
    gba_audio_play_sfx(GBA_SFX_WATER_SPLASH, 256);
    *REG_SOUND4CNT_L = 0xE300;
    *REG_SOUND4CNT_H = 0x8052;
}

void gba_sfx_gopher_chuckle(void) {
    /* DirectSound 16kHz PCM staccato laughter + Channel 1 Caddyshack chirp */
    gba_audio_play_sfx(GBA_SFX_GOPHER_CHUCKLE, 240);
    *REG_SOUND1CNT_L = 0x0011;
    *REG_SOUND1CNT_H = 0xC100;
    *REG_SOUND1CNT_X = 0x8760;
}

void gba_sfx_gopher_gulp(void) {
    /* DirectSound 16kHz PCM subterranean drop + Channel 1 sweep */
    gba_audio_play_sfx(GBA_SFX_GOPHER_GULP, 240);
    *REG_SOUND1CNT_L = 0x0074;
    *REG_SOUND1CNT_H = 0xF200;
    *REG_SOUND1CNT_X = 0x8600;
}

void gba_sfx_carl_explosion(void) {
    /* DirectSound 16kHz PCM massive detonation + Channel 4 rumble */
    gba_audio_play_sfx(GBA_SFX_CARL_EXPLOSION, 256);
    *REG_SOUND4CNT_L = 0xF500;
    *REG_SOUND4CNT_H = 0x8071;
    *REG_SOUND1CNT_L = 0x0077;
    *REG_SOUND1CNT_H = 0xF400;
    *REG_SOUND1CNT_X = 0x8180;
}

void gba_sfx_carl_hose(void) {
    /* Carl High-Pressure Water Hose Spray */
    *REG_SOUND4CNT_L = 0xB100;
    *REG_SOUND4CNT_H = 0x8043;
}

void gba_sfx_whistle(void) {
    /* DirectSound 16kHz PCM two-tone whistle + Channel 2 clear whistle */
    gba_audio_play_sfx(GBA_SFX_BEV_WHISTLE, 250);
    *REG_SOUND2CNT_L = 0xA200;
    *REG_SOUND2CNT_H = 0x8740;
}

void gba_sfx_burp(void) {
    /* DirectSound 16kHz PCM guttural vocal fry burp */
    gba_audio_play_sfx(GBA_SFX_GOLFER_BURP, 256);
}

void gba_sfx_boomstick(void) {
    /* DirectSound 16kHz PCM Ash shotgun blast & Groovy vocal resonance */
    gba_audio_play_sfx(GBA_SFX_BOOMSTICK, 256);
}

void gba_sfx_play_ambient(int ambient_type) {
    if (ambient_type == 1) {
        gba_audio_play_ambient(GBA_SFX_AMBIENT_WIND, 130);
    } else if (ambient_type == 2) {
        gba_audio_play_ambient(GBA_SFX_AMBIENT_WATER, 130);
    } else {
        gba_audio_play_ambient(GBA_SFX_NONE, 0);
    }
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

