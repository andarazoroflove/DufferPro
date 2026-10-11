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

static int s_swing_seq_timer = 0;
static int s_swing_club_type = 0;
static int s_cup_seq_timer = 0;

void gba_sfx_tick(void) {
    /* Keep sound circuit active at 100% volume */
    *REG_SOUNDCNT_X = 0x0080;
    *REG_SOUNDCNT_L = 0xFF77;
    *REG_SOUNDCNT_H = 0x0002;

    if (s_swing_seq_timer > 0) {
        s_swing_seq_timer--;
        if (s_swing_seq_timer == 1) {
            /* Stage 2: Metallic ring resonance / turf divot follow-through */
            if (s_swing_club_type == 0) {
                /* Driver: High titanium face resonance ring (2,750 Hz) */
                *REG_SOUND2CNT_L = 0xE280;
                *REG_SOUND2CNT_H = 0x8758;
            } else if (s_swing_club_type == 1) {
                /* Iron: Forged carbon steel ping (2,150 Hz) */
                *REG_SOUND2CNT_L = 0xD280;
                *REG_SOUND2CNT_H = 0x86E8;
            } else if (s_swing_club_type == 2) {
                /* Wedge: High-spin groove zip (3,600 Hz) */
                *REG_SOUND2CNT_L = 0xC180;
                *REG_SOUND2CNT_H = 0x8798;
            }
        }
    }
    
    if (s_cup_seq_timer > 0) {
        s_cup_seq_timer--;
        if (s_cup_seq_timer == 3) {
            /* Stage 2 of Cup Sink: Cup bottom plastic rattle & settle */
            *REG_SOUND2CNT_L = 0xD280;
            *REG_SOUND2CNT_H = 0x86E0; /* 1,650 Hz */
            *REG_SOUND1CNT_L = 0x0019;
            *REG_SOUND1CNT_H = 0xB180;
            *REG_SOUND1CNT_X = 0x8400; /* Low hollow cup thump */
        }
    }
}

void gba_sfx_swing_hit(int club_type) {
    *REG_SOUNDCNT_X = 0x0080;
    *REG_SOUNDCNT_L = 0xFF77;
    *REG_SOUNDCNT_H = 0x0002;

    s_swing_club_type = club_type;
    s_swing_seq_timer = 3; /* Multi-frame acoustic progression */

    if (club_type == 0) {
        /* Driver: Explosive titanium face crack (16.4 kHz white noise) + sub-bass punch */
        *REG_SOUND4CNT_L = 0xF100; /* Max volume 15 */
        *REG_SOUND4CNT_H = 0x8041; /* s=4, r=1 -> 16,384 Hz crisp crack */
        *REG_SOUND1CNT_L = 0x0019; /* Fast downward sweep */
        *REG_SOUND1CNT_H = 0xF180; /* Volume 15, instant punch decay */
        *REG_SOUND1CNT_X = 0x8680; /* Drops from high compression down to sub-bass */
    } else if (club_type == 4) {
        /* Putter: Solid urethane/wood contact "TOCK" */
        *REG_SOUND4CNT_L = 0x8100;
        *REG_SOUND4CNT_H = 0x8063; /* s=6, r=3 -> 1,365 Hz audible click */
        *REG_SOUND1CNT_L = 0x0000;
        *REG_SOUND1CNT_H = 0xA180;
        *REG_SOUND1CNT_X = 0x8380; /* Warm low pop ~280 Hz */
        *REG_SOUND2CNT_L = 0x8180;
        *REG_SOUND2CNT_H = 0x8520;
    } else if (club_type == 2) {
        /* Wedge: High-frequency groove spin zip + grass bite */
        *REG_SOUND4CNT_L = 0xD100;
        *REG_SOUND4CNT_H = 0x8042; /* s=4, r=2 -> 8,192 Hz groove cut */
        *REG_SOUND1CNT_L = 0x002B;
        *REG_SOUND1CNT_H = 0xB180;
        *REG_SOUND1CNT_X = 0x8580;
    } else {
        /* Iron: Forged steel ping + solid turf divot shear */
        *REG_SOUND4CNT_L = 0xE100;
        *REG_SOUND4CNT_H = 0x8052; /* s=5, r=2 -> 4,096 Hz divot crunch */
        *REG_SOUND1CNT_L = 0x002A;
        *REG_SOUND1CNT_H = 0xE180;
        *REG_SOUND1CNT_X = 0x8540;
    }
}

void gba_sfx_ball_landing(void) {
    *REG_SOUNDCNT_X = 0x0080;
    *REG_SOUNDCNT_L = 0xFF77;
    *REG_SOUNDCNT_H = 0x0002;

    /* Initial Ball Impact on Turf: Heavy downward acoustic thud + ground rustle */
    *REG_SOUND1CNT_L = 0x002A; /* Downward sweep (bit 3=1), time 2, shifts 2 */
    *REG_SOUND1CNT_H = 0xF180; /* Vol 15, heavy punch decay */
    *REG_SOUND1CNT_X = 0x8400; /* Low thump: drops to sub-bass ~80 Hz */
    *REG_SOUND4CNT_L = 0xC100; /* Low-frequency turf contact rustle */
    *REG_SOUND4CNT_H = 0x8062; /* s=6, r=2 -> 2,048 Hz grass compression */
}

void gba_sfx_pin_ding(void) {
    *REG_SOUNDCNT_X = 0x0080;
    *REG_SOUNDCNT_L = 0xFF77;
    *REG_SOUNDCNT_H = 0x0002;

    /* Channel 2: High crystalline C7 bell ding on placing aim flag (C7 chime: ~2093 Hz) */
    *REG_SOUND2CNT_L = 0xF480; /* 50% duty, vol 15, sustained shimmer decay */
    *REG_SOUND2CNT_H = 0x8772; /* C7 crystal bell frequency */
    
    /* Channel 1: Sympathetic harmonic overtone (E7: ~2637 Hz) */
    *REG_SOUND1CNT_L = 0x0000;
    *REG_SOUND1CNT_H = 0xD480; /* Vol 13, step 4 */
    *REG_SOUND1CNT_X = 0x878E;
}

void gba_sfx_bounce(void) {
    /* Secondary hop / UI bounce on Channel 1 */
    *REG_SOUND1CNT_L = 0x0019; /* Sweep down, time 1 */
    *REG_SOUND1CNT_H = 0xC180; /* 50% duty, vol 12, quick decay */
    *REG_SOUND1CNT_X = 0x8540; /* ~700 Hz */
}

void gba_sfx_cup_sink(void) {
    *REG_SOUNDCNT_X = 0x0080;
    *REG_SOUNDCNT_L = 0xFF77;
    *REG_SOUNDCNT_H = 0x0002;

    /* Stage 1: High metallic rim clink (2,400 Hz) */
    *REG_SOUND2CNT_L = 0xF280;
    *REG_SOUND2CNT_H = 0x8740;
    s_cup_seq_timer = 6; /* Stage 2 cup bottom rattle triggers in 3 frames */
}

void gba_sfx_water_splash(void) {
    /* Channel 4: White noise water splash + Channel 1 bubble gurgle */
    *REG_SOUND4CNT_L = 0xF300; /* High volume, turbulent splash */
    *REG_SOUND4CNT_H = 0x8052;
    *REG_SOUND1CNT_L = 0x0012; /* Upward bubble sweep */
    *REG_SOUND1CNT_H = 0xC180;
    *REG_SOUND1CNT_X = 0x8400;
}

void gba_sfx_ui_tick(void) {
    /* Subtle tactile mechanical click for D-pad aiming movement */
    *REG_SOUND4CNT_L = 0x7100;
    *REG_SOUND4CNT_H = 0x8043; /* s=4, r=3 -> 5,461 Hz crisp tick */
}

void gba_sfx_club_click(void) {
    /* Crisp mechanical club ratchet click */
    *REG_SOUND4CNT_L = 0xA100;
    *REG_SOUND4CNT_H = 0x8052; /* 4,096 Hz ratchet */
    *REG_SOUND2CNT_L = 0xB180;
    *REG_SOUND2CNT_H = 0x8660;
}

void gba_sfx_footstep(void) {
    /* Soft natural turf step */
    *REG_SOUND4CNT_L = 0x5100;
    *REG_SOUND4CNT_H = 0x8063; /* 1,365 Hz turf crunch */
}

void gba_sfx_tree_hit(void) {
    /* Solid wood trunk thwack */
    *REG_SOUND4CNT_L = 0xF100;
    *REG_SOUND4CNT_H = 0x8051;
    *REG_SOUND1CNT_L = 0x003A;
    *REG_SOUND1CNT_H = 0xD180;
    *REG_SOUND1CNT_X = 0x8480;
}

void gba_sfx_rock_hit(void) {
    /* High metallic boulder clang */
    *REG_SOUND4CNT_L = 0xF100;
    *REG_SOUND4CNT_H = 0x8049; /* 7-bit metallic chime noise */
    *REG_SOUND2CNT_L = 0xF280;
    *REG_SOUND2CNT_H = 0x8760; /* Loud metallic ring */
}

void gba_sfx_sweetspot(void) {
    /* Sweetspot Strike: Titanium Driver crack + high C8 crystal ping */
    *REG_SOUND4CNT_L = 0xF100;
    *REG_SOUND4CNT_H = 0x8041;
    *REG_SOUND2CNT_L = 0xF380;
    *REG_SOUND2CNT_H = 0x87F0;
    *REG_SOUND1CNT_L = 0x0019;
    *REG_SOUND1CNT_H = 0xF180;
    *REG_SOUND1CNT_X = 0x8680;
}

void gba_sfx_applause(void) {
    /* Warm golf clap crowd applause on Channel 4 */
    *REG_SOUND4CNT_L = 0xB200;
    *REG_SOUND4CNT_H = 0x8052;
}

void gba_sfx_siren(void) {
    /* Marshall Ranger Patrol Cart Siren */
    *REG_SOUND1CNT_L = 0x0013;
    *REG_SOUND1CNT_H = 0xE180;
    *REG_SOUND1CNT_X = 0x8680;
}

void gba_sfx_chainsaw(void) {
    /* Ash Chainsaw Engine Buzz on Channel 4 */
    *REG_SOUND4CNT_L = 0xF200;
    *REG_SOUND4CNT_H = 0x8039;
}

void gba_sfx_zombie(void) {
    /* Low gutter zombie groan */
    *REG_SOUND1CNT_L = 0x0055;
    *REG_SOUND1CNT_H = 0xD300;
    *REG_SOUND1CNT_X = 0x8180;
}

void gba_sfx_seamonster(void) {
    /* Subterranean sea monster rumble */
    *REG_SOUND4CNT_L = 0xF400;
    *REG_SOUND4CNT_H = 0x8061;
    *REG_SOUND1CNT_L = 0x0077;
    *REG_SOUND1CNT_H = 0xF400;
    *REG_SOUND1CNT_X = 0x8120;
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
    *REG_SOUND4CNT_L = 0xF500; /* Max volume, long rumble */
    *REG_SOUND4CNT_H = 0x8071; /* Heavy low-frequency noise */
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

