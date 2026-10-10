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
    golf_init(&s_gba_game_state);
    
    int splash_timer = 0;
    
    /* 2. Main Game Loop */
    while (1) {
        gba_poll_keys();
        
        if (s_gba_game_state.mode == MODE_SPLASH) {
            splash_timer++;
            
            /* Press START or A to Tee Off */
            if (gba_key_pressed(KEY_START) || gba_key_pressed(KEY_A)) {
                golf_set_mode(&s_gba_game_state, MODE_PLAY);
                gba_sfx_whistle();
            }
            
            gba_render_splash(&s_gba_render_ctx, splash_timer);
            gba_flip();
        } else {
            /* PLAY MODE */
            
            /* Key Controls */
            if (s_gba_game_state.ball.state == BALL_IDLE) {
                /* D-PAD Aiming */
                if (gba_key_held(KEY_LEFT)) {
                    s_gba_game_state.aim_angle = (s_gba_game_state.aim_angle + 3) & 0xFF;
                }
                if (gba_key_held(KEY_RIGHT)) {
                    s_gba_game_state.aim_angle = (s_gba_game_state.aim_angle - 3) & 0xFF;
                }
                if (gba_key_pressed(KEY_UP)) {
                    s_gba_game_state.aim_distance_yds += 5;
                    if (s_gba_game_state.aim_distance_yds > 350) s_gba_game_state.aim_distance_yds = 350;
                }
                if (gba_key_pressed(KEY_DOWN)) {
                    s_gba_game_state.aim_distance_yds -= 5;
                    if (s_gba_game_state.aim_distance_yds < 5) s_gba_game_state.aim_distance_yds = 5;
                }
                
                /* Shoulder Buttons: Change Club */
                if (gba_key_pressed(KEY_R)) {
                    s_gba_game_state.current_club = (ClubType)((s_gba_game_state.current_club + 1) % CLUB_COUNT);
                    gba_sfx_bounce();
                }
                if (gba_key_pressed(KEY_L)) {
                    s_gba_game_state.current_club = (ClubType)((s_gba_game_state.current_club + CLUB_COUNT - 1) % CLUB_COUNT);
                    gba_sfx_bounce();
                }
                
                /* START: Randomize Course Hole */
                if (gba_key_pressed(KEY_START)) {
                    golf_randomize_hole(&s_gba_game_state, (uint32_t)splash_timer * 1103515245 + 12345);
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
