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
                /* Center camera on golfer/ball initially */
                int bgx = FROM_FIXED(s_gba_game_state.ball.x);
                int bgy = FROM_FIXED(s_gba_game_state.ball.y);
                if (bgx < 0) bgx = 0; if (bgx >= GRID_W) bgx = GRID_W - 1;
                if (bgy < 0) bgy = 0; if (bgy >= GRID_H) bgy = GRID_H - 1;
                int belev = s_gba_game_state.grid[bgx][bgy].elevation;
                int32_t target_sx = (bgx - bgy) * GBA_TILE_HALF_W;
                int32_t target_sy = (bgx + bgy) * GBA_TILE_HALF_H - (belev * GBA_HEIGHT_STEP);
                s_gba_render_ctx.cam_x = (GBA_SCREEN_W / 2) - target_sx - GBA_TILE_HALF_W;
                s_gba_render_ctx.cam_y = (GBA_SCREEN_H / 2) - target_sy - GBA_TILE_HALF_H;
                s_gba_render_ctx.target_cam_x = s_gba_render_ctx.cam_x;
                s_gba_render_ctx.target_cam_y = s_gba_render_ctx.cam_y;
                s_gba_render_ctx.aim_locked = false;
                s_gba_render_ctx.look_mode = false;
            }
            
            gba_render_splash(&s_gba_render_ctx, splash_timer, splash_menu_choice);
            gba_flip();
        } else {
            /* PLAY MODE */
            
            /* Reset aim lock when ball is moving or being struck */
            if (s_gba_game_state.ball.state != BALL_IDLE) {
                s_gba_render_ctx.aim_locked = false;
            }
            
            /* Controls active when ball is idle and golfer is not walking */
            if (s_gba_game_state.ball.state == BALL_IDLE && s_gba_game_state.golfer.state != GOLFER_STATE_WALKING) {
                /* D-PAD: Camera / Crosshair Movement across course by default */
                int speed_x = gba_key_held(KEY_B) ? 26 : 12;
                int speed_y = gba_key_held(KEY_B) ? 18 : 8;
                
                bool dpad_moved = false;
                if (gba_key_held(KEY_UP))    { s_gba_render_ctx.cam_y += speed_y; dpad_moved = true; }
                if (gba_key_held(KEY_DOWN))  { s_gba_render_ctx.cam_y -= speed_y; dpad_moved = true; }
                if (gba_key_held(KEY_LEFT))  { s_gba_render_ctx.cam_x += speed_x; dpad_moved = true; }
                if (gba_key_held(KEY_RIGHT)) { s_gba_render_ctx.cam_x -= speed_x; dpad_moved = true; }
                
                /* Keep camera within course boundaries */
                if (s_gba_render_ctx.cam_x > 900)  s_gba_render_ctx.cam_x = 900;
                if (s_gba_render_ctx.cam_x < -900) s_gba_render_ctx.cam_x = -900;
                if (s_gba_render_ctx.cam_y > 300)  s_gba_render_ctx.cam_y = 300;
                if (s_gba_render_ctx.cam_y < -950) s_gba_render_ctx.cam_y = -950;
                
                /* If shot was locked and player moves D-Pad or presses B, unlock so player can re-aim! */
                if (s_gba_render_ctx.aim_locked && (dpad_moved || gba_key_pressed(KEY_B))) {
                    s_gba_render_ctx.aim_locked = false;
                    gba_sfx_bounce();
                }
                
                /* Shoulder Triggers (RT / LT -> R / L) to cycle through all 14 clubs:
                 * Driver -> 1W -> 2W -> 3W -> 4I..9I -> PW -> SW -> LW -> PT */
                if (gba_key_pressed(KEY_R)) {
                    golf_change_club(&s_gba_game_state, 1);
                    gba_sfx_bounce();
                } else if (gba_key_pressed(KEY_L)) {
                    golf_change_club(&s_gba_game_state, -1);
                    gba_sfx_bounce();
                }
                
                /* A BUTTON: 2-Click Sequence
                 * Click 1 (when !aim_locked): Drops pin at crosshair, marks LOCKED, returns camera to ball
                 * Click 2 (when aim_locked): Executes untimed shot */
                if (gba_key_pressed(KEY_A)) {
                    if (!s_gba_render_ctx.aim_locked) {
                        /* Click 1: Lock aim pin & return camera to ball */
                        s_gba_render_ctx.aim_target_gx = s_gba_render_ctx.cross_gx;
                        s_gba_render_ctx.aim_target_gy = s_gba_render_ctx.cross_gy;
                        s_gba_render_ctx.aim_locked = true;
                        
                        golf_set_aim_target(&s_gba_game_state,
                            TO_FIXED(s_gba_render_ctx.aim_target_gx) + FIXED_HALF,
                            TO_FIXED(s_gba_render_ctx.aim_target_gy) + FIXED_HALF);
                        s_gba_game_state.aim_distance_yds = s_gba_render_ctx.cross_dist_yds;
                        
                        /* Snap camera back to center on ball */
                        int bgx = FROM_FIXED(s_gba_game_state.ball.x);
                        int bgy = FROM_FIXED(s_gba_game_state.ball.y);
                        if (bgx < 0) bgx = 0; if (bgx >= GRID_W) bgx = GRID_W - 1;
                        if (bgy < 0) bgy = 0; if (bgy >= GRID_H) bgy = GRID_H - 1;
                        int belev = s_gba_game_state.grid[bgx][bgy].elevation;
                        int32_t target_sx = (bgx - bgy) * GBA_TILE_HALF_W;
                        int32_t target_sy = (bgx + bgy) * GBA_TILE_HALF_H - (belev * GBA_HEIGHT_STEP);
                        s_gba_render_ctx.target_cam_x = (GBA_SCREEN_W / 2) - target_sx - GBA_TILE_HALF_W;
                        s_gba_render_ctx.target_cam_y = (GBA_SCREEN_H / 2) - target_sy - GBA_TILE_HALF_H;
                        s_gba_render_ctx.cam_x = s_gba_render_ctx.target_cam_x;
                        s_gba_render_ctx.cam_y = s_gba_render_ctx.target_cam_y;
                        
                        gba_sfx_cup_sink();
                    } else {
                        /* Click 2: Untimed Shot Execution */
                        golf_execute_shot(&s_gba_game_state);
                        if (s_gba_game_state.audio_event != AUDIO_EVT_NONE) {
                            dispatch_gba_audio(s_gba_game_state.audio_event, (int)s_gba_game_state.current_club);
                            s_gba_game_state.audio_event = AUDIO_EVT_NONE;
                        }
                    }
                }
                
                /* START: Randomize Course Hole and cycle color scheme */
                if (gba_key_pressed(KEY_START)) {
                    uint32_t seed = (uint32_t)splash_timer * 1103515245 + 12345;
                    s_gba_game_state.theme = (CourseTheme)((s_gba_game_state.theme + 1) % THEME_COUNT);
                    golf_randomize_hole(&s_gba_game_state, seed);
                    s_gba_render_ctx.aim_locked = false;
                    gba_sfx_gopher_chuckle();
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
