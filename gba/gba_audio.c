#include "gba_audio.h"
#include "gba_sound_data.h"
#include "gba_hw.h"

/* GBA Hardware Audio Registers */
#define REG_FIFO_A_ADDR     0x040000A0
#define REG_DMA1SAD_P       ((volatile uint32_t*)0x040000BC)
#define REG_DMA1DAD_P       ((volatile uint32_t*)0x040000C0)
#define REG_DMA1CNT_L_P     ((volatile uint16_t*)0x040000C4)
#define REG_DMA1CNT_H_P     ((volatile uint16_t*)0x040000C6)
#define REG_TM0CNT_L_P      ((volatile uint16_t*)0x04000100)
#define REG_TM0CNT_H_P      ((volatile uint16_t*)0x04000102)

/* 16,000 Hz: 16777216 / 16000 = 1048.576 -> 65536 - 1048 = 64488 (0xFBE8) */
#define TM0_RELOAD_16KHZ    0xFBE8

/* DirectSound DMA Circular Stream Buffer (1024 samples = ~4 frames at 16 kHz) */
#define AUDIO_BUF_SIZE      1024
#define SAMPLES_PER_FRAME   267   /* 16,000 / 60 = 266.67 */

__attribute__((aligned(4))) static int8_t s_dma_stream_buf[AUDIO_BUF_SIZE];
static uint32_t s_write_cursor = 0;

/* Multi-channel DirectSound Voice Structure */
typedef struct {
    const int8_t *samples;
    uint32_t length;
    uint32_t position;
    int volume;       /* 0..256 */
    bool active;
    bool loop;
} GbaVoice;

#define MAX_GBA_VOICES 4
static GbaVoice s_voices[MAX_GBA_VOICES];

void gba_audio_init(void) {
    /* 1. Clear DMA stream buffer and voices */
    for (int i = 0; i < AUDIO_BUF_SIZE; ++i) {
        s_dma_stream_buf[i] = 0;
    }
    s_write_cursor = 0;
    for (int v = 0; v < MAX_GBA_VOICES; ++v) {
        s_voices[v].active = false;
        s_voices[v].samples = 0;
        s_voices[v].length = 0;
        s_voices[v].position = 0;
        s_voices[v].volume = 256;
        s_voices[v].loop = false;
    }

    /* 2. Turn Master Audio Circuit ON */
    *REG_SOUNDCNT_X = 0x0080;

    /* 3. Configure DirectSound A: Full volume, Left+Right speakers, Timer 0, Reset FIFO */
    /* Bit 0=1 (100% Vol), Bit 2=1 (Right ON), Bit 3=1 (Left ON), Bit 11=1 (FIFO Reset) -> 0x080D */
    *REG_SOUNDCNT_H = 0x080D;

    /* 4. Disable and configure DMA Channel 1 for DirectSound FIFO Auto-Refill */
    *REG_DMA1CNT_H_P = 0; /* Stop DMA */
    *REG_DMA1SAD_P = (uint32_t)s_dma_stream_buf;
    *REG_DMA1DAD_P = (uint32_t)REG_FIFO_A_ADDR;
    *REG_DMA1CNT_L_P = 0;
    
    /* 0xB640:
     * Bit 5-6 = 2 (Dest Fixed: 0x0040)
     * Bit 7-8 = 0 (Source Increment)
     * Bit 9   = 1 (Repeat: 0x0200)
     * Bit 10  = 1 (32-bit transfer: 0x0400)
     * Bit 12-13 = 3 (Special Start Timing / FIFO request: 0x3000)
     * Bit 15  = 1 (DMA Enable: 0x8000) */
    *REG_DMA1CNT_H_P = 0xB640;

    /* 5. Start Timer 0 to clock DirectSound DAC at 16,000 Hz */
    *REG_TM0CNT_H_P = 0; /* Stop Timer */
    *REG_TM0CNT_L_P = TM0_RELOAD_16KHZ;
    *REG_TM0CNT_H_P = 0x0080; /* Start Timer 0 (no prescaler, 16.78 MHz) */
}

static void get_sfx_asset(GbaSfxId id, const int8_t **out_samples, uint32_t *out_len) {
    switch (id) {
        case GBA_SFX_DRIVER:
            *out_samples = g_sfx_driver; *out_len = sizeof(g_sfx_driver); break;
        case GBA_SFX_IRON:
            *out_samples = g_sfx_iron; *out_len = sizeof(g_sfx_iron); break;
        case GBA_SFX_WEDGE:
            *out_samples = g_sfx_wedge; *out_len = sizeof(g_sfx_wedge); break;
        case GBA_SFX_PUTTER:
            *out_samples = g_sfx_putter; *out_len = sizeof(g_sfx_putter); break;
        case GBA_SFX_BALL_THUD:
            *out_samples = g_sfx_ball_thud; *out_len = sizeof(g_sfx_ball_thud); break;
        case GBA_SFX_CUP_SINK:
            *out_samples = g_sfx_cup_sink; *out_len = sizeof(g_sfx_cup_sink); break;
        case GBA_SFX_PIN_DING:
            *out_samples = g_sfx_pin_ding; *out_len = sizeof(g_sfx_pin_ding); break;
        case GBA_SFX_BEV_WHISTLE:
            *out_samples = g_sfx_bev_whistle; *out_len = sizeof(g_sfx_bev_whistle); break;
        case GBA_SFX_GOLFER_BURP:
            *out_samples = g_sfx_golfer_burp; *out_len = sizeof(g_sfx_golfer_burp); break;
        case GBA_SFX_BOOMSTICK:
            *out_samples = g_sfx_boomstick; *out_len = sizeof(g_sfx_boomstick); break;
        case GBA_SFX_GOPHER_CHUCKLE:
            *out_samples = g_sfx_gopher_chuckle; *out_len = sizeof(g_sfx_gopher_chuckle); break;
        case GBA_SFX_GOPHER_GULP:
            *out_samples = g_sfx_gopher_gulp; *out_len = sizeof(g_sfx_gopher_gulp); break;
        case GBA_SFX_CARL_EXPLOSION:
            *out_samples = g_sfx_carl_explosion; *out_len = sizeof(g_sfx_carl_explosion); break;
        case GBA_SFX_WATER_SPLASH:
            *out_samples = g_sfx_water_splash; *out_len = sizeof(g_sfx_water_splash); break;
        case GBA_SFX_AMBIENT_WIND:
            *out_samples = g_sfx_ambient_wind; *out_len = sizeof(g_sfx_ambient_wind); break;
        case GBA_SFX_AMBIENT_WATER:
            *out_samples = g_sfx_ambient_water; *out_len = sizeof(g_sfx_ambient_water); break;
        default:
            *out_samples = 0; *out_len = 0; break;
    }
}

void gba_audio_play_sfx(GbaSfxId sfx_id, int volume) {
    if (sfx_id == GBA_SFX_NONE) return;
    const int8_t *samples = 0;
    uint32_t len = 0;
    get_sfx_asset(sfx_id, &samples, &len);
    if (!samples || len == 0) return;

    /* Prioritize SFX channels 0, 1, 2 */
    int target_voice = -1;
    for (int v = 0; v < 3; ++v) {
        if (!s_voices[v].active) {
            target_voice = v;
            break;
        }
    }
    if (target_voice == -1) target_voice = 0; /* Preempt oldest channel */

    GbaVoice *vox = &s_voices[target_voice];
    vox->samples = samples;
    vox->length = len;
    vox->position = 0;
    vox->volume = volume > 256 ? 256 : (volume < 0 ? 0 : volume);
    vox->loop = false;
    vox->active = true;
}

void gba_audio_play_ambient(GbaSfxId ambient_id, int volume) {
    /* Channel 3 is dedicated to seamless looping ambient background */
    if (ambient_id == GBA_SFX_NONE) {
        s_voices[3].active = false;
        return;
    }
    const int8_t *samples = 0;
    uint32_t len = 0;
    get_sfx_asset(ambient_id, &samples, &len);
    if (!samples || len == 0) return;

    GbaVoice *vox = &s_voices[3];
    vox->samples = samples;
    vox->length = len;
    vox->position = 0;
    vox->volume = volume > 256 ? 256 : (volume < 0 ? 0 : volume);
    vox->loop = true;
    vox->active = true;
}

void gba_audio_stop_all(void) {
    for (int v = 0; v < MAX_GBA_VOICES; ++v) {
        s_voices[v].active = false;
    }
}

/* Call every frame to mix 267 audio samples ahead into circular DMA buffer */
void gba_audio_update(void) {
    for (int i = 0; i < SAMPLES_PER_FRAME; ++i) {
        int32_t mix = 0;
        for (int v = 0; v < MAX_GBA_VOICES; ++v) {
            GbaVoice *vox = &s_voices[v];
            if (vox->active && vox->samples) {
                int8_t s = vox->samples[vox->position];
                mix += (s * vox->volume) >> 8;
                vox->position++;
                if (vox->position >= vox->length) {
                    if (vox->loop) {
                        vox->position = 0;
                    } else {
                        vox->active = false;
                    }
                }
            }
        }
        
        /* Clamp to 8-bit signed range (-128..127) */
        if (mix > 127) mix = 127;
        if (mix < -128) mix = -128;

        s_dma_stream_buf[s_write_cursor] = (int8_t)mix;
        s_write_cursor = (s_write_cursor + 1) & (AUDIO_BUF_SIZE - 1);
    }
}

