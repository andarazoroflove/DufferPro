#ifndef GOLF_RENDER_H
#define GOLF_RENDER_H

#include <stdint.h>
#include <stdbool.h>
#include "golf_core.h"

/* Native Desktop Resolution */
#define SCREEN_W            640
#define SCREEN_H            480

/* Isometric Tile Geometry (2:1 Ratio) */
#define TILE_W              64
#define TILE_H              32
#define TILE_HALF_W         32
#define TILE_HALF_H         16

/* Render Context */
typedef struct {
    uint32_t *framebuffer;  /* 640x480 32-bit ARGB (0xAARRGGBB) */
    int32_t camera_x;
    int32_t camera_y;
    int32_t mouse_gx;       /* Hovered grid tile */
    int32_t mouse_gy;
    int32_t mouse_sx;       /* Screen mouse X for UI hover */
    int32_t mouse_sy;       /* Screen mouse Y for UI hover */
    bool mouse_valid;
    uint32_t tick_count;
} RenderContext;

/* Core Renderer Prototypes */
void render_init(RenderContext *rc, uint32_t *buffer);
void render_free(RenderContext *rc);

/* Isometric coordinate conversions */
void grid_to_screen(int32_t gx, int32_t gy, int32_t elevation, int32_t cam_x, int32_t cam_y, int32_t *sx, int32_t *sy);
bool screen_to_grid(const GolfState *gs, int32_t sx, int32_t sy, int32_t cam_x, int32_t cam_y, int32_t *out_gx, int32_t *out_gy);

/* Main Draw Entrypoint */
void render_frame(RenderContext *rc, const GolfState *gs);

/* Camera helpers */
void render_center_camera_on(RenderContext *rc, int32_t gx, int32_t gy, int32_t elevation);
void render_pan_camera(RenderContext *rc, int32_t dx, int32_t dy);

/* UI Text Blitter */
void draw_text(RenderContext *rc, int32_t x, int32_t y, const char *str, uint32_t color, bool shadow);
void draw_rect(RenderContext *rc, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
void draw_rect_border(RenderContext *rc, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);

/* Screenshot helper (saves 640x480 BMP) */
bool render_save_bmp(const RenderContext *rc, const char *filename);

/* Splash Screen Renderer */
void render_splash_screen(RenderContext *rc, uint32_t tick_count);

#endif /* GOLF_RENDER_H */
