#include "gba_hw.h"
#include "gba_render.h"
#include "golf_core.h"

static GolfState s_gba_game_state;
static GbaRenderContext s_gba_render_ctx;
static bool s_club_manually_chosen = false;

static ClubType gba_recommend_club(int32_t yds, SurfaceType surf) {
    if (surf == SURF_GREEN || yds <= 20) return CLUB_PT;
    if (surf == SURF_SAND && yds <= 90) return CLUB_SW;
    if (yds >= 250) return CLUB_DRV;
    if (yds >= 232) return CLUB_1W;
    if (yds >= 217) return CLUB_2W;
    if (yds >= 202) return CLUB_3W;
    if (yds >= 187) return CLUB_4I;
    if (yds >= 172) return CLUB_5I;
    if (yds >= 157) return CLUB_6I;
    if (yds >= 142) return CLUB_7I;
    if (yds >= 127) return CLUB_8I;
    if (yds >= 112) return CLUB_9I;
    if (yds >= 95)  return CLUB_PW;
    if (yds >= 75)  return CLUB_SW;
    return CLUB_LW;
}

static void dispatch_gba_audio(GolfAudioEvent evt, int club) {
    (void)club;
    switch (evt) {
        case AUDIO_EVT_SWING_DRIVER:   gba_sfx_swing_hit(0); break;
        case AUDIO_EVT_SWING_IRON:     gba_sfx_swing_hit(1); break;
        case AUDIO_EVT_SWING_WEDGE:    gba_sfx_swing_hit(2); break;
        case AUDIO_EVT_SWING_PUTTER:   gba_sfx_swing_hit(4); break;
        case AUDIO_EVT_BALL_BOUNCE:
            if (s_gba_game_state.ball.bounce_count <= 1) {
                gba_sfx_ball_landing();
            } else {
                gba_sfx_bounce();
            }
            break;
        case AUDIO_EVT_WATER_SPLASH:   gba_sfx_water_splash(); break;
        case AUDIO_EVT_CUP_SINK:       gba_sfx_cup_sink(); break;
        case AUDIO_EVT_GOPHER_CHUCKLE: gba_sfx_gopher_chuckle(); break;
        case AUDIO_EVT_GOPHER_GULP:    gba_sfx_gopher_gulp(); break;
        case AUDIO_EVT_CARL_EXPLOSION: gba_sfx_carl_explosion(); break;
        case AUDIO_EVT_CARL_HOSE:      gba_sfx_carl_hose(); break;
        case AUDIO_EVT_BEV_WHISTLE:    gba_sfx_whistle(); break;
        case AUDIO_EVT_BURP:           gba_sfx_burp(); break;
        case AUDIO_EVT_SHOTGUN:        gba_sfx_boomstick(); break;
        case AUDIO_EVT_GOLFER_JUMP:    gba_sfx_bounce(); break;
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
            
            /* Responsive skip-walking controls: Click A, B, or START while walking/jumping to arrive immediately */
            if (s_gba_game_state.golfer.state == GOLFER_STATE_WALKING || s_gba_game_state.golfer.state == GOLFER_STATE_JUMPING) {
                if (gba_key_pressed(KEY_A) || gba_key_pressed(KEY_B) || gba_key_pressed(KEY_START)) {
                    golf_golfer_arrive_at_ball(&s_gba_game_state);
                    s_gba_render_ctx.aim_locked = false;
                    s_club_manually_chosen = false;
                    gba_sfx_bounce();
                }
            }
            
            /* Ambient audio soundscape management based on course theme */
            static CourseTheme s_prev_theme = (CourseTheme)-1;
            if (s_gba_game_state.theme != s_prev_theme) {
                s_prev_theme = s_gba_game_state.theme;
                if (s_gba_game_state.theme == THEME_LA_PLAYA) {
                    gba_sfx_play_ambient(1); /* Desert breeze loop */
                } else {
                    gba_sfx_play_ambient(2); /* Country club water brook loop */
                }
            }

            /* Controls active when ball is idle and golfer is not walking, jumping, or taking a drink */
            if (s_gba_game_state.ball.state == BALL_IDLE && 
                s_gba_game_state.golfer.state != GOLFER_STATE_WALKING && 
                s_gba_game_state.golfer.state != GOLFER_STATE_JUMPING &&
                s_gba_game_state.golfer.state != GOLFER_STATE_DRINKING &&
                s_gba_game_state.bev_cart.state != BEV_STATE_BURPING) {
                /* D-PAD: Camera / Crosshair Movement across course (fast & turbo speeds) */
                int speed_x = gba_key_held(KEY_B) ? 56 : 32;
                int speed_y = gba_key_held(KEY_B) ? 36 : 20;
                
                bool dpad_moved = false;
                if (gba_key_held(KEY_UP))    { s_gba_render_ctx.cam_y += speed_y; dpad_moved = true; }
                if (gba_key_held(KEY_DOWN))  { s_gba_render_ctx.cam_y -= speed_y; dpad_moved = true; }
                if (gba_key_held(KEY_LEFT))  { s_gba_render_ctx.cam_x += speed_x; dpad_moved = true; }
                if (gba_key_held(KEY_RIGHT)) { s_gba_render_ctx.cam_x -= speed_x; dpad_moved = true; }
                
                /* Keep camera within course boundaries */
                if (s_gba_render_ctx.cam_x > 1200)  s_gba_render_ctx.cam_x = 1200;
                if (s_gba_render_ctx.cam_x < -1200) s_gba_render_ctx.cam_x = -1200;
                if (s_gba_render_ctx.cam_y > 400)   s_gba_render_ctx.cam_y = 400;
                if (s_gba_render_ctx.cam_y < -1200) s_gba_render_ctx.cam_y = -1200;
                
                /* If shot was locked and player moves D-Pad or presses B, unlock so player can re-aim! */
                if (s_gba_render_ctx.aim_locked && (dpad_moved || gba_key_pressed(KEY_B))) {
                    s_gba_render_ctx.aim_locked = false;
                    s_gba_render_ctx.flag_linger_timer = 0;
                    gba_sfx_bounce();
                }
                
                /* Shoulder Triggers (R / L) to cycle through all 14 clubs:
                 * Driver -> 1W -> 2W -> 3W -> 4I..9I -> PW -> SW -> LW -> PT */
                if (gba_key_pressed(KEY_R)) {
                    golf_change_club(&s_gba_game_state, 1);
                    s_club_manually_chosen = true;
                    gba_sfx_bounce();
                } else if (gba_key_pressed(KEY_L)) {
                    golf_change_club(&s_gba_game_state, -1);
                    s_club_manually_chosen = true;
                    gba_sfx_bounce();
                }
                
                /* Auto-recommend club when aiming if player hasn't manually chosen one yet */
                if (!s_gba_render_ctx.aim_locked && !s_club_manually_chosen) {
                    if (golf_is_high_column(&s_gba_game_state, s_gba_render_ctx.cross_gx, s_gba_render_ctx.cross_gy)) {
                        s_gba_game_state.current_club = CLUB_LW;
                    } else {
                        s_gba_game_state.current_club = gba_recommend_club(s_gba_render_ctx.cross_dist_yds, s_gba_game_state.ball.current_surf);
                    }
                }
                
                /* A BUTTON: 2-Click Sequence
                 * Click 1 (when !aim_locked): Drops pin at crosshair, lingers to see flag planted, then camera glides to ball
                 * Click 2 (when aim_locked): Executes untimed shot */
                if (gba_key_pressed(KEY_A)) {
                    if (!s_gba_render_ctx.aim_locked) {
                        /* Click 1: Lock aim pin & linger ~0.46s so player sees flag firmly planted! */
                        ClubType chosen_club = s_gba_game_state.current_club;
                        s_gba_render_ctx.aim_target_gx = s_gba_render_ctx.cross_gx;
                        s_gba_render_ctx.aim_target_gy = s_gba_render_ctx.cross_gy;
                        s_gba_render_ctx.aim_locked = true;
                        s_club_manually_chosen = true;
                        s_gba_render_ctx.flag_linger_timer = 28; /* Linger ~0.46s at target */
                        
                        /* High column restriction: Level tops only targetable with Lob Wedge */
                        if (golf_is_high_column(&s_gba_game_state, s_gba_render_ctx.aim_target_gx, s_gba_render_ctx.aim_target_gy)) {
                            chosen_club = CLUB_LW;
                        }
                        
                        golf_set_aim_target(&s_gba_game_state,
                            TO_FIXED(s_gba_render_ctx.aim_target_gx) + FIXED_HALF,
                            TO_FIXED(s_gba_render_ctx.aim_target_gy) + FIXED_HALF);
                        s_gba_game_state.aim_distance_yds = s_gba_render_ctx.cross_dist_yds;
                        
                        /* CRITICAL: Preserve player's chosen club against auto-recommend overwrite */
                        s_gba_game_state.current_club = chosen_club;
                        
                        gba_sfx_pin_ding();
                    } else {
                        /* Click 2: Untimed Shot Execution with Full Club Power */
                        s_gba_render_ctx.flag_linger_timer = 0;
                        const ClubData *cd = golf_get_club_data(s_gba_game_state.current_club);
                        if (!cd->is_putter) {
                            s_gba_game_state.aim_distance_yds = cd->nominal_yards;
                        }
                        golf_execute_shot(&s_gba_game_state);
                        s_club_manually_chosen = false;
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
                    s_gba_render_ctx.flag_linger_timer = 0;
                    s_club_manually_chosen = false;
                    gba_sfx_gopher_chuckle();
                }
            }
            
            /* Decrement flag planting linger countdown */
            if (s_gba_render_ctx.flag_linger_timer > 0) {
                s_gba_render_ctx.flag_linger_timer--;
            }
            
            /* Multi-tick Game Physics & Entity Simulation */
            int num_ticks = 1;
            if (s_gba_game_state.ball.state == BALL_IN_FLIGHT) {
                num_ticks = 6; /* 2x faster flight animation (doubled from 3 to 6) */
            } else if (s_gba_game_state.ball.state == BALL_BOUNCING || s_gba_game_state.ball.state == BALL_ROLLING) {
                num_ticks = 4; /* 2x faster bounce & roll (doubled from 2 to 4) */
            } else if (s_gba_game_state.golfer.state == GOLFER_STATE_WALKING) {
                num_ticks = 3; /* 3x faster walking */
            }
            
            for (int t = 0; t < num_ticks; ++t) {
                golf_tick(&s_gba_game_state);
                if (s_gba_game_state.audio_event != AUDIO_EVT_NONE) {
                    dispatch_gba_audio(s_gba_game_state.audio_event, (int)s_gba_game_state.current_club);
                    s_gba_game_state.audio_event = AUDIO_EVT_NONE;
                }
                if (s_gba_game_state.ball.state == BALL_IDLE && s_gba_game_state.golfer.state != GOLFER_STATE_WALKING) {
                    break;
                }
            }
            
            if (s_gba_game_state.audio_event != AUDIO_EVT_NONE) {
                dispatch_gba_audio(s_gba_game_state.audio_event, (int)s_gba_game_state.current_club);
                s_gba_game_state.audio_event = AUDIO_EVT_NONE;
            }
            
            /* Ambient background sound generator (chirps & breeze with silence intervals) */
            gba_audio_tick_ambient();
            
            /* Render Frame & Display via VSync Double-Buffer Flip */
            gba_render_draw_frame(&s_gba_render_ctx, &s_gba_game_state);
            gba_flip();
        }
    }
    
    return 0;
}
