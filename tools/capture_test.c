#include <stdio.h>
#include <stdlib.h>
#include "golf_core.h"
#include "golf_render.h"

int main(void) {
    static GolfState gs;
    static RenderContext rc;
    static uint32_t fb[SCREEN_W * SCREEN_H];
    
    golf_init(&gs);
    render_init(&rc, fb);
    
    /* 1. Splash Screen */
    gs.mode = MODE_SPLASH;
    rc.tick_count = 15; /* Active pulsing prompt */
    render_frame(&rc, &gs);
    render_save_bmp(&rc, "screenshot_splash.bmp");
    printf("Saved screenshot_splash.bmp\n");
    
    /* 2. Play Mode (Tee view) */
    golf_set_mode(&gs, MODE_PLAY);
    render_center_camera_on(&rc, gs.tee_gx, gs.tee_gy, gs.grid[gs.tee_gx][gs.tee_gy].elevation);
    render_pan_camera(&rc, 0, 70);
    render_frame(&rc, &gs);
    render_save_bmp(&rc, "screenshot_play.bmp");
    printf("Saved screenshot_play.bmp\n");
    
    /* 3. Build Mode */
    golf_set_mode(&gs, MODE_BUILD);
    rc.mouse_gx = gs.tee_gx + 2;
    rc.mouse_gy = gs.tee_gy - 4;
    rc.mouse_valid = true;
    render_frame(&rc, &gs);
    render_save_bmp(&rc, "screenshot_build.bmp");
    printf("Saved screenshot_build.bmp\n");
    
    /* 4. Green & Pin View */
    golf_set_mode(&gs, MODE_PLAY);
    render_center_camera_on(&rc, gs.pin_gx, gs.pin_gy, gs.grid[gs.pin_gx][gs.pin_gy].elevation);
    render_pan_camera(&rc, 0, 50);
    render_frame(&rc, &gs);
    render_save_bmp(&rc, "screenshot_green.bmp");
    printf("Saved screenshot_green.bmp\n");
    
    /* 5. Randomized Hole Test */
    golf_randomize_hole(&gs, 0x1337BEEF);
    golf_set_mode(&gs, MODE_PLAY);
    render_center_camera_on(&rc, gs.tee_gx, gs.tee_gy, gs.grid[gs.tee_gx][gs.tee_gy].elevation);
    render_pan_camera(&rc, 0, 70);
    rc.mouse_sx = 490; /* Hover over RANDOM HOLE button to test hover state */
    rc.mouse_sy = 12;
    render_frame(&rc, &gs);
    render_save_bmp(&rc, "screenshot_random_hole.bmp");
    printf("Saved screenshot_random_hole.bmp (Tee: %d,%d, Pin: %d,%d, Yds: %d, Par: %d)\n",
           gs.tee_gx, gs.tee_gy, gs.pin_gx, gs.pin_gy, gs.hole_yards, gs.par);
    
    render_free(&rc);
    return 0;
}
