#ifndef GBA_HW_H
#define GBA_HW_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define GBA_SCREEN_W 240
#define GBA_SCREEN_H 160

/* Direct Color 15-bit (BGR555) */
#define RGB15(r, g, b) ((uint16_t)(((b) << 10) | ((g) << 5) | (r)))
#define RGB24(r, g, b) RGB15(((r) >> 3), ((g) >> 3), ((b) >> 3))

/* Basic Palette Colors */
#define GBA_BLACK       RGB15(0, 0, 0)
#define GBA_WHITE       RGB15(31, 31, 31)
#define GBA_RED         RGB15(31, 5, 5)
#define GBA_GREEN       RGB15(4, 26, 6)
#define GBA_LIGHT_GREEN RGB15(8, 30, 10)
#define GBA_DARK_GREEN  RGB15(2, 16, 4)
#define GBA_BLUE        RGB15(4, 14, 30)
#define GBA_SKY_BLUE    RGB15(12, 22, 31)
#define GBA_WATER_BLUE  RGB15(6, 18, 28)
#define GBA_SAND_YELLOW RGB15(30, 27, 16)
#define GBA_YELLOW      RGB15(31, 30, 8)
#define GBA_GOLD        RGB15(30, 24, 6)
#define GBA_ORANGE      RGB15(31, 16, 4)
#define GBA_BROWN       RGB15(16, 10, 5)
#define GBA_DARK_GRAY   RGB15(6, 6, 8)
#define GBA_GRAY        RGB15(16, 16, 18)

/* GBA Hardware Registers */
#define REG_DISPCNT     ((volatile uint16_t*)0x04000000)
#define REG_DISPSTAT    ((volatile uint16_t*)0x04000004)
#define REG_VCOUNT      ((volatile uint16_t*)0x04000006)
#define REG_KEYINPUT    ((volatile uint16_t*)0x04000130)
#define REG_IE          ((volatile uint16_t*)0x04000200)
#define REG_IF          ((volatile uint16_t*)0x04000202)
#define REG_WAITCNT     ((volatile uint16_t*)0x04000204)
#define REG_IME         ((volatile uint16_t*)0x04000208)

/* GBA Timer & DMA Registers */
#define REG_TM0CNT_L    ((volatile uint16_t*)0x04000100)
#define REG_TM0CNT_H    ((volatile uint16_t*)0x04000102)
#define REG_DMA1SAD     ((volatile uint32_t*)0x040000BC)
#define REG_DMA1DAD     ((volatile uint32_t*)0x040000C0)
#define REG_DMA1CNT_L   ((volatile uint16_t*)0x040000C4)
#define REG_DMA1CNT_H   ((volatile uint16_t*)0x040000C6)
#define REG_DMA3SAD     ((volatile uint32_t*)0x040000D4)
#define REG_DMA3DAD     ((volatile uint32_t*)0x040000D8)
#define REG_DMA3CNT_L   ((volatile uint16_t*)0x040000DC)
#define REG_DMA3CNT_H   ((volatile uint16_t*)0x040000DE)
#define REG_FIFO_A      ((volatile uint32_t*)0x040000A0)

/* GBA Sound Registers */
#define REG_SOUND1CNT_L ((volatile uint16_t*)0x04000060)
#define REG_SOUND1CNT_H ((volatile uint16_t*)0x04000062)
#define REG_SOUND1CNT_X ((volatile uint16_t*)0x04000064)
#define REG_SOUND2CNT_L ((volatile uint16_t*)0x04000068)
#define REG_SOUND2CNT_H ((volatile uint16_t*)0x0400006C)
#define REG_SOUND4CNT_L ((volatile uint16_t*)0x04000078)
#define REG_SOUND4CNT_H ((volatile uint16_t*)0x0400007C)
#define REG_SOUNDCNT_L  ((volatile uint16_t*)0x04000080)
#define REG_SOUNDCNT_H  ((volatile uint16_t*)0x04000082)
#define REG_SOUNDCNT_X  ((volatile uint16_t*)0x04000084)

/* VRAM Base in Mode 3 (16-bit 240x160 bitmap) */
#define VRAM            ((volatile uint16_t*)0x06000000)

/* Key Definitions (Active Low) */
#define KEY_A           (1 << 0)
#define KEY_B           (1 << 1)
#define KEY_SELECT      (1 << 2)
#define KEY_START       (1 << 3)
#define KEY_RIGHT       (1 << 4)
#define KEY_LEFT        (1 << 5)
#define KEY_UP          (1 << 6)
#define KEY_DOWN        (1 << 7)
#define KEY_R           (1 << 8)
#define KEY_L           (1 << 9)

/* Hardware Init & Video */
void gba_hw_init(void);
void gba_vsync(void);
void gba_flip(void);
uint16_t* gba_get_backbuffer(void);

/* Input */
void gba_poll_keys(void);
bool gba_key_held(uint16_t key);
bool gba_key_pressed(uint16_t key);

/* Hardware Sound Effects */
void gba_sfx_swing_hit(int club_type);
void gba_sfx_ball_landing(void);
void gba_sfx_pin_ding(void);
void gba_sfx_bounce(void);
void gba_sfx_cup_sink(void);
void gba_sfx_water_splash(void);
void gba_sfx_gopher_chuckle(void);
void gba_sfx_gopher_gulp(void);
void gba_sfx_carl_explosion(void);
void gba_sfx_carl_hose(void);
void gba_sfx_whistle(void);
void gba_sfx_burp(void);
void gba_sfx_boomstick(void);
void gba_sfx_play_ambient(int ambient_type);
void gba_audio_tick_ambient(void);
void gba_audio_tick_splash(int tick);
void gba_audio_stop_music(void);

#endif /* GBA_HW_H */
