#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "golf_core.h"
#include "golf_render.h"

int main(void) {
    static GolfState gs;
    printf("=== TESTING SWING MECHANICS & ENCOUNTER BALANCING ===\n\n");
    
    /* TEST 1: Standard 3-Press Swing (Press 1: Start -> Press 2: Lock Power -> Press 3: Sweetspot Strike) */
    printf("[Test 1] 3-Press Swing Sequence...\n");
    golf_init(&gs);
    golf_set_mode(&gs, MODE_PLAY);
    assert(gs.swing.state == SWING_STATE_READY);
    assert(gs.ball.state == BALL_IDLE);
    assert(gs.strokes == 0);
    
    /* Press 1: Start Swing */
    golf_swing_click(&gs);
    assert(gs.swing.state == SWING_STATE_POWER);
    assert(gs.golfer.state == GOLFER_STATE_SWINGING);
    
    /* Advance 30 ticks */
    for (int t = 0; t < 30; ++t) golf_tick(&gs);
    assert(gs.swing.state == SWING_STATE_POWER);
    assert(gs.swing.cursor_pos > 0 && gs.swing.cursor_pos < FIXED_ONE);
    
    /* Press 2: Lock Power */
    fixed locked_p = gs.swing.cursor_pos;
    golf_swing_click(&gs);
    assert(gs.swing.state == SWING_STATE_ACCURACY);
    assert(gs.swing.locked_power == locked_p);
    
    /* Advance needle down toward sweetspot */
    while (gs.swing.cursor_pos > gs.swing.sweetspot_center + TO_FIXED(0.05)) {
        golf_tick(&gs);
    }
    
    /* Press 3: Strike ball */
    golf_swing_click(&gs);
    assert(gs.swing.state == SWING_STATE_RESOLVED);
    assert(gs.strokes == 1);
    assert(gs.ball.state == BALL_IN_FLIGHT || gs.ball.state == BALL_ROLLING);
    printf("  -> PASS: 3-press swing successfully fired shot with power=%d, strokes=%d\n\n", FROM_FIXED(locked_p * 100), gs.strokes);
    
    /* TEST 2: Far Side Auto-Bounce (Press 1: Start -> Top Auto-Bounce -> Press 2: Strike) */
    printf("[Test 2] Top / Far Side Auto-Bounce (2nd Press needed in sweetspot)...\n");
    golf_init(&gs);
    golf_set_mode(&gs, MODE_PLAY);
    
    /* Press 1: Start Swing */
    golf_swing_click(&gs);
    assert(gs.swing.state == SWING_STATE_POWER);
    
    /* Allow bar to reach 100% without user pressing */
    int safety = 200;
    while (gs.swing.state == SWING_STATE_POWER && --safety > 0) {
        golf_tick(&gs);
    }
    assert(gs.swing.state == SWING_STATE_ACCURACY);
    assert(gs.swing.locked_power == FIXED_ONE);
    printf("  -> Bar reached 100%%, auto-bounced and locked 100%% power cleanly!\n");
    
    /* Needle moving back down toward sweetspot */
    while (gs.swing.cursor_pos > gs.swing.sweetspot_center) {
        golf_tick(&gs);
    }
    
    /* Press 2: Strike ball in sweetspot */
    golf_swing_click(&gs);
    assert(gs.swing.state == SWING_STATE_RESOLVED);
    assert(gs.strokes == 1);
    printf("  -> PASS: Auto-bounce requires only 1 more press in sweetspot to strike!\n\n");
    
    /* TEST 3: No Auto-Fire on Return (Zero timeout resets swing safely) */
    printf("[Test 3] Zero Timeout (Never auto-fire a shot)...\n");
    golf_init(&gs);
    golf_set_mode(&gs, MODE_PLAY);
    
    golf_swing_click(&gs); /* Press 1: start */
    /* Wait for auto-bounce */
    for (int t = 0; t < 120; ++t) golf_tick(&gs);
    assert(gs.swing.state == SWING_STATE_ACCURACY);
    
    /* Let needle travel all the way to 0 without player pressing */
    for (int t = 0; t < 120; ++t) golf_tick(&gs);
    assert(gs.swing.state == SWING_STATE_READY);
    assert(gs.strokes == 0); /* MUST NOT HAVE FIRED */
    assert(gs.ball.state == BALL_IDLE);
    printf("  -> PASS: Reaching 0 on return reset swing to READY without auto-firing (strokes=%d)\n\n", gs.strokes);
    
    /* TEST 4: Golfer Walking Arrival & Skip */
    printf("[Test 4] Walking Arrival & Click-to-Skip...\n");
    golf_init(&gs);
    golf_set_mode(&gs, MODE_PLAY);
    gs.golfer.state = GOLFER_STATE_WALKING;
    gs.golfer.x = TO_FIXED(10);
    gs.golfer.y = TO_FIXED(10);
    gs.ball.x = TO_FIXED(25);
    gs.ball.y = TO_FIXED(25);
    gs.golfer.target_x = gs.ball.x;
    gs.golfer.target_y = gs.ball.y;
    
    /* Clicking while walking skips walk to arrival */
    golf_swing_click(&gs);
    assert(gs.golfer.state == GOLFER_STATE_READY);
    assert(gs.golfer.x == gs.ball.x);
    assert(gs.golfer.y == gs.ball.y);
    assert(gs.swing.state == SWING_STATE_READY);
    printf("  -> PASS: Golfer arrived at ball immediately on click/press without popping or swinging prematurely!\n\n");
    
    /* TEST 5: Ranger Catch Balancing (1 in 20 chance & immunity cooldown) */
    printf("[Test 5] Ranger Encounter Balancing...\n");
    golf_init(&gs);
    golf_start_sneak_on(&gs);
    assert(gs.is_sneak_on == true);
    assert(gs.ranger.active == true);
    assert(gs.ranger.cooldown > 0); /* Started with grace period */
    
    /* Simulate 500 direct encounters with the ranger */
    int caught_count = 0;
    int warning_count = 0;
    for (int trial = 0; trial < 500; ++trial) {
        gs.ranger.cooldown = 0;
        gs.ranger.state = RANGER_STATE_PATROLLING;
        gs.ranger.x = gs.golfer.x;
        gs.ranger.y = gs.golfer.y;
        golf_tick(&gs);
        if (gs.ranger.cooldown >= 1800) {
            caught_count++;
        } else if (gs.ranger.cooldown >= 900) {
            warning_count++;
        }
    }
    double caught_pct = (double)caught_count / 500.0 * 100.0;
    printf("  -> Over 500 encounters: Caught=%d (%.1f%%), Warnings=%d (%.1f%%)\n",
           caught_count, caught_pct, warning_count, (double)warning_count / 500.0 * 100.0);
    assert(caught_pct >= 2.0 && caught_pct <= 9.0); /* Approx 5% (1 in 20) */
    printf("  -> PASS: Ranger catch rate perfectly tuned to ~5%% (1 in 20) with long immunity!\n\n");
    
    printf("ALL TESTS PASSED WITH 100%% SUCCESS!\n");
    return 0;
}

