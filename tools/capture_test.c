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
    
    /* 1. Play Mode (Tee view) */
    golf_set_mode(&gs, MODE_PLAY);
    render_center_camera_on(&rc, gs.tee_gx, gs.tee_gy, gs.grid[gs.tee_gx][gs.tee_gy].elevation);
    render_pan_camera(&rc, 0, 70);
    render_frame(&rc, &gs);
    render_save_bmp(&rc, "screenshot_play.bmp");
    printf("Saved screenshot_play.bmp\n");
    
    /* 2. Build Mode */
    golf_set_mode(&gs, MODE_BUILD);
    rc.mouse_gx = gs.tee_gx + 2;
    rc.mouse_gy = gs.tee_gy - 4;
    rc.mouse_valid = true;
    render_frame(&rc, &gs);
    render_save_bmp(&rc, "screenshot_build.bmp");
    printf("Saved screenshot_build.bmp\n");
    
    /* 3. Green & Pin View */
    golf_set_mode(&gs, MODE_PLAY);
    render_center_camera_on(&rc, gs.pin_gx, gs.pin_gy, gs.grid[gs.pin_gx][gs.pin_gy].elevation);
    render_pan_camera(&rc, 0, 50);
    render_frame(&rc, &gs);
    render_save_bmp(&rc, "screenshot_green.bmp");
    printf("Saved screenshot_green.bmp\n");
    
    render_free(&rc);
    return 0;
}
