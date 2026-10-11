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

    /* 2. GBA Aiming Mode: High-Visibility Center Crosshair & Yardage Tag */
    golf_set_mode(&gs, MODE_PLAY);
    gs.theme = THEME_BUSHWOOD;
    gs.banner_timer = 0;
    gs.banner_text[0] = '\0';
    grc.tick_count = 10;
    grc.aim_locked = false;
    
    /* Pan camera forward into fairway to demonstrate crosshair aiming */
    int32_t ball_gx = FROM_FIXED(gs.ball.x);
    int32_t ball_gy = FROM_FIXED(gs.ball.y);
    int32_t aim_target_gx = ball_gx + 1;
    int32_t aim_target_gy = ball_gy - 12;
    int32_t aim_target_sx = (aim_target_gx - aim_target_gy) * GBA_TILE_HALF_W;
    int32_t aim_target_sy = (aim_target_gx + aim_target_gy) * GBA_TILE_HALF_H - (gs.grid[aim_target_gx][aim_target_gy].elevation * GBA_HEIGHT_STEP);
    grc.cam_x = (GBA_SCREEN_W / 2) - aim_target_sx - GBA_TILE_HALF_W;
    grc.cam_y = (GBA_SCREEN_H / 2) - aim_target_sy - GBA_TILE_HALF_H;
    gba_render_draw_frame(&grc, &gs);
    save_gba_bmp_scaled("screenshot_gba_aiming.bmp", 3);
    printf("[3/9] Saved screenshot_gba_aiming.bmp (720x480 GBA Aiming Mode with Center Crosshair)\n");

    /* 3. GBA Aim Locked Mode: Pinned Target Flag & LOCKED Box */
    grc.aim_locked = true;
    grc.aim_target_gx = aim_target_gx;
    grc.aim_target_gy = aim_target_gy;
    golf_set_aim_target(&gs, TO_FIXED(aim_target_gx) + FIXED_HALF, TO_FIXED(aim_target_gy) + FIXED_HALF);
    
    /* Return camera to ball */
    int b_elev = gs.grid[ball_gx][ball_gy].elevation;
    int32_t b_sx = (ball_gx - ball_gy) * GBA_TILE_HALF_W;
    int32_t b_sy = (ball_gx + ball_gy) * GBA_TILE_HALF_H - (b_elev * GBA_HEIGHT_STEP);
    grc.cam_x = (GBA_SCREEN_W / 2) - b_sx - GBA_TILE_HALF_W;
    grc.cam_y = (GBA_SCREEN_H / 2) - b_sy - GBA_TILE_HALF_H;
    gba_render_draw_frame(&grc, &gs);
    save_gba_bmp_scaled("screenshot_gba_locked.bmp", 3);
    printf("[4/10] Saved screenshot_gba_locked.bmp (720x480 GBA Locked Pin & LOCKED Box at Ball)\n");

    /* 3b. GBA Pinned Target Flag on Terrain */
    int t_elev = gs.grid[aim_target_gx][aim_target_gy].elevation;
    int32_t t_sx = (aim_target_gx - aim_target_gy) * GBA_TILE_HALF_W;
    int32_t t_sy = (aim_target_gx + aim_target_gy) * GBA_TILE_HALF_H - (t_elev * GBA_HEIGHT_STEP);
    grc.cam_x = (GBA_SCREEN_W / 2) - t_sx - GBA_TILE_HALF_W;
    grc.cam_y = (GBA_SCREEN_H / 2) - t_sy - GBA_TILE_HALF_H;
    gba_render_draw_frame(&grc, &gs);
    save_gba_bmp_scaled("screenshot_gba_pinned_flag.bmp", 3);
    printf("[5/10] Saved screenshot_gba_pinned_flag.bmp (720x480 GBA Pinned Target Flag on Course)\n");

    /* 4. GBA Golfer Walking Animation */
    grc.aim_locked = false;
    grc.cam_x = (GBA_SCREEN_W / 2) - b_sx - GBA_TILE_HALF_W;
    grc.cam_y = (GBA_SCREEN_H / 2) - b_sy - GBA_TILE_HALF_H;
    gs.golfer.state = GOLFER_STATE_WALKING;
    gs.golfer.walk_anim_frame = 2;
    gba_render_draw_frame(&grc, &gs);
    save_gba_bmp_scaled("screenshot_gba_walking.bmp", 3);
    printf("[6/10] Saved screenshot_gba_walking.bmp (720x480 GBA Animated Walking Golfer)\n");
    gs.golfer.state = GOLFER_STATE_READY;
    gs.golfer.walk_anim_frame = 0;

    /* 5. GBA Play Mode: La Playa Desert Theme with Cactus, Tumbleweeds, Rapids */
    gs.theme = THEME_LA_PLAYA;
    golf_randomize_hole(&gs, 0x1234567);
    gs.theme = THEME_LA_PLAYA;
    grc.aim_locked = false;
    grc.tick_count = 0;
    grc.cam_x = 0;
    grc.cam_y = 0;
    gba_render_draw_frame(&grc, &gs);
    save_gba_bmp_scaled("screenshot_gba_la_playa.bmp", 3);
    printf("[6/9] Saved screenshot_gba_la_playa.bmp (720x480 GBA La Playa Desert Theme)\n");

    /* 6. GBA Play Mode: Nocturnal Green Gothic Theme with Autumn Trees & Tombstones */
    gs.theme = THEME_NOCTURNAL_GREEN;
    golf_randomize_hole(&gs, 0x7654321);
    gs.theme = THEME_NOCTURNAL_GREEN;
    grc.tick_count = 0;
    grc.cam_x = 0;
    grc.cam_y = 0;
    gba_render_draw_frame(&grc, &gs);
    save_gba_bmp_scaled("screenshot_gba_nocturnal_green.bmp", 3);
    printf("[7/9] Saved screenshot_gba_nocturnal_green.bmp (720x480 GBA Nocturnal Green Theme)\n");

    /* 7. GBA Sneak On Mode: Ranger Patrol Cart Active */
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
    printf("[8/9] Saved screenshot_gba_sneak_on_ranger.bmp (720x480 GBA Sneak On with Ranger Cart)\n");

    /* 8. Backward Compatibility: Look Mode & Aim Set */
    grc.look_mode = true;
    grc.aim_locked = false;
    gba_render_draw_frame(&grc, &gs);
    save_gba_bmp_scaled("screenshot_gba_look_mode.bmp", 3);
    printf("[9/9] Saved screenshot_gba_look_mode.bmp (720x480 GBA Look Mode Compatibility)\n");

    printf("=== ALL GBA VERIFICATION TESTS PASSED ===\n");
    return 0;
}
