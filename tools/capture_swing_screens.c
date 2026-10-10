#include <stdio.h>
#include <stdlib.h>
#include "golf_core.h"
#include "golf_render.h"

int main(void) {
    static GolfState gs;
    static RenderContext rc;
    static uint32_t fb[SCREEN_W * SCREEN_H];
    
    printf("Capturing updated swing and gameplay screenshots...\n");
    golf_init(&gs);
    render_init(&rc, fb);
    
    /* 1. Swing State READY: Waiting to swing */
    gs.mode = MODE_PLAY;
    gs.theme = THEME_BUSHWOOD;
    render_center_camera_on(&rc, gs.tee_gx, gs.tee_gy, gs.grid[gs.tee_gx][gs.tee_gy].elevation);
    render_pan_camera(&rc, 0, 70);
    render_frame(&rc, &gs);
    render_save_bmp(&rc, "screenshot_swing_ready.bmp");
    
    /* 2. Swing State POWER: needle rising toward 100% */
    gs.swing.state = SWING_STATE_POWER;
    gs.swing.cursor_pos = FIXED_ONE * 70 / 100;
    render_frame(&rc, &gs);
    render_save_bmp(&rc, "screenshot_swing_power.bmp");
    
    /* 3. Swing State ACCURACY: power locked at 100% (bounced), returning to sweetspot */
    gs.swing.state = SWING_STATE_ACCURACY;
    gs.swing.locked_power = FIXED_ONE;
    gs.swing.cursor_pos = gs.swing.sweetspot_center + TO_FIXED(2) / 100;
    render_frame(&rc, &gs);
    render_save_bmp(&rc, "screenshot_swing_accuracy.bmp");
    
    /* 4. Golfer Walking to Ball */
    gs.swing.state = SWING_STATE_READY;
    gs.golfer.state = GOLFER_STATE_WALKING;
    render_frame(&rc, &gs);
    render_save_bmp(&rc, "screenshot_golfer_walking.bmp");
    
    printf("Screenshots captured successfully!\n");
    return 0;
}

