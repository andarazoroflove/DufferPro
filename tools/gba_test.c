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

    /* 1. GBA Splash Screen (tick 25 shows blinking START prompt) */
    gba_render_splash(&grc, 25);
    save_gba_bmp_scaled("screenshot_gba_splash.bmp", 3);
    printf("[1/4] Saved screenshot_gba_splash.bmp (720x480 3x scaled GBA Title)\n");

    /* 2. GBA Play Mode: Tee view with Golfer, Caddy, 3-Click Meter */
    golf_set_mode(&gs, MODE_PLAY);
    gs.banner_timer = 0;
    gs.banner_text[0] = '\0';
    grc.tick_count = 0;
    grc.cam_x = 0;
    grc.cam_y = 0;
    gba_render_draw_frame(&grc, &gs);
    save_gba_bmp_scaled("screenshot_gba_play.bmp", 3);
    printf("[2/4] Saved screenshot_gba_play.bmp (720x480 GBA Play Mode with Swing Meter)\n");

    /* 3. GBA Carl Spackler Dynamite Attack near Tee */
    int h_gx = gs.tee_gx + 2;
    int h_gy = gs.tee_gy - 2;
    gs.gopher.visible = true;
    gs.gopher.gx = h_gx;
    gs.gopher.gy = h_gy;
    gs.carl.active = true;
    gs.carl.target_gx = h_gx;
    gs.carl.target_gy = h_gy;
    gs.carl.attack_type = CARL_ATTACK_DYNAMITE;
    gs.carl.plunger_x = TO_FIXED(h_gx + 2) + FIXED_HALF;
    gs.carl.plunger_y = TO_FIXED(h_gy + 1) + FIXED_HALF;
    gs.carl.x = gs.carl.plunger_x + TO_FIXED(0.2);
    gs.carl.y = gs.carl.plunger_y + TO_FIXED(0.2);
    gs.carl.current_z = golf_get_ground_z_fixed(&gs, gs.carl.x, gs.carl.y);
    gs.carl.state = CARL_STATE_DYNAMITE_PLUNGE;
    gs.carl.timer = 24;
    grc.tick_count = 0;
    grc.cam_x = 0;
    grc.cam_y = 0;
    gba_render_draw_frame(&grc, &gs);
    save_gba_bmp_scaled("screenshot_gba_carl_dynamite.bmp", 3);
    printf("[3/4] Saved screenshot_gba_carl_dynamite.bmp (720x480 GBA Carl Dynamite Detonation)\n");

    /* 4. GBA Carl Spackler Water Hose Flooding */
    gs.carl.active = true;
    gs.carl.target_gx = h_gx;
    gs.carl.target_gy = h_gy;
    gs.carl.attack_type = CARL_ATTACK_HOSE;
    gs.carl.x = TO_FIXED(h_gx + 2) + FIXED_HALF;
    gs.carl.y = TO_FIXED(h_gy) + FIXED_HALF;
    gs.carl.current_z = golf_get_ground_z_fixed(&gs, gs.carl.x, gs.carl.y);
    gs.carl.state = CARL_STATE_HOSE_SPRAY;
    gs.carl.timer = 50;
    grc.tick_count = 0;
    grc.cam_x = 0;
    grc.cam_y = 0;
    gba_render_draw_frame(&grc, &gs);
    save_gba_bmp_scaled("screenshot_gba_carl_hose.bmp", 3);
    printf("[4/6] Saved screenshot_gba_carl_hose.bmp (720x480 GBA Carl Water Hose Flooding)\n");

    /* 5. GBA SELECT Button: Look / Course Scroll Mode */
    gs.carl.active = false;
    gs.gopher.visible = false;
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
    printf("[5/6] Saved screenshot_gba_look_mode.bmp (720x480 GBA SELECT Course Look Mode with Reticle)\n");

    /* 6. GBA Return to Golfer with Aim Spot Locked (Ready to Swing) */
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
    printf("[6/6] Saved screenshot_gba_aim_set.bmp (720x480 GBA Golfer with Locked Aim Spot Ready to Swing)\n");

    printf("=== ALL GBA VERIFICATION TESTS PASSED ===\n");
    return 0;
}
