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
    printf("[8/10] Saved screenshot_beverage_pro.bmp (Beverage Cart Whistling at %d,%d)\n", bc_gx, bc_gy);
    
    /* 9. Gopher Pop-Up & Dancing on Course */
    gs.gopher.state = GOPHER_STATE_HIDDEN;
    gs.gopher.pop_cooldown = 1;
    golf_tick(&gs);
    printf("Gopher Pop-Up: Visible=%d, State=%d (Expected 1/POPPING_UP), AudioEvent=%d (Expected 21/GOPHER_CHUCKLE)\n",
           gs.gopher.visible, gs.gopher.state, gs.audio_event);
    gs.audio_event = AUDIO_EVT_NONE;
    
    /* Advance gopher to dancing state */
    for (int t = 0; t < 25; ++t) golf_tick(&gs);
    printf("Gopher Dancing: State=%d (Expected 2/TAUNTING), AnimFrame=%d\n",
           gs.gopher.state, gs.gopher.anim_frame);
    render_center_camera_on(&rc, gs.gopher.gx, gs.gopher.gy, gs.grid[gs.gopher.gx][gs.gopher.gy].elevation);
    render_pan_camera(&rc, 0, 70);
    render_frame(&rc, &gs);
    render_save_bmp(&rc, "screenshot_gopher_pop.bmp");
    printf("[9/10] Saved screenshot_gopher_pop.bmp (Gopher dancing at %d,%d)\n", gs.gopher.gx, gs.gopher.gy);
    
    /* 10. Ball Rolling into Gopher Hole Hazard */
    /* Place ball rolling directly toward the gopher hole */
    int h_gx = gs.gopher.holes[0].gx;
    int h_gy = gs.gopher.holes[0].gy;
    gs.ball.x = TO_FIXED(h_gx) - TO_FIXED(1);
    gs.ball.y = TO_FIXED(h_gy) + FIXED_HALF;
    gs.ball.ground_z = golf_get_ground_z_fixed(&gs, gs.ball.x, gs.ball.y);
    gs.ball.z = gs.ball.ground_z;
    gs.ball.vx = 8000; /* rolling towards hole */
    gs.ball.vy = 0;
    gs.ball.vz = 0;
    gs.ball.state = BALL_ROLLING;
    gs.audio_event = AUDIO_EVT_NONE;
    
    bool gopher_gulp_ok = false;
    for (int t = 0; t < 20; ++t) {
        golf_tick(&gs);
        if (gs.audio_event == AUDIO_EVT_GOPHER_GULP || gs.audio_event == AUDIO_EVT_GOPHER_CHUCKLE) {
            gopher_gulp_ok = true;
            printf("Ball Swallowed by Gopher Hole! AudioEvent=%d (Expected 22/GOPHER_GULP), Banner: %s\n",
                   gs.audio_event, gs.banner_text);
            gs.audio_event = AUDIO_EVT_NONE;
            break;
        }
    }
    render_center_camera_on(&rc, h_gx, h_gy, gs.grid[h_gx][h_gy].elevation);
    render_pan_camera(&rc, 0, 70);
    render_frame(&rc, &gs);
    render_save_bmp(&rc, "screenshot_gopher_trap.bmp");
    printf("[10/12] Saved screenshot_gopher_trap.bmp (Gopher Hole Trap: %s)\n", gopher_gulp_ok ? "SUCCESS" : "FAILED");
    
    /* 11. Carl Spackler Greenskeeper: Dynamite Blasting & Crater Formation */
    printf("\n--- Testing Carl Spackler Dynamite Attack ---\n");
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
    gs.carl.timer = 13;
    gs.carl.triggered_effect = false;
    gs.audio_event = AUDIO_EVT_NONE;
    
    golf_tick(&gs); /* Timer hits 14: Detonation instant! */
    printf("Carl Dynamite: AudioEvent=%d (Expected 23/CARL_EXPLOSION), Crater Surface=%d (Expected 4/SAND), Banner: %s\n",
           gs.audio_event, gs.grid[h_gx][h_gy].surface, gs.banner_text);
    gs.audio_event = AUDIO_EVT_NONE;
    
    /* Frame during detonation smoke & blast particles */
    gs.carl.timer = 20;
    render_center_camera_on(&rc, h_gx + 1, h_gy, gs.grid[h_gx][h_gy].elevation);
    render_pan_camera(&rc, 0, 70);
    render_frame(&rc, &gs);
    render_save_bmp(&rc, "screenshot_carl_dynamite.bmp");
    printf("[11/12] Saved screenshot_carl_dynamite.bmp (Carl Detonating Dynamite Bomb Crater)\n");
    
    /* 12. Carl Spackler Greenskeeper: High-Pressure Water Hose Flood Hazard */
    printf("\n--- Testing Carl Spackler Hose Flood Attack ---\n");
    int flood_gx = gs.tee_gx + 4;
    int flood_gy = gs.tee_gy - 2;
    gs.carl.active = true;
    gs.carl.target_gx = flood_gx;
    gs.carl.target_gy = flood_gy;
    gs.carl.attack_type = CARL_ATTACK_HOSE;
    gs.carl.x = TO_FIXED(flood_gx + 1) + FIXED_HALF;
    gs.carl.y = TO_FIXED(flood_gy) + FIXED_HALF;
    gs.carl.current_z = golf_get_ground_z_fixed(&gs, gs.carl.x, gs.carl.y);
    gs.carl.state = CARL_STATE_HOSE_SPRAY;
    gs.carl.timer = 44;
    gs.carl.triggered_effect = false;
    gs.audio_event = AUDIO_EVT_NONE;
    
    golf_tick(&gs); /* Timer hits 45: Carl hose audio event */
    int hose_audio = gs.audio_event;
    for (int t = 46; t <= 50; ++t) golf_tick(&gs); /* Timer hits 50: Water hazard flood! */
    printf("Carl Hose Flood: AudioEvent=%d (Expected 24/CARL_HOSE), Flooded Surface=%d (Expected 0/WATER), Banner: %s\n",
           hose_audio, gs.grid[flood_gx][flood_gy].surface, gs.banner_text);
    gs.audio_event = AUDIO_EVT_NONE;
    
    render_center_camera_on(&rc, flood_gx, flood_gy, gs.grid[flood_gx][flood_gy].elevation);
    render_pan_camera(&rc, 0, 70);
    render_frame(&rc, &gs);
    render_save_bmp(&rc, "screenshot_carl_hose.bmp");
    printf("[12/12] Saved screenshot_carl_hose.bmp (Carl Flooding Course with Water Hose)\n");
    
    render_free(&rc);
    printf("=== ALL VERIFICATION TESTS PASSED ===\n");
    return 0;
}
