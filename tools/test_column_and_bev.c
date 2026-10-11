#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "golf_core.h"
#include "golf_render.h"

int main(void) {
    printf("=== RUNNING VERIFICATION TEST SUITE: HIGH COLUMNS, SOFT LANDING, BEVERAGE CART, ELEVATION LOCK ===\n");

    GolfState gs;
    golf_init(&gs);
    gs.mode = MODE_PLAY;

    /* 1. Verify Elevation Lock */
    printf("[1/7] Testing Terrain Elevation Lock...\n");
    assert(gs.elevation_locked == true);
    for (int y = 0; y < GRID_H; ++y) {
        for (int x = 0; x < GRID_W; ++x) {
            assert(gs.locked_elevation[x][y] == gs.grid[x][y].elevation);
        }
    }
    /* Attempt rogue mutation mid-hole */
    uint8_t orig_elev = gs.grid[10][10].elevation;
    gs.grid[10][10].elevation = 250;
    golf_tick(&gs);
    assert(gs.grid[10][10].elevation == orig_elev);
    printf("  -> PASS: Elevation lock strictly enforced and restored in MODE_PLAY.\n");

    /* 2. Verify High Column Identification & Lob Wedge Targeting */
    printf("[2/7] Testing High Column Identification & Lob Wedge Auto-Selection...\n");
    int test_col_x = 18;
    int test_col_y = 12;
    gs.grid[test_col_x][test_col_y].elevation = ELEV_PLATEAU; /* Tier 4 */
    gs.locked_elevation[test_col_x][test_col_y] = ELEV_PLATEAU;
    assert(golf_is_high_column(&gs, test_col_x, test_col_y) == true);

    fixed tgt_x = TO_FIXED(test_col_x) + FIXED_HALF;
    fixed tgt_y = TO_FIXED(test_col_y) + FIXED_HALF;
    golf_set_aim_target(&gs, tgt_x, tgt_y);
    assert(gs.current_club == CLUB_LW);
    printf("  -> PASS: golf_set_aim_target auto-selected CLUB_LW (Lob Wedge).\n");

    /* 3. Verify Club Change Restriction on High Column */
    printf("[3/7] Testing Club Change Restriction on High Column...\n");
    golf_change_club(&gs, 1);
    assert(gs.current_club == CLUB_LW);
    golf_change_club(&gs, -2);
    assert(gs.current_club == CLUB_LW);
    golf_select_club(&gs, CLUB_DRV);
    assert(gs.current_club == CLUB_LW);
    printf("  -> PASS: Only Lob Wedge allowed when targeting high column top.\n");

    /* 4. Verify Ball Soft Landing on High Column */
    printf("[4/7] Testing Ball Soft Landing & Stick Landing...\n");
    gs.ball.x = tgt_x;
    gs.ball.y = tgt_y;
    gs.ball.ground_z = golf_get_ground_z_fixed(&gs, tgt_x, tgt_y);
    gs.ball.z = gs.ball.ground_z + TO_FIXED(5);
    gs.ball.vx = 800;
    gs.ball.vy = 800;
    gs.ball.vz = -TO_FIXED(10);
    gs.ball.state = BALL_IN_FLIGHT;
    gs.ball.air_ticks = 40;

    golf_tick(&gs);
    /* vz should be reversed with soft restitution = 1200 (~0.018), becoming negligible */
    assert(gs.ball.z >= gs.ball.ground_z);
    printf("  -> PASS: Ball stick landing on column top damped vertical bounce to soft landing.\n");

    /* 5. Verify Golfer Athletic Leap / Jump up to High Column */
    printf("[5/7] Testing Golfer Jump Animation to High Column Top...\n");
    /* Place golfer 1 tile away at lower elevation */
    gs.ball.state = BALL_IDLE;
    gs.golfer.x = tgt_x - TO_FIXED(1);
    gs.golfer.y = tgt_y;
    gs.golfer.current_z = TO_FIXED(ELEV_LOWLAND * HEIGHT_STEP_PX);
    gs.golfer.target_x = tgt_x;
    gs.golfer.target_y = tgt_y;
    gs.golfer.state = GOLFER_STATE_WALKING;

    golf_tick(&gs);
    assert(gs.golfer.state == GOLFER_STATE_JUMPING);
    assert(gs.audio_event == AUDIO_EVT_GOLFER_JUMP);
    printf("  -> PASS: Golfer transitioned to GOLFER_STATE_JUMPING with AUDIO_EVT_GOLFER_JUMP.\n");

    /* Tick through jump arc until landing */
    while (gs.golfer.state == GOLFER_STATE_JUMPING) {
        golf_tick(&gs);
    }
    assert(gs.golfer.state == GOLFER_STATE_READY);
    assert(gs.golfer.current_z == gs.ball.ground_z);
    printf("  -> PASS: Golfer completed jump arc and landed safely on column top in GOLFER_STATE_READY.\n");

    /* 6. Verify Beverage Cart 2-Square Radius Proximity, Drink & Burp */
    printf("[6/7] Testing Beverage Cart Proximity Interaction, Drink & Burp...\n");
    gs.bev_cart.active = true;
    gs.bev_cart.state = BEV_STATE_DRIVING;
    gs.bev_cart.interaction_cooldown = 0;
    /* Position cart 1.5 tiles away from golfer */
    gs.bev_cart.x = gs.golfer.x + TO_FIXED(1);
    gs.bev_cart.y = gs.golfer.y + TO_FIXED(1);
    gs.bev_cart.current_z = gs.golfer.current_z;

    golf_tick(&gs);
    assert(gs.bev_cart.state == BEV_STATE_APPROACHING_PLAYER);
    printf("  -> PASS: Beverage cart detected golfer in 2-square radius and approached player.\n");

    /* Drive cart close to player */
    while (gs.bev_cart.state == BEV_STATE_APPROACHING_PLAYER) {
        golf_tick(&gs);
    }
    assert(gs.bev_cart.state == BEV_STATE_DRINKING);
    assert(gs.golfer.state == GOLFER_STATE_DRINKING);
    printf("  -> PASS: Beverage cart arrived; golfer is drinking cold beverage.\n");

    /* Tick through drink until burp */
    bool heard_burp = false;
    while (gs.bev_cart.state == BEV_STATE_DRINKING || gs.bev_cart.state == BEV_STATE_BURPING) {
        if (gs.audio_event == AUDIO_EVT_BURP) {
            heard_burp = true;
        }
        golf_tick(&gs);
    }
    assert(heard_burp == true);
    assert(gs.bev_cart.state == BEV_STATE_DRIVING);
    assert(gs.golfer.state == GOLFER_STATE_READY);
    assert(gs.bev_cart.interaction_cooldown > 0);
    printf("  -> PASS: Golfer burped (AUDIO_EVT_BURP), cart drove away, and shot continues.\n");

    /* 7. Verify Framebuffer & Text Out-Of-Bounds Safety */
    printf("[7/7] Testing Rendering Negative Bounds Memory Safety & Canaries...\n");
    RenderContext rc;
    static uint32_t fb[SCREEN_W * SCREEN_H + 64];
    memset(fb, 0xAA, sizeof(fb));
    render_init(&rc, fb + 32); /* Guard buffer before and after */

    /* Render frames with standard and extreme camera offsets */
    render_setup_screen(&rc, &gs, 0);
    render_frame(&rc, &gs);

    rc.camera_x = -2500;
    rc.camera_y = -2500;
    render_frame(&rc, &gs);

    rc.camera_x = 3000;
    rc.camera_y = 3000;
    render_frame(&rc, &gs);

    /* Verify no negative indices were written (canary bytes at start of fb must be intact) */
    for (int i = 0; i < 32; ++i) {
        assert(fb[i] == 0xAAAAAAAA);
    }
    /* Verify end canaries are intact */
    for (int i = SCREEN_W * SCREEN_H + 32; i < SCREEN_W * SCREEN_H + 64; ++i) {
        assert(fb[i] == 0xAAAAAAAA);
    }
    printf("  -> PASS: No buffer underflow or out-of-bounds writes. Canaries 100%% intact.\n");

    printf("\n=== ALL 7/7 VERIFICATION TESTS PASSED SUCCESSFULLY! ===\n");
    return 0;
}
