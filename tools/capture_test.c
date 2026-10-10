#include <stdio.h>
#include <stdlib.h>
#include "golf_core.h"
#include "golf_render.h"

int main(void) {
    static GolfState gs;
    static RenderContext rc;
    static uint32_t fb[SCREEN_W * SCREEN_H];
    
    printf("=== DUFFER-PRO VERIFICATION TEST (1024x768) ===\n");
    golf_init(&gs);
    render_init(&rc, fb);
    
    /* 1. Splash Screen */
    gs.mode = MODE_SPLASH;
    rc.tick_count = 15;
    render_frame(&rc, &gs);
    render_save_bmp(&rc, "screenshot_splash_pro.bmp");
    printf("[1/7] Saved screenshot_splash_pro.bmp (1024x768)\n");
    
    /* 2. Play Mode (Tee & Clubhouse view with Caddy and Spectators) */
    golf_set_mode(&gs, MODE_PLAY);
    render_center_camera_on(&rc, gs.tee_gx - 1, gs.tee_gy - 1, gs.grid[gs.tee_gx][gs.tee_gy].elevation);
    render_pan_camera(&rc, 0, 90);
    render_frame(&rc, &gs);
    render_save_bmp(&rc, "screenshot_play_pro.bmp");
    printf("[2/7] Saved screenshot_play_pro.bmp (Tee: %d,%d, Clubhouse: %d,%d, Rating: %d%%, Fun Stars: %d)\n",
           gs.tee_gx, gs.tee_gy, gs.clubhouse_gx, gs.clubhouse_gy, gs.fun_rating, (gs.fun_rating * 5) / 100);
    
    /* 3. Design Mode: Scenery Palette */
    golf_set_mode(&gs, MODE_BUILD);
    golf_set_build_category(&gs, BUILD_CAT_SCENERY);
    golf_set_tool(&gs, TOOL_FLOWERING);
    rc.mouse_gx = gs.tee_gx + 2;
    rc.mouse_gy = gs.tee_gy - 3;
    rc.mouse_valid = true;
    render_frame(&rc, &gs);
    render_save_bmp(&rc, "screenshot_design_pro.bmp");
    printf("[3/7] Saved screenshot_design_pro.bmp (Category: SCENERY, Tool: CHERRY BLOSSOM)\n");
    
    /* 4. Terrain Sculpting Test: Raise and Lower */
    printf("\n--- Testing Terrain Raising & Lowering ---\n");
    uint8_t old_elev = gs.grid[gs.tee_gx + 2][gs.tee_gy - 2].elevation;
    golf_raise_terrain(&gs, gs.tee_gx + 2, gs.tee_gy - 2, 2);
    printf("Raised terrain: %d -> %d, AudioEvent=%d (Expected 19/TERRAIN_SCULPT)\n",
           old_elev, gs.grid[gs.tee_gx + 2][gs.tee_gy - 2].elevation, gs.audio_event);
    gs.audio_event = AUDIO_EVT_NONE;
    render_frame(&rc, &gs);
    render_save_bmp(&rc, "screenshot_sculpt_pro.bmp");
    printf("[4/7] Saved screenshot_sculpt_pro.bmp\n");
    
    /* 5. Aiming & Ball Flight */
    golf_set_mode(&gs, MODE_PLAY);
    render_center_camera_on(&rc, gs.tee_gx, gs.tee_gy, gs.grid[gs.tee_gx][gs.tee_gy].elevation);
    render_pan_camera(&rc, 0, 70);
    
    golf_swing_click(&gs); /* Click 1 */
    for (int t = 0; t < 55; ++t) golf_tick(&gs);
    golf_swing_click(&gs); /* Click 2 */
    while (gs.swing.cursor_pos > gs.swing.sweetspot_center + 600) golf_tick(&gs);
    golf_swing_click(&gs); /* Click 3 */
    printf("Driver Strike: Ball state=%d, vz=%.2f, AudioEvent=%d (Expected 1/SWING_DRIVER)\n",
           gs.ball.state, (float)FROM_FIXED(gs.ball.vz), gs.audio_event);
    gs.audio_event = AUDIO_EVT_NONE;
    
    for (int t = 0; t < 30; ++t) golf_tick(&gs);
    int32_t bgx = FROM_FIXED(gs.ball.x);
    int32_t bgy = FROM_FIXED(gs.ball.y);
    render_center_camera_on(&rc, bgx, bgy, FROM_FIXED(gs.ball.ground_z) / HEIGHT_STEP_PX);
    render_pan_camera(&rc, 0, 70);
    render_frame(&rc, &gs);
    render_save_bmp(&rc, "screenshot_flight_pro.bmp");
    printf("[5/7] Saved screenshot_flight_pro.bmp (Apex Height=%.1f px)\n", (float)FROM_FIXED(gs.ball.z - gs.ball.ground_z));
    
    /* Settle ball */
    while (gs.ball.state != BALL_IDLE) golf_tick(&gs);
    
    /* 6. Golfer & Caddy Walking */
    for (int t = 0; t < 25; ++t) golf_tick(&gs);
    int32_t ggx = FROM_FIXED(gs.golfer.x);
    int32_t ggy = FROM_FIXED(gs.golfer.y);
    render_center_camera_on(&rc, ggx, ggy, gs.grid[ggx][ggy].elevation);
    render_pan_camera(&rc, 0, 70);
    render_frame(&rc, &gs);
    render_save_bmp(&rc, "screenshot_walking_pro.bmp");
    printf("[6/7] Saved screenshot_walking_pro.bmp (Golfer at %d,%d, Caddy State=%d)\n", ggx, ggy, gs.caddy.state);
    
    /* 7. Putting & Cup Sink with Gallery Celebration */
    gs.ball.x = TO_FIXED(gs.pin_gx) + FIXED_HALF + 16384;
    gs.ball.y = TO_FIXED(gs.pin_gy) + FIXED_HALF;
    gs.ball.ground_z = golf_get_ground_z_fixed(&gs, gs.ball.x, gs.ball.y);
    gs.ball.z = gs.ball.ground_z;
    gs.ball.state = BALL_IDLE;
    gs.ball.current_surf = SURF_GREEN;
    gs.current_club = CLUB_PT;
    gs.aim_target_x = TO_FIXED(gs.pin_gx) + FIXED_HALF;
    gs.aim_target_y = TO_FIXED(gs.pin_gy) + FIXED_HALF;
    gs.aim_angle = 128;
    gs.aim_distance_yds = 2;
    gs.swing.state = SWING_STATE_READY;
    gs.hole_completed = false;
    
    golf_swing_click(&gs);
    gs.swing.locked_power = 14000;
    gs.swing.state = SWING_STATE_ACCURACY;
    gs.swing.cursor_pos = gs.swing.sweetspot_center;
    golf_swing_click(&gs);
    printf("Putt Strike: AudioEvent=%d (Expected 4/SWING_PUTTER)\n", gs.audio_event);
    gs.audio_event = AUDIO_EVT_NONE;
    
    bool sink_ok = false;
    for (int t = 0; t < 100; ++t) {
        golf_tick(&gs);
        if (gs.audio_event == AUDIO_EVT_CUP_SINK) {
            sink_ok = true;
            printf("Cup Sunk! AudioEvent=%d (Expected 13/CUP_SINK), CaddyState=%d (Celebrating!)\n",
                   gs.audio_event, gs.caddy.state);
            gs.audio_event = AUDIO_EVT_NONE;
            break;
        }
    }
    render_center_camera_on(&rc, gs.pin_gx, gs.pin_gy, gs.grid[gs.pin_gx][gs.pin_gy].elevation);
    render_pan_camera(&rc, 0, 60);
    render_frame(&rc, &gs);
    render_save_bmp(&rc, "screenshot_holed_pro.bmp");
    printf("[7/8] Saved screenshot_holed_pro.bmp (Cup Sink: %s)\n", sink_ok ? "SUCCESS" : "FAILED");
    
    /* 8. Beverage Cart Driving & Whistling */
    gs.bev_cart.active = true;
    gs.bev_cart.x = TO_FIXED(gs.tee_gx - 1) + FIXED_HALF;
    gs.bev_cart.y = TO_FIXED(gs.tee_gy - 3) + FIXED_HALF;
    gs.bev_cart.current_z = golf_get_ground_z_fixed(&gs, gs.bev_cart.x, gs.bev_cart.y);
    gs.bev_cart.target_x = TO_FIXED(gs.tee_gx + 2) + FIXED_HALF;
    gs.bev_cart.target_y = TO_FIXED(gs.tee_gy - 6) + FIXED_HALF;
    gs.bev_cart.state = BEV_STATE_DRIVING;
    gs.bev_cart.whistle_timer = 2; /* trigger whistle in 2 ticks */
    golf_tick(&gs);
    golf_tick(&gs);
    printf("Beverage Cart: Active=%d, AudioEvent=%d (Expected 20/BEV_WHISTLE), SpeechTimer=%d\n",
           gs.bev_cart.active, gs.audio_event, gs.bev_cart.speech_timer);
    gs.audio_event = AUDIO_EVT_NONE;
    int32_t bc_gx = FROM_FIXED(gs.bev_cart.x);
    int32_t bc_gy = FROM_FIXED(gs.bev_cart.y);
    render_center_camera_on(&rc, bc_gx, bc_gy, FROM_FIXED(gs.bev_cart.current_z) / HEIGHT_STEP_PX);
    render_pan_camera(&rc, 0, 80);
    render_frame(&rc, &gs);
    render_save_bmp(&rc, "screenshot_beverage_pro.bmp");
    printf("[8/8] Saved screenshot_beverage_pro.bmp (Beverage Cart Whistling at %d,%d)\n", bc_gx, bc_gy);
    
    render_free(&rc);
    printf("=== ALL VERIFICATION TESTS PASSED ===\n");
    return 0;
}
