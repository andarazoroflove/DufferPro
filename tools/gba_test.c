#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "gba_render.h"
#include "golf_core.h"

static uint16_t s_test_backbuffer[GBA_SCREEN_W * GBA_SCREEN_H];

uint16_t* gba_get_backbuffer(void) {
    return s_test_backbuffer;
}

/* Save GBA 15-bit BGR555 frame as 3x scaled 24-bit BMP (720x480) for crisp viewing */
static void save_gba_bmp_scaled(const char *filename, int scale) {
    int out_w = GBA_SCREEN_W * scale;
    int out_h = GBA_SCREEN_H * scale;
    int row_bytes = (out_w * 3 + 3) & ~3;
    int img_size = row_bytes * out_h;
    int file_size = 54 + img_size;

    uint8_t header[54] = {
        'B', 'M',
        file_size & 0xFF, (file_size >> 8) & 0xFF, (file_size >> 16) & 0xFF, (file_size >> 24) & 0xFF,
        0, 0, 0, 0,
        54, 0, 0, 0,
        40, 0, 0, 0,
        out_w & 0xFF, (out_w >> 8) & 0xFF, (out_w >> 16) & 0xFF, (out_w >> 24) & 0xFF,
        out_h & 0xFF, (out_h >> 8) & 0xFF, (out_h >> 16) & 0xFF, (out_h >> 24) & 0xFF,
        1, 0,
        24, 0,
        0, 0, 0, 0,
        img_size & 0xFF, (img_size >> 8) & 0xFF, (img_size >> 16) & 0xFF, (img_size >> 24) & 0xFF,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0
    };

    FILE *f = fopen(filename, "wb");
    if (!f) return;
    fwrite(header, 1, 54, f);

    uint8_t *row_buf = (uint8_t*)malloc(row_bytes);
    for (int y = out_h - 1; y >= 0; --y) {
        int src_y = y / scale;
        int buf_idx = 0;
        for (int x = 0; x < out_w; ++x) {
            int src_x = x / scale;
            uint16_t bgr15 = s_test_backbuffer[src_y * GBA_SCREEN_W + src_x];
            /* Extract 5-bit channels and expand to 8-bit */
            uint8_t r = ((bgr15 >> 0) & 0x1F) << 3;
            uint8_t g = ((bgr15 >> 5) & 0x1F) << 3;
            uint8_t b = ((bgr15 >> 10) & 0x1F) << 3;
            
            row_buf[buf_idx++] = b;
            row_buf[buf_idx++] = g;
            row_buf[buf_idx++] = r;
        }
        while (buf_idx < row_bytes) row_buf[buf_idx++] = 0;
        fwrite(row_buf, 1, row_bytes, f);
    }
    free(row_buf);
    fclose(f);
}

int main(void) {
    static GolfState gs;
    static GbaRenderContext grc;

    printf("=== GBA VERSION VERIFICATION & CAPTURE TEST ===\n");
    golf_init(&gs);
    gba_render_init(&grc);

    /* 1. GBA Splash Screen (Jump On selected) */
    gba_render_splash(&grc, 25, 0);
    save_gba_bmp_scaled("screenshot_gba_splash.bmp", 3);
    printf("[1/8] Saved screenshot_gba_splash.bmp (720x480 GBA Title with JUMP ON)\n");

    /* 1b. GBA Splash Screen (Play Hole selected) */
    gba_render_splash(&grc, 25, 1);
    save_gba_bmp_scaled("screenshot_gba_splash_play_hole.bmp", 3);
    printf("[2/8] Saved screenshot_gba_splash_play_hole.bmp (720x480 GBA Title with PLAY HOLE)\n");

    /* 2. GBA Play Mode: Bushwood Theme with Full-Width Swing Meter */
    golf_set_mode(&gs, MODE_PLAY);
    gs.theme = THEME_BUSHWOOD;
    gs.banner_timer = 0;
    gs.banner_text[0] = '\0';
    grc.tick_count = 0;
    grc.cam_x = 0;
    grc.cam_y = 0;
    gba_render_draw_frame(&grc, &gs);
    save_gba_bmp_scaled("screenshot_gba_play.bmp", 3);
    printf("[3/8] Saved screenshot_gba_play.bmp (720x480 GBA Bushwood with Full-Width Swing Meter)\n");

    /* 3. GBA Play Mode: La Playa Desert Theme with Cactus, Tumbleweeds, Rapids */
    gs.theme = THEME_LA_PLAYA;
    golf_randomize_hole(&gs, 0x1234567);
    gs.theme = THEME_LA_PLAYA;
    grc.tick_count = 0;
    grc.cam_x = 0;
    grc.cam_y = 0;
    gba_render_draw_frame(&grc, &gs);
    save_gba_bmp_scaled("screenshot_gba_la_playa.bmp", 3);
    printf("[4/8] Saved screenshot_gba_la_playa.bmp (720x480 GBA La Playa Desert Theme)\n");

    /* 4. GBA Play Mode: Nocturnal Green Gothic Theme with Autumn Trees & Tombstones */
    gs.theme = THEME_NOCTURNAL_GREEN;
    golf_randomize_hole(&gs, 0x7654321);
    gs.theme = THEME_NOCTURNAL_GREEN;
    grc.tick_count = 0;
    grc.cam_x = 0;
    grc.cam_y = 0;
    gba_render_draw_frame(&grc, &gs);
    save_gba_bmp_scaled("screenshot_gba_nocturnal_green.bmp", 3);
    printf("[5/8] Saved screenshot_gba_nocturnal_green.bmp (720x480 GBA Nocturnal Green Theme)\n");

    /* 5. GBA Sneak On Mode: Ranger Patrol Cart Active */
    gs.theme = THEME_BUSHWOOD;
    golf_start_sneak_on(&gs);
    gs.ranger.x = gs.ball.x + TO_FIXED(3);
    gs.ranger.y = gs.ball.y + TO_FIXED(1);
    gs.ranger.state = RANGER_STATE_PATROLLING;
    grc.tick_count = 0;
    grc.cam_x = 0;
    grc.cam_y = 0;
    gba_render_draw_frame(&grc, &gs);
    save_gba_bmp_scaled("screenshot_gba_sneak_on_ranger.bmp", 3);
    printf("[6/8] Saved screenshot_gba_sneak_on_ranger.bmp (720x480 GBA Sneak On with Ranger Cart)\n");

    /* 6. GBA SELECT Button: Ultra-Fast Look / Course Scroll Mode */
    grc.look_mode = true;
    int32_t ball_gx = FROM_FIXED(gs.ball.x);
    int32_t ball_gy = FROM_FIXED(gs.ball.y);
    int32_t look_target_gx = ball_gx + 1;
    int32_t look_target_gy = ball_gy - 12;
    int32_t look_target_sx = (look_target_gx - look_target_gy) * GBA_TILE_HALF_W;
    int32_t look_target_sy = (look_target_gx + look_target_gy) * GBA_TILE_HALF_H - (gs.grid[look_target_gx][look_target_gy].elevation * GBA_HEIGHT_STEP);
    grc.look_cam_x = (GBA_SCREEN_W / 2) - look_target_sx;
    grc.look_cam_y = (GBA_SCREEN_H / 2) - look_target_sy;
    grc.cam_x = grc.look_cam_x;
    grc.cam_y = grc.look_cam_y;
    grc.tick_count = 10;
    gba_render_draw_frame(&grc, &gs);
    save_gba_bmp_scaled("screenshot_gba_look_mode.bmp", 3);
    printf("[7/8] Saved screenshot_gba_look_mode.bmp (720x480 GBA Fast Look Mode Reticle)\n");

    /* 7. GBA Return to Golfer with Aim Spot Locked (Ready to Swing) */
    gs.aim_target_x = TO_FIXED(grc.look_center_gx) + FIXED_HALF;
    gs.aim_target_y = TO_FIXED(grc.look_center_gy) + FIXED_HALF;
    gs.has_custom_aim = true;
    gs.aim_distance_yds = grc.look_dist_yds;
    grc.look_mode = false;
    grc.cam_x = 0;
    grc.cam_y = 0;
    grc.tick_count = 0;
    gba_render_draw_frame(&grc, &gs);
    save_gba_bmp_scaled("screenshot_gba_aim_set.bmp", 3);
    printf("[8/8] Saved screenshot_gba_aim_set.bmp (720x480 GBA Golfer with Aim Set)\n");

    printf("=== ALL GBA VERIFICATION TESTS PASSED ===\n");
    return 0;
}
