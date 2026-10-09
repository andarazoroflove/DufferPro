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
    
    /* 5b. Click-to-Aim Test */
    printf("\n--- Testing Click-to-Aim Mechanism ---\n");
    /* Simulate player clicking the screen at (sx=450, sy=260) */
    fixed click_fx, click_fy;
    if (screen_to_world_fixed(&gs, 450, 260, rc.camera_x, rc.camera_y, &click_fx, &click_fy)) {
        golf_set_aim_target(&gs, click_fx, click_fy);
    }
    printf("Clicked Screen (450, 260) -> World (%.1f, %.1f) -> Angle=%d, Distance=%d yds, Selected Club=%s\n",
           (float)FROM_FIXED(gs.aim_target_x), (float)FROM_FIXED(gs.aim_target_y),
           gs.aim_angle, gs.aim_distance_yds, golf_get_club_data(gs.current_club)->name);
    rc.mouse_sx = 450;
    rc.mouse_sy = 260;
    render_frame(&rc, &gs);
    render_save_bmp(&rc, "screenshot_aim_target.bmp");
    printf("Saved screenshot_aim_target.bmp\n");
    
    /* 6. Swing & Ball Flight Test */
    printf("\n--- Simulating 3-Click Swing ---\n");
    printf("Initial: State=%d, Ball=(%.2f, %.2f, z=%.2f)\n",
           gs.swing.state, (float)FROM_FIXED(gs.ball.x), (float)FROM_FIXED(gs.ball.y), (float)FROM_FIXED(gs.ball.z));
    
    /* Click 1: Start swing */
    golf_swing_click(&gs);
    printf("Click 1: State=%d (Expected 1/POWER)\n", gs.swing.state);
    
    /* Advance 60 ticks (reaching ~64% power) */
    for (int t = 0; t < 60; ++t) golf_tick(&gs);
    printf("Tick 60: Power needle=%.2f\n", (float)gs.swing.cursor_pos / 65536.0f);
    
    /* Click 2: Lock power */
    golf_swing_click(&gs);
    printf("Click 2: State=%d (Expected 2/ACCURACY), LockedPower=%.2f\n",
           gs.swing.state, (float)gs.swing.locked_power / 65536.0f);
    
    /* Advance down towards sweetspot (~45 ticks) */
    while (gs.swing.cursor_pos > gs.swing.sweetspot_center + 500) {
        golf_tick(&gs);
    }
    printf("Returning: Needle=%.4f (Sweetspot=%.4f)\n",
           (float)gs.swing.cursor_pos / 65536.0f, (float)gs.swing.sweetspot_center / 65536.0f);
    
    /* Click 3: Snap sweetspot & Strike ball! */
    golf_swing_click(&gs);
    printf("Click 3: Ball state=%d (Expected 1/IN_FLIGHT), vz=%.2f px/tick\n",
           gs.ball.state, (float)FROM_FIXED(gs.ball.vz));
    
    /* Fly 35 ticks into the air (near apex) and capture flight screenshot */
    for (int t = 0; t < 35; ++t) golf_tick(&gs);
    printf("Apex (tick 35): Ball=(%.2f, %.2f), GroundZ=%.1f, BallZ=%.1f (Height=%.1f px)\n",
           (float)FROM_FIXED(gs.ball.x), (float)FROM_FIXED(gs.ball.y),
           (float)FROM_FIXED(gs.ball.ground_z), (float)FROM_FIXED(gs.ball.z),
           (float)FROM_FIXED(gs.ball.z - gs.ball.ground_z));
    
    /* Center camera dynamically on flying ball */
    {
        int32_t bgx = FROM_FIXED(gs.ball.x);
        int32_t bgy = FROM_FIXED(gs.ball.y);
        render_center_camera_on(&rc, bgx, bgy, FROM_FIXED(gs.ball.ground_z) / HEIGHT_STEP_PX);
        render_pan_camera(&rc, 0, 70);
        render_frame(&rc, &gs);
        render_save_bmp(&rc, "screenshot_swing_flight.bmp");
        printf("Saved screenshot_swing_flight.bmp\n");
    }
    
    /* Run remaining ticks until ball settles */
    int total_sim_ticks = 35;
    while (gs.ball.state != BALL_IDLE && total_sim_ticks < 500) {
        golf_tick(&gs);
        total_sim_ticks++;
    }
    printf("Settled after %d ticks: Ball state=%d, Final Ball=(%.2f, %.2f), Strokes=%d, SwingState=%d\n",
           total_sim_ticks, gs.ball.state, (float)FROM_FIXED(gs.ball.x), (float)FROM_FIXED(gs.ball.y),
           gs.strokes, gs.swing.state);
    
    render_free(&rc);
    return 0;
}
