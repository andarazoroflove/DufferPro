#include "gba_hw.h"
#include "gba_render.h"
#include "golf_core.h"

static GolfState s_gba_game_state;
static GbaRenderContext s_gba_render_ctx;

static void dispatch_gba_audio(GolfAudioEvent evt, int club) {
    switch (evt) {
        case AUDIO_EVT_SWING_DRIVER:   gba_sfx_swing_hit(0); break;
        case AUDIO_EVT_SWING_IRON:     gba_sfx_swing_hit(1); break;
        case AUDIO_EVT_SWING_WEDGE:    gba_sfx_swing_hit(2); break;
        case AUDIO_EVT_SWING_PUTTER:   gba_sfx_swing_hit(4); break;
        case AUDIO_EVT_BALL_BOUNCE:    gba_sfx_bounce(); break;
        case AUDIO_EVT_WATER_SPLASH:   gba_sfx_water_splash(); break;
        case AUDIO_EVT_CUP_SINK:       gba_sfx_cup_sink(); break;
        case AUDIO_EVT_GOPHER_CHUCKLE: gba_sfx_gopher_chuckle(); break;
        case AUDIO_EVT_GOPHER_GULP:    gba_sfx_gopher_gulp(); break;
        case AUDIO_EVT_CARL_EXPLOSION: gba_sfx_carl_explosion(); break;
        case AUDIO_EVT_CARL_HOSE:      gba_sfx_carl_hose(); break;
        case AUDIO_EVT_BEV_WHISTLE:    gba_sfx_whistle(); break;
        default: break;
    }
}

int main(void) {
    /* 1. Hardware Initialization */
    gba_hw_init();
    gba_render_init(&s_gba_render_ctx);
    s_gba_game_state.mode = MODE_SPLASH;
    int splash_menu_choice = 0; /* 0 = Jump On, 1 = Play Hole */
    
    /* Draw and display title screen immediately so screen is vibrant on boot */
    gba_render_splash(&s_gba_render_ctx, 0, splash_menu_choice);
    gba_flip();
    
    /* Pre-generate initial course hole in RAM */
    golf_init(&s_gba_game_state);
    s_gba_game_state.mode = MODE_SPLASH;
    int splash_timer = 0;
    
    /* 2. Main Game Loop */
    while (1) {
        gba_poll_keys();
        
        if (s_gba_game_state.mode == MODE_SPLASH) {
            splash_timer++;
            gba_audio_tick_splash(splash_timer);
            
            /* D-PAD: Switch Between "JUMP ON" and "PLAY HOLE" */
            if (gba_key_pressed(KEY_LEFT) || gba_key_pressed(KEY_RIGHT) ||
                gba_key_pressed(KEY_UP) || gba_key_pressed(KEY_DOWN)) {
                splash_menu_choice = 1 - splash_menu_choice;
                gba_sfx_bounce();
            }
            
            /* START or A Button: Confirm Mode Selection */
            if (gba_key_pressed(KEY_START) || gba_key_pressed(KEY_A)) {
                gba_audio_stop_music();
                if (splash_menu_choice == 0) {
                    /* JUMP ON / SNEAK ON: Random theme, random obstacles 1..5, WITH Marshall Ranger */
                    golf_start_sneak_on(&s_gba_game_state);
                    golf_set_banner(&s_gba_game_state, "JUMP ON ACTIVE! AVOID THE RANGER!", 180);
                    gba_sfx_whistle();
                } else {
                    /* PLAY HOLE: Random theme, random obstacles 1..5, WITHOUT Marshall Ranger */
                    uint32_t seed = (uint32_t)splash_timer * 1103515245 + 12345;
                    s_gba_game_state.is_sneak_on = false;
                    s_gba_game_state.tree_scale = 1 + (seed % 5);
                    s_gba_game_state.bush_scale = 1 + ((seed >> 3) % 5);
                    s_gba_game_state.rock_scale = 1 + ((seed >> 6) % 5);
                    s_gba_game_state.water_scale = 1 + ((seed >> 9) % 5);
                    s_gba_game_state.theme = (CourseTheme)((seed >> 12) % THEME_COUNT);
                    golf_randomize_hole(&s_gba_game_state, seed);
                    s_gba_game_state.ranger.active = false;
                    s_gba_game_state.ranger.state = RANGER_STATE_INACTIVE;
                    s_gba_game_state.mode = MODE_PLAY;
                    golf_set_banner(&s_gba_game_state, "PLAY HOLE ACTIVE! HIT THE GREEN!", 180);
                    gba_sfx_swing_hit(0);
                }
            }
            
            gba_render_splash(&s_gba_render_ctx, splash_timer, splash_menu_choice);
            gba_flip();
        } else {
            /* PLAY MODE */
            
            /* Key Controls */
            if (s_gba_game_state.ball.state == BALL_IDLE) {
                /* SELECT Button: Toggle Look / Fast Course Scroll Mode */
                if (gba_key_pressed(KEY_SELECT)) {
                    s_gba_render_ctx.look_mode = !s_gba_render_ctx.look_mode;
                    if (s_gba_render_ctx.look_mode) {
                        /* Start look camera at current view */
                        s_gba_render_ctx.look_cam_x = s_gba_render_ctx.cam_x;
                        s_gba_render_ctx.look_cam_y = s_gba_render_ctx.cam_y;
                        gba_sfx_bounce();
                    } else {
                        /* Returning to golfer to take shot */
                        gba_sfx_bounce();
                    }
                }
                
                if (s_gba_render_ctx.look_mode) {
                    /* ULTRA-FAST LOOK / SCROLL MODE:
                     * Base speed: 14px H / 10px V per frame (~4x faster)
                     * Turbo speed: 28px H / 20px V per frame with B held (~8x faster!) */
                    int speed_x = gba_key_held(KEY_B) ? 28 : 14;
                    int speed_y = gba_key_held(KEY_B) ? 20 : 10;
                    
                    if (gba_key_held(KEY_UP))    s_gba_render_ctx.look_cam_y += speed_y;
                    if (gba_key_held(KEY_DOWN))  s_gba_render_ctx.look_cam_y -= speed_y;
                    if (gba_key_held(KEY_LEFT))  s_gba_render_ctx.look_cam_x += speed_x;
                    if (gba_key_held(KEY_RIGHT)) s_gba_render_ctx.look_cam_x -= speed_x;
                    
                    /* Press A to SET AIM SPOT at center of window */
                    if (gba_key_pressed(KEY_A)) {
                        int32_t cgx = s_gba_render_ctx.look_center_gx;
                        int32_t cgy = s_gba_render_ctx.look_center_gy;
                        golf_set_aim_target(&s_gba_game_state, TO_FIXED(cgx) + FIXED_HALF, TO_FIXED(cgy) + FIXED_HALF);
                        s_gba_game_state.aim_distance_yds = s_gba_render_ctx.look_dist_yds;
                        
                        /* Auto-select optimal club for this distance */
                        if (s_gba_game_state.aim_distance_yds > 210) golf_select_club(&s_gba_game_state, CLUB_1W);
                        else if (s_gba_game_state.aim_distance_yds > 175) golf_select_club(&s_gba_game_state, CLUB_3W);
                        else if (s_gba_game_state.aim_distance_yds > 140) golf_select_club(&s_gba_game_state, CLUB_5I);
                        else if (s_gba_game_state.aim_distance_yds > 105) golf_select_club(&s_gba_game_state, CLUB_9I);
                        else if (s_gba_game_state.aim_distance_yds > 35) golf_select_club(&s_gba_game_state, CLUB_PW);
                        else golf_select_club(&s_gba_game_state, CLUB_PT);
                        
                        gba_sfx_cup_sink();
                    }
                } else {
                    /* NORMAL PLAY MODE CONTROLS */
                    /* D-PAD Left/Right: Aim Direction */
                    if (gba_key_held(KEY_LEFT)) {
                        golf_adjust_aim(&s_gba_game_state, 3);
                    }
                    if (gba_key_held(KEY_RIGHT)) {
                        golf_adjust_aim(&s_gba_game_state, -3);
                    }
                    
                    /* Responsive Club Selection:
                     * B button, D-Pad Up/Down, or Shoulder buttons (L/R)
                     * Ensures player is never trapped on a single club! */
                    if (gba_key_pressed(KEY_B) || gba_key_pressed(KEY_DOWN) || gba_key_pressed(KEY_R)) {
                        golf_change_club(&s_gba_game_state, 1);
                        gba_sfx_bounce();
                    } else if (gba_key_pressed(KEY_UP) || gba_key_pressed(KEY_L)) {
                        golf_change_club(&s_gba_game_state, -1);
                        gba_sfx_bounce();
                    }
                    
                    /* START: Randomize Course Hole and cycle color scheme */
                    if (gba_key_pressed(KEY_START)) {
                        uint32_t seed = (uint32_t)splash_timer * 1103515245 + 12345;
                        s_gba_game_state.theme = (CourseTheme)((s_gba_game_state.theme + 1) % THEME_COUNT);
                        golf_randomize_hole(&s_gba_game_state, seed);
                        gba_sfx_gopher_chuckle();
                    }
                    
                    /* A Button: 3-Click Swing System */
                    if (gba_key_pressed(KEY_A)) {
                        golf_swing_click(&s_gba_game_state);
                        if (s_gba_game_state.audio_event != AUDIO_EVT_NONE) {
                            dispatch_gba_audio(s_gba_game_state.audio_event, (int)s_gba_game_state.current_club);
                            s_gba_game_state.audio_event = AUDIO_EVT_NONE;
                        }
                    }
                }
            }
            
            /* Game Physics & Entity Simulation */
            golf_tick(&s_gba_game_state);
            
            if (s_gba_game_state.audio_event != AUDIO_EVT_NONE) {
                dispatch_gba_audio(s_gba_game_state.audio_event, (int)s_gba_game_state.current_club);
                s_gba_game_state.audio_event = AUDIO_EVT_NONE;
            }
            
            /* Render Frame & Display via VSync Double-Buffer Flip */
            gba_render_draw_frame(&s_gba_render_ctx, &s_gba_game_state);
            gba_flip();
        }
    }
    
    return 0;
}
