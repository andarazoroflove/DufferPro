#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include "golf_core.h"
#include "golf_render.h"

int main(void) {
    printf("=== Duffer-Pro Aiming and Obstacle Physics Test ===\n\n");
    
    /* ------------------------------------------------------------- */
    /* TEST 1: Continuous Screen <-> World Precision & Inversion     */
    /* ------------------------------------------------------------- */
    printf("[1] Testing Continuous Screen <-> World Coordinate Inversion...\n");
    GolfState gs;
    golf_init(&gs);
    golf_generate_terrain_scaled(&gs, 42, 3, 3, 3, 3);
    gs.mode = MODE_PLAY;
    
    int cam_x = -200;
    int cam_y = 150;
    int mismatches = 0;
    
    for (int gx = 2; gx < 20; gx += 3) {
        for (int gy = 2; gy < 40; gy += 4) {
            fixed fx = TO_FIXED(gx) + FIXED_HALF;
            fixed fy = TO_FIXED(gy) + FIXED_HALF;
            fixed gz = golf_get_ground_z_fixed(&gs, fx, fy);
            
            int sx, sy;
            world_to_screen_fixed(fx, fy, gz, cam_x, cam_y, &sx, &sy);
            
            fixed rfx, rfy;
            bool ok = screen_to_world_fixed(&gs, sx, sy, cam_x, cam_y, &rfx, &rfy);
            assert(ok && "screen_to_world_fixed must find the tile");
            
            int rsx, rsy;
            world_to_screen_fixed(rfx, rfy, gz, cam_x, cam_y, &rsx, &rsy);
            
            if (abs(sx - rsx) > 1 || abs(sy - rsy) > 1) {
                printf("  Mismatch at (%d, %d): original screen (%d, %d) vs reconstructed (%d, %d)\n",
                       gx, gy, sx, sy, rsx, rsy);
                mismatches++;
            }
            
            /* Verify tile index matches */
            int32_t calc_gx = FROM_FIXED(rfx);
            int32_t calc_gy = FROM_FIXED(rfy);
            assert(calc_gx == gx && calc_gy == gy && "Must reconstruct exact tile");
        }
    }
    assert(mismatches == 0 && "Screen <-> World conversion must be pixel-accurate");
    printf("  PASS: Screen-to-world inversion is pixel-accurate across entire map!\n\n");
    
    /* ------------------------------------------------------------- */
    /* TEST 2: Aim Target Reticle Alignment                          */
    /* ------------------------------------------------------------- */
    printf("[2] Testing Aim Reticle Alignment Under Mouse Click...\n");
    RenderContext rc;
    render_center_camera_on(&rc, gs.tee_gx, gs.tee_gy, gs.grid[gs.tee_gx][gs.tee_gy].elevation);
    cam_x = rc.camera_x;
    cam_y = rc.camera_y;
    
    int click_mx = 512;
    int click_my = 384;
    fixed aim_fx, aim_fy;
    bool aim_ok = screen_to_world_fixed(&gs, click_mx, click_my, cam_x, cam_y, &aim_fx, &aim_fy);
    assert(aim_ok);
    
    golf_set_aim_target(&gs, aim_fx, aim_fy);
    assert(gs.has_custom_aim);
    assert(gs.aim_target_x == aim_fx);
    assert(gs.aim_target_y == aim_fy);
    
    /* Calculate screen position of aim reticle */
    fixed gz_aim = golf_get_ground_z_fixed(&gs, gs.aim_target_x, gs.aim_target_y);
    int reticle_sx, reticle_sy;
    world_to_screen_fixed(gs.aim_target_x, gs.aim_target_y, gz_aim, cam_x, cam_y, &reticle_sx, &reticle_sy);
    
    printf("  Mouse Click: (%d, %d) -> Reticle Screen Pos: (%d, %d)\n",
           click_mx, click_my, reticle_sx, reticle_sy);
    assert(abs(click_mx - reticle_sx) <= 1 && abs(click_my - reticle_sy) <= 1 &&
           "Reticle must appear exactly under mouse cursor!");
    printf("  PASS: Reticle appears directly under mouse click position!\n\n");
    
    /* ------------------------------------------------------------- */
    /* TEST 3: Struck Ball Heading Accuracy Towards Reticle          */
    /* ------------------------------------------------------------- */
    printf("[3] Testing Ball Velocity Vector Matches Aim Target Heading...\n");
    /* Place ball at (10, 20) and aim at (18, 12) */
    gs.ball.x = TO_FIXED(10) + FIXED_HALF;
    gs.ball.y = TO_FIXED(20) + FIXED_HALF;
    gs.ball.ground_z = golf_get_ground_z_fixed(&gs, gs.ball.x, gs.ball.y);
    gs.ball.z = gs.ball.ground_z;
    gs.ball.state = BALL_IDLE;
    gs.ball.current_surf = SURF_FAIRWAY;
    
    fixed target_x = TO_FIXED(18) + FIXED_HALF;
    fixed target_y = TO_FIXED(12) + FIXED_HALF;
    golf_set_aim_target(&gs, target_x, target_y);
    
    /* Perform swing */
    gs.swing.state = SWING_STATE_READY;
    golf_swing_click(&gs); /* Ready -> Power */
    gs.swing.cursor_pos = FIXED_ONE; /* 100% power */
    golf_swing_click(&gs); /* Power -> Accuracy */
    gs.swing.cursor_pos = gs.swing.sweetspot_center; /* 100% sweet spot */
    golf_swing_click(&gs); /* Accuracy -> Strike! */
    
    assert(gs.ball.state == BALL_IN_FLIGHT);
    
    /* Check angle of initial velocity */
    uint8_t flight_angle = fixed_atan2(gs.ball.vy, gs.ball.vx);
    int angle_diff = abs((int)flight_angle - (int)gs.aim_angle);
    if (angle_diff > 128) angle_diff = 256 - angle_diff;
    printf("  Aim Angle: %u, Ball Initial Flight Angle: %u (diff: %d index units / 256)\n",
           gs.aim_angle, flight_angle, angle_diff);
    assert(angle_diff <= 1 && "Ball flight vector must match aim direction exactly");
    printf("  PASS: Ball flight launches directly along aim vector!\n\n");
    
    /* ------------------------------------------------------------- */
    /* TEST 4: Obstacle Launch Square Pass-Through (Boulder)         */
    /* ------------------------------------------------------------- */
    printf("[4] Testing Obstacle Pass-Through when Launching from Boulder Tile...\n");
    /* Place boulder on tile (15, 25) and place ball right inside that square */
    gs.grid[15][25].feature = FEAT_ROCK;
    gs.ball.x = TO_FIXED(15) + FIXED_HALF;
    gs.ball.y = TO_FIXED(25) + FIXED_HALF;
    gs.ball.ground_z = golf_get_ground_z_fixed(&gs, gs.ball.x, gs.ball.y);
    gs.ball.z = gs.ball.ground_z;
    gs.ball.state = BALL_IDLE;
    gs.ball.current_surf = SURF_ROUGH;
    
    /* Clear target outside boulder tile */
    golf_set_aim_target(&gs, TO_FIXED(22) + FIXED_HALF, TO_FIXED(25) + FIXED_HALF);
    
    /* Swing from inside boulder */
    gs.swing.state = SWING_STATE_READY;
    golf_swing_click(&gs);
    gs.swing.cursor_pos = FIXED_ONE;
    golf_swing_click(&gs);
    gs.swing.cursor_pos = gs.swing.sweetspot_center;
    golf_swing_click(&gs);
    
    assert(gs.shot_origin_gx == 15);
    assert(gs.shot_origin_gy == 25);
    assert(gs.ball.state == BALL_IN_FLIGHT);
    
    /* Step physics for 10 ticks while inside origin square */
    bool hit_rock = false;
    for (int t = 0; t < 10; ++t) {
        golf_tick(&gs);
        if (gs.audio_event == AUDIO_EVT_ROCK_HIT) {
            hit_rock = true;
        }
    }
    assert(!hit_rock && "Ball must NOT collide with boulder in its own launch square!");
    /* Verify ball traveled out of launch square */
    for (int t = 0; t < 40; ++t) {
        golf_tick(&gs);
    }
    int32_t final_gx = FROM_FIXED(gs.ball.x);
    printf("  Ball started at (15, 25) with FEAT_ROCK, successfully exited to gx=%d\n", final_gx);
    assert(final_gx > 15 && "Ball must cleanly fly out of rock square without getting trapped!");
    printf("  PASS: Ball is not blocked when struck from inside boulder square!\n\n");
    
    /* ------------------------------------------------------------- */
    /* TEST 5: Obstacle Collision from Outside                       */
    /* ------------------------------------------------------------- */
    printf("[5] Testing Boulder Collision When Struck from Outside Tile...\n");
    /* Place boulder at (18, 25) and launch ball from clear tile (16, 25) directly at boulder */
    gs.grid[16][25].feature = FEAT_NONE;
    gs.grid[18][25].feature = FEAT_ROCK;
    gs.ball.x = TO_FIXED(16) + FIXED_HALF;
    gs.ball.y = TO_FIXED(25) + FIXED_HALF;
    gs.ball.ground_z = golf_get_ground_z_fixed(&gs, gs.ball.x, gs.ball.y);
    gs.ball.z = gs.ball.ground_z;
    gs.ball.state = BALL_IDLE;
    gs.ball.current_surf = SURF_FAIRWAY;
    gs.golfer.state = GOLFER_STATE_READY;
    
    golf_set_aim_target(&gs, TO_FIXED(20) + FIXED_HALF, TO_FIXED(25) + FIXED_HALF);
    
    gs.swing.state = SWING_STATE_READY;
    golf_swing_click(&gs);
    gs.swing.cursor_pos = FIXED_ONE;
    golf_swing_click(&gs);
    gs.swing.cursor_pos = gs.swing.sweetspot_center;
    golf_swing_click(&gs);
    
    assert(gs.shot_origin_gx == 16);
    assert(gs.shot_origin_gy == 25);
    
    hit_rock = false;
    for (int t = 0; t < 60; ++t) {
        golf_tick(&gs);
        if (gs.audio_event == AUDIO_EVT_ROCK_HIT) {
            hit_rock = true;
            break;
        }
    }
    assert(hit_rock && "Ball struck from outside MUST collide with boulder!");
    printf("  PASS: Obstacle collision successfully triggers when struck from outside square!\n\n");
    
    /* ------------------------------------------------------------- */
    /* TEST 6: Obstacle Pass-Through when Rolling (Putter)           */
    /* ------------------------------------------------------------- */
    printf("[6] Testing Putter Roll Out of Obstacle Square...\n");
    gs.grid[10][10].feature = FEAT_BUSH;
    gs.ball.x = TO_FIXED(10) + FIXED_HALF;
    gs.ball.y = TO_FIXED(10) + FIXED_HALF;
    gs.ball.ground_z = golf_get_ground_z_fixed(&gs, gs.ball.x, gs.ball.y);
    gs.ball.z = gs.ball.ground_z;
    gs.ball.state = BALL_IDLE;
    gs.ball.current_surf = SURF_GREEN;
    gs.golfer.state = GOLFER_STATE_READY;
    
    golf_set_aim_target(&gs, TO_FIXED(14) + FIXED_HALF, TO_FIXED(10) + FIXED_HALF);
    assert(gs.current_club == CLUB_PT);
    
    gs.swing.state = SWING_STATE_READY;
    golf_swing_click(&gs);
    gs.swing.cursor_pos = FIXED_ONE;
    golf_swing_click(&gs);
    gs.swing.cursor_pos = gs.swing.sweetspot_center;
    golf_swing_click(&gs);
    
    assert(gs.ball.state == BALL_ROLLING);
    assert(gs.shot_origin_gx == 10);
    assert(gs.shot_origin_gy == 10);
    
    bool hit_tree = false;
    for (int t = 0; t < 30; ++t) {
        golf_tick(&gs);
        if (gs.audio_event == AUDIO_EVT_TREE_HIT) {
            hit_tree = true;
        }
    }
    assert(!hit_tree && "Putt must NOT be stopped by bush/tree inside its own launch tile!");
    printf("  PASS: Putter roll cleanly exits bush square without getting stuck!\n\n");
    
    printf("ALL AIMING & OBSTACLE COLLISION TESTS PASSED PERFECTLY!\n");
    return 0;
}
