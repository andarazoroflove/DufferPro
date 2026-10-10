#include <stdio.h>
#include <stdlib.h>
#include "golf_core.h"
#include "golf_render.h"

int main(void) {
    static GolfState gs;
    static RenderContext rc;
    static uint32_t fb[SCREEN_W * SCREEN_H];
    
    printf("=== TESTING NEW FEATURES (BUSHWOOD, LA PLAYA, NOCTURNAL GREEN, SETUP, SNEAK ON) ===\n");
    golf_init(&gs);
    render_init(&rc, fb);
    
    /* 1. Splash Screen with Bushwood Sign, Caddyshack & Kids, Dual Mode Buttons */
    gs.mode = MODE_SPLASH;
    rc.tick_count = 42;
    render_frame(&rc, &gs);
    render_save_bmp(&rc, "screenshot_splash_v3.bmp");
    printf("[1/6] Saved screenshot_splash_v3.bmp\n");
    
    /* 2. Setup Screen with 1..5 Radio Buttons & Theme Cards */
    gs.mode = MODE_SETUP;
    gs.tree_scale = 3;
    gs.bush_scale = 3;
    gs.rock_scale = 3;
    gs.water_scale = 3;
    gs.theme = THEME_LA_PLAYA;
    rc.tick_count = 50;
    render_frame(&rc, &gs);
    render_save_bmp(&rc, "screenshot_setup_screen.bmp");
    printf("[2/6] Saved screenshot_setup_screen.bmp\n");
    
    /* 3. Bushwood Theme Gameplay with [REPLAY HOLE] button in top bar */
    golf_init(&gs);
    gs.theme = THEME_BUSHWOOD;
    golf_set_mode(&gs, MODE_PLAY);
    render_center_camera_on(&rc, gs.tee_gx, gs.tee_gy, gs.grid[gs.tee_gx][gs.tee_gy].elevation);
    render_pan_camera(&rc, 0, 70);
    render_frame(&rc, &gs);
    render_save_bmp(&rc, "screenshot_bushwood_replay.bmp");
    printf("[3/6] Saved screenshot_bushwood_replay.bmp\n");
    
    /* 4. La Playa Desert Theme Gameplay */
    golf_init(&gs);
    golf_set_theme(&gs, THEME_LA_PLAYA);
    golf_generate_terrain_scaled(&gs, 12345, 4, 4, 3, 3);
    golf_place_default_routing(&gs);
    golf_set_mode(&gs, MODE_PLAY);
    /* Activate Tumbleweeds, Raver, Police Cart */
    gs.tumbleweeds[0].active = true;
    gs.tumbleweeds[0].x = TO_FIXED(gs.tee_gx + 2);
    gs.tumbleweeds[0].y = TO_FIXED(gs.tee_gy - 2);
    gs.tumbleweeds[0].current_z = golf_get_ground_z_fixed(&gs, gs.tumbleweeds[0].x, gs.tumbleweeds[0].y);
    gs.tumbleweeds[0].rotation = 45;
    
    gs.gopher.visible = true;
    gs.gopher.state = GOPHER_STATE_TAUNTING;
    gs.gopher.gx = gs.tee_gx + 3;
    gs.gopher.gy = gs.tee_gy - 4;
    gs.gopher.x = TO_FIXED(gs.gopher.gx) + FIXED_HALF;
    gs.gopher.y = TO_FIXED(gs.gopher.gy) + FIXED_HALF;
    gs.gopher.current_z = golf_get_ground_z_fixed(&gs, gs.gopher.x, gs.gopher.y);
    
    gs.carl.active = true;
    gs.carl.state = CARL_STATE_APPROACHING;
    gs.carl.x = TO_FIXED(gs.tee_gx + 5);
    gs.carl.y = TO_FIXED(gs.tee_gy - 2);
    gs.carl.current_z = golf_get_ground_z_fixed(&gs, gs.carl.x, gs.carl.y);
    
    render_center_camera_on(&rc, gs.tee_gx + 2, gs.tee_gy - 2, gs.grid[gs.tee_gx][gs.tee_gy].elevation);
    render_pan_camera(&rc, 0, 70);
    rc.tick_count = 60;
    render_frame(&rc, &gs);
    render_save_bmp(&rc, "screenshot_la_playa.bmp");
    printf("[4/6] Saved screenshot_la_playa.bmp\n");
    
    /* 5. Nocturnal Green Transylvania Theme Gameplay */
    golf_init(&gs);
    golf_set_theme(&gs, THEME_NOCTURNAL_GREEN);
    golf_generate_terrain_scaled(&gs, 54321, 3, 3, 4, 4);
    golf_place_default_routing(&gs);
    golf_set_mode(&gs, MODE_PLAY);
    
    /* Sea Monster in water */
    gs.sea_monster.active = true;
    gs.sea_monster.state = 2; /* Tentacle swipe! */
    gs.sea_monster.x = TO_FIXED(gs.tee_gx + 2);
    gs.sea_monster.y = TO_FIXED(gs.tee_gy - 5);
    gs.sea_monster.current_z = golf_get_ground_z_fixed(&gs, gs.sea_monster.x, gs.sea_monster.y);
    
    /* Zombie */
    gs.gopher.visible = true;
    gs.gopher.state = GOPHER_STATE_TAUNTING;
    gs.gopher.gx = gs.tee_gx + 3;
    gs.gopher.gy = gs.tee_gy - 2;
    gs.gopher.x = TO_FIXED(gs.gopher.gx) + FIXED_HALF;
    gs.gopher.y = TO_FIXED(gs.gopher.gy) + FIXED_HALF;
    gs.gopher.current_z = golf_get_ground_z_fixed(&gs, gs.gopher.x, gs.gopher.y);
    
    /* Ash Williams */
    gs.carl.active = true;
    gs.carl.state = CARL_STATE_CELEBRATING;
    gs.carl.x = TO_FIXED(gs.tee_gx + 5);
    gs.carl.y = TO_FIXED(gs.tee_gy - 2);
    gs.carl.current_z = golf_get_ground_z_fixed(&gs, gs.carl.x, gs.carl.y);
    
    render_center_camera_on(&rc, gs.tee_gx + 2, gs.tee_gy - 2, gs.grid[gs.tee_gx][gs.tee_gy].elevation);
    render_pan_camera(&rc, 0, 70);
    rc.tick_count = 90;
    render_frame(&rc, &gs);
    render_save_bmp(&rc, "screenshot_nocturnal_green.bmp");
    printf("[5/6] Saved screenshot_nocturnal_green.bmp\n");
    
    /* 6. Sneak On Mode with Patrol Ranger */
    golf_init(&gs);
    golf_start_sneak_on(&gs);
    golf_set_mode(&gs, MODE_PLAY);
    gs.ranger.active = true;
    gs.ranger.state = RANGER_STATE_SCOLDING;
    gs.ranger.speech_timer = 150;
    snprintf(gs.ranger.speech_msg, sizeof(gs.ranger.speech_msg), "RANGER: 'HEY! NO SNEAKING ON BUSHWOOD!'");
    gs.ranger.x = TO_FIXED(gs.tee_gx + 2);
    gs.ranger.y = TO_FIXED(gs.tee_gy - 2);
    gs.ranger.current_z = golf_get_ground_z_fixed(&gs, gs.ranger.x, gs.ranger.y);
    
    render_center_camera_on(&rc, gs.tee_gx, gs.tee_gy, gs.grid[gs.tee_gx][gs.tee_gy].elevation);
    render_pan_camera(&rc, 0, 70);
    rc.tick_count = 120;
    render_frame(&rc, &gs);
    render_save_bmp(&rc, "screenshot_sneak_on_ranger.bmp");
    printf("[6/6] Saved screenshot_sneak_on_ranger.bmp\n");
    
    printf("=== ALL TEST SCREENSHOTS COMPLETED SUCCESSFULLY ===\n");
    return 0;
}

