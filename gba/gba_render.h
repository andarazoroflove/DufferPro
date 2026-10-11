#ifndef GBA_RENDER_H
#define GBA_RENDER_H

#include "gba_hw.h"
#include "golf_core.h"

#define GBA_TILE_W        32
#define GBA_TILE_H        16
#define GBA_TILE_HALF_W   16
#define GBA_TILE_HALF_H   8
#define GBA_HEIGHT_STEP   4

typedef struct {
    int32_t cam_x;
    int32_t cam_y;
    int32_t target_cam_x;
    int32_t target_cam_y;
    int32_t tick_count;
    bool aim_locked;          /* True when aim target pin is locked at ball */
    int32_t aim_target_gx;    /* Tile coordinates of pinned target flag */
    int32_t aim_target_gy;
    int32_t cross_gx;         /* Tile under center crosshair during aiming */
    int32_t cross_gy;
    int32_t cross_dist_yds;   /* Distance from ball to crosshair in yards */
    bool pin_magnet_locked;   /* True when crosshair is magnetically snapped to cup pin */
    int32_t flag_linger_timer;/* Frames to linger at target flag before camera returns to ball */
    
    /* Look mode compatibility */
    bool look_mode;
    int32_t look_cam_x;
    int32_t look_cam_y;
    int32_t look_center_gx;
    int32_t look_center_gy;
    int32_t look_dist_yds;
} GbaRenderContext;

void gba_render_init(GbaRenderContext *grc);
void gba_render_draw_frame(GbaRenderContext *grc, const GolfState *gs);
void gba_render_splash(GbaRenderContext *grc, int anim_tick, int menu_choice);

/* UI Helpers */
void gba_draw_text(uint16_t *fb, int x, int y, const char *str, uint16_t color, bool shadow);
void gba_draw_rect(uint16_t *fb, int x, int y, int w, int h, uint16_t color);

#endif /* GBA_RENDER_H */

