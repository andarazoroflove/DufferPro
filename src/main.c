#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#if defined(USE_WIN32_GDI)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <SDL2/SDL.h>
#endif

#include "golf_core.h"
#include "golf_render.h"
#include "golf_audio.h"

#define FPS                60
#define FRAME_DELAY_MS     (1000 / FPS)

/* Global Engine State */
static GolfState s_game_state;
static RenderContext s_render_ctx;
static uint32_t s_framebuffer[SCREEN_W * SCREEN_H];

/* Camera panning state */
static bool s_rbutton_down = false;
static int s_prev_mouse_x = 0;
static int s_prev_mouse_y = 0;
static bool s_lbutton_down = false;
static bool s_free_cam = false;

/* Dispatch procedural audio events immediately */
static void dispatch_audio_events(void) {
    if (s_game_state.audio_event != AUDIO_EVT_NONE) {
        golf_audio_play_sfx((GolfSfxType)s_game_state.audio_event);
        s_game_state.audio_event = AUDIO_EVT_NONE;
    }
}

/* Handle bottom tool click in Build Mode */
static bool handle_build_ui_click(GolfState *gs, int mx, int my) {
    int btm_y = SCREEN_H - 70;
    
    /* 1. Category Tabs at y = 674..698 */
    if (my >= btm_y - 26 && my < btm_y) {
        int tab_w = 110;
        for (int c = 0; c < BUILD_CAT_COUNT; ++c) {
            int tx = 10 + c * (tab_w + 6);
            if (mx >= tx && mx <= tx + tab_w) {
                golf_set_build_category(gs, (BuildCategory)c);
                return true;
            }
        }
        return true;
    }
    
    if (my < btm_y) return false;
    
    int btn_y = btm_y + 8;
    int bh = 32;
    
    /* Brush toggle click */
    int brush_x = SCREEN_W - 130;
    if (mx >= brush_x && mx <= brush_x + 115 && my >= btn_y && my <= btn_y + bh) {
        gs->brush_size = (gs->brush_size == 1) ? 2 : 1;
        return true;
    }
    
    /* Category Tools */
    int btn_x = 10;
    int bw = 85;
    
    if (gs->build_category == BUILD_CAT_TERRAIN) {
        BuildTool t_list[8] = { TOOL_FAIRWAY, TOOL_GREEN, TOOL_SAND, TOOL_ROUGH, TOOL_WATER, TOOL_CARTPATH, TOOL_RAISE, TOOL_LOWER };
        for (int i = 0; i < 8; ++i) {
            if (mx >= btn_x && mx <= btn_x + bw && my >= btn_y && my <= btn_y + bh) {
                golf_set_tool(gs, t_list[i]);
                return true;
            }
            btn_x += bw + 8;
        }
        /* Bulldoze button */
        if (mx >= btn_x && mx <= btn_x + bw && my >= btn_y && my <= btn_y + bh) {
            golf_set_tool(gs, TOOL_BULLDOZE);
            return true;
        }
    } else if (gs->build_category == BUILD_CAT_COURSE) {
        BuildTool t_list[5] = { TOOL_TEE, TOOL_PIN, TOOL_BRIDGE, TOOL_BENCH, TOOL_CART };
        for (int i = 0; i < 5; ++i) {
            if (mx >= btn_x && mx <= btn_x + bw + 10 && my >= btn_y && my <= btn_y + bh) {
                golf_set_tool(gs, t_list[i]);
                return true;
            }
            btn_x += bw + 18;
        }
    } else if (gs->build_category == BUILD_CAT_SCENERY) {
        BuildTool t_list[8] = { TOOL_PINE, TOOL_OAK, TOOL_PALM, TOOL_FLOWERING, TOOL_BUSH, TOOL_FLOWERS, TOOL_ROCK, TOOL_CLUBHOUSE };
        for (int i = 0; i < 8; ++i) {
            int w_btn = (i == 7) ? 95 : bw;
            if (mx >= btn_x && mx <= btn_x + w_btn && my >= btn_y && my <= btn_y + bh) {
                golf_set_tool(gs, t_list[i]);
                return true;
            }
            btn_x += w_btn + 6;
        }
    }
    
    return true; /* Consumed by UI */
}

/* Handle bottom tool click in Play Mode */
static bool handle_play_ui_click(GolfState *gs, int mx, int my) {
    int btm_y = SCREEN_H - 70;
    if (my < btm_y) return false;
    
    /* Club Box click: cycle club */
    if (mx >= 14 && mx <= 174 && my >= btm_y + 8 && my <= btm_y + 60) {
        golf_change_club(gs, 1);
        return true;
    }
    
    /* Swing Gauge click (bar + hint prompt area) */
    if (mx >= 350 && mx <= 690 && my >= btm_y + 8 && my <= btm_y + 60) {
        s_free_cam = false;
        golf_swing_click(gs);
        dispatch_audio_events();
        return true;
    }
    
    return true; /* Consumed by UI */
}

#if !defined(USE_WIN32_GDI)

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }
    
    SDL_Window *window = SDL_CreateWindow(
        "Duffer-Pro [SimGolf Championship Edition]",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_W,
        SCREEN_H,
        SDL_WINDOW_SHOWN
    );
    
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    
    SDL_Renderer *renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    SDL_Texture *texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        SCREEN_W,
        SCREEN_H
    );
    
    if (!texture) {
        fprintf(stderr, "SDL_CreateTexture Error: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    /* Initialize Game, Renderer & Audio */
    golf_init(&s_game_state);
    render_init(&s_render_ctx, s_framebuffer);
    golf_audio_init();
    golf_audio_start_intro_music();
    
    /* Center camera on Tee */
    if (s_game_state.has_tee) {
        render_center_camera_on(&s_render_ctx, s_game_state.tee_gx, s_game_state.tee_gy, s_game_state.grid[s_game_state.tee_gx][s_game_state.tee_gy].elevation);
        render_pan_camera(&s_render_ctx, 0, 70);
    }
    
    bool running = true;
    SDL_Event event;
    uint32_t last_time = SDL_GetTicks();
    
    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                    
                case SDL_KEYDOWN:
                    if (s_game_state.mode == MODE_SPLASH) {
                        /* Only Return/Enter starts the game from splash screen */
                        if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) {
                            golf_audio_stop_intro_music();
                            golf_set_mode(&s_game_state, MODE_PLAY);
                            s_game_state.swing.state = SWING_STATE_READY;
                            s_game_state.swing.cursor_pos = s_game_state.swing.sweetspot_center;
                            if (s_game_state.has_tee) {
                                render_center_camera_on(&s_render_ctx, s_game_state.tee_gx, s_game_state.tee_gy, s_game_state.grid[s_game_state.tee_gx][s_game_state.tee_gy].elevation);
                                s_free_cam = false;
                            }
                        }
                        break;
                    }
                    switch (event.key.keysym.sym) {
                        case SDLK_ESCAPE:
                            running = false;
                            break;
                            
                        case SDLK_TAB:
                            golf_set_mode(&s_game_state, (s_game_state.mode == MODE_PLAY) ? MODE_BUILD : MODE_PLAY);
                            break;
                            
                        case SDLK_SPACE:
                            if (s_game_state.mode == MODE_PLAY) {
                                s_free_cam = false;
                                golf_swing_click(&s_game_state);
                                dispatch_audio_events();
                            }
                            break;
                            
                        case SDLK_h:
                        case SDLK_HOME:
                            s_free_cam = false;
                            break;
                            
                        case SDLK_LEFT:
                        case SDLK_a:
                            if (s_game_state.mode == MODE_PLAY) {
                                golf_adjust_aim(&s_game_state, -4);
                            } else {
                                render_pan_camera(&s_render_ctx, 20, 0);
                            }
                            break;
                            
                        case SDLK_RIGHT:
                        case SDLK_d:
                            if (s_game_state.mode == MODE_PLAY) {
                                golf_adjust_aim(&s_game_state, 4);
                            } else {
                                render_pan_camera(&s_render_ctx, -20, 0);
                            }
                            break;
                            
                        case SDLK_UP:
                        case SDLK_w:
                            if (s_game_state.mode == MODE_PLAY) {
                                golf_change_club(&s_game_state, -1);
                            } else {
                                render_pan_camera(&s_render_ctx, 0, 20);
                            }
                            break;
                            
                        case SDLK_DOWN:
                        case SDLK_s:
                            if (s_game_state.mode == MODE_PLAY) {
                                golf_change_club(&s_game_state, 1);
                            } else {
                                render_pan_camera(&s_render_ctx, 0, -20);
                            }
                            break;
                            
                        case SDLK_c:
                            golf_change_club(&s_game_state, 1);
                            break;
                            
                        case SDLK_b:
                            s_game_state.brush_size = (s_game_state.brush_size == 1) ? 2 : 1;
                            break;
                            
                        case SDLK_r:
                            golf_reset_ball_to_tee(&s_game_state);
                            if (s_game_state.has_tee) {
                                render_center_camera_on(&s_render_ctx, s_game_state.tee_gx, s_game_state.tee_gy, s_game_state.grid[s_game_state.tee_gx][s_game_state.tee_gy].elevation);
                                s_free_cam = false;
                            }
                            break;
                            
                        case SDLK_F12:
                            render_save_bmp(&s_render_ctx, "duffer_pro.bmp");
                            break;
                            
                        case SDLK_g:
                            golf_randomize_hole(&s_game_state, SDL_GetTicks() ^ (uint32_t)rand());
                            if (s_game_state.has_tee) {
                                render_center_camera_on(&s_render_ctx, s_game_state.tee_gx, s_game_state.tee_gy, s_game_state.grid[s_game_state.tee_gx][s_game_state.tee_gy].elevation);
                                s_free_cam = false;
                            }
                            break;
                            
                        case SDLK_1: golf_set_build_category(&s_game_state, BUILD_CAT_TERRAIN); break;
                        case SDLK_2: golf_set_build_category(&s_game_state, BUILD_CAT_COURSE); break;
                        case SDLK_3: golf_set_build_category(&s_game_state, BUILD_CAT_SCENERY); break;
                        default: break;
                    }
                    break;
                    
                case SDL_MOUSEMOTION:
                    {
                        int mx = event.motion.x;
                        int my = event.motion.y;
                        
                        s_render_ctx.mouse_sx = mx;
                        s_render_ctx.mouse_sy = my;
                        
                        /* Right button drag pans camera */
                        if (s_rbutton_down) {
                            int dx = mx - s_prev_mouse_x;
                            int dy = my - s_prev_mouse_y;
                            render_pan_camera(&s_render_ctx, dx, dy);
                            s_free_cam = true;
                        }
                        
                        s_prev_mouse_x = mx;
                        s_prev_mouse_y = my;
                        
                        /* Hover calculation */
                        s_render_ctx.mouse_valid = screen_to_grid(
                            &s_game_state,
                            mx, my,
                            s_render_ctx.camera_x,
                            s_render_ctx.camera_y,
                            &s_render_ctx.mouse_gx,
                            &s_render_ctx.mouse_gy
                        );
                        
                        /* Continuous brush painting on left-drag */
                        if (s_lbutton_down && s_game_state.mode == MODE_BUILD && s_render_ctx.mouse_valid && my < SCREEN_H - 70 && my > 48) {
                            golf_build_apply_tool(&s_game_state, s_render_ctx.mouse_gx, s_render_ctx.mouse_gy);
                            dispatch_audio_events();
                        }
                        
                        /* Drag-aiming in Play Mode */
                        if (s_lbutton_down && s_game_state.mode == MODE_PLAY && s_game_state.swing.state == SWING_STATE_READY && my < SCREEN_H - 70 && my > 48) {
                            fixed target_fx, target_fy;
                            if (screen_to_world_fixed(&s_game_state, mx, my, s_render_ctx.camera_x, s_render_ctx.camera_y, &target_fx, &target_fy)) {
                                golf_set_aim_target(&s_game_state, target_fx, target_fy);
                            }
                        }
                    }
                    break;
                    
                case SDL_MOUSEBUTTONDOWN:
                    if (s_game_state.mode == MODE_SPLASH) {
                        int mx = event.button.x;
                        int my = event.button.y;
                        int btn_w = 440;
                        int btn_h = 44;
                        int btn_x = SCREEN_W / 2 - btn_w / 2;
                        int btn_y = 646;
                        if ((mx >= btn_x && mx <= btn_x + btn_w && my >= btn_y && my <= btn_y + btn_h) || my >= 640) {
                            golf_audio_stop_intro_music();
                            golf_set_mode(&s_game_state, MODE_PLAY);
                            s_game_state.swing.state = SWING_STATE_READY;
                            s_game_state.swing.cursor_pos = s_game_state.swing.sweetspot_center;
                            if (s_game_state.has_tee) {
                                render_center_camera_on(&s_render_ctx, s_game_state.tee_gx, s_game_state.tee_gy, s_game_state.grid[s_game_state.tee_gx][s_game_state.tee_gy].elevation);
                                s_free_cam = false;
                            }
                        }
                        break;
                    }
                    if (event.button.button == SDL_BUTTON_RIGHT) {
                        s_rbutton_down = true;
                        s_prev_mouse_x = event.button.x;
                        s_prev_mouse_y = event.button.y;
                    } else if (event.button.button == SDL_BUTTON_LEFT) {
                        s_lbutton_down = true;
                        int mx = event.button.x;
                        int my = event.button.y;
                        
                        /* Top bar clicks */
                        if (my <= 46) {
                            int btn_rand_x = SCREEN_W - 275;
                            if (mx >= btn_rand_x && mx <= btn_rand_x + 125) {
                                golf_randomize_hole(&s_game_state, SDL_GetTicks() ^ (uint32_t)rand());
                                if (s_game_state.has_tee) {
                                    render_center_camera_on(&s_render_ctx, s_game_state.tee_gx, s_game_state.tee_gy, s_game_state.grid[s_game_state.tee_gx][s_game_state.tee_gy].elevation);
                                    render_pan_camera(&s_render_ctx, 0, 70);
                                }
                                break;
                            }
                            int btn_mode_x = SCREEN_W - 140;
                            if (mx >= btn_mode_x && mx <= btn_mode_x + 125) {
                                golf_set_mode(&s_game_state, (s_game_state.mode == MODE_PLAY) ? MODE_BUILD : MODE_PLAY);
                                break;
                            }
                        }
                        
                        /* Bottom UI click */
                        bool handled = false;
                        if (s_game_state.mode == MODE_BUILD) {
                            handled = handle_build_ui_click(&s_game_state, mx, my);
                        } else {
                            handled = handle_play_ui_click(&s_game_state, mx, my);
                        }
                        
                        /* World click */
                        if (!handled && my > 48 && my < SCREEN_H - 70) {
                            if (s_game_state.mode == MODE_BUILD) {
                                if (s_render_ctx.mouse_valid) {
                                    golf_build_apply_tool(&s_game_state, s_render_ctx.mouse_gx, s_render_ctx.mouse_gy);
                                    dispatch_audio_events();
                                }
                            } else {
                                /* Play Mode */
                                if (s_game_state.swing.state == SWING_STATE_READY) {
                                    /* Check if clicking golfer or ball to swing */
                                    fixed fx = s_game_state.ball.x;
                                    fixed fy = s_game_state.ball.y;
                                    fixed gz = s_game_state.ball.ground_z;
                                    int32_t bsx = FROM_FIXED(FMUL(fx - fy, TO_FIXED(TILE_HALF_W))) + s_render_ctx.camera_x + TILE_HALF_W;
                                    int32_t bsy = FROM_FIXED(FMUL(fx + fy, TO_FIXED(TILE_HALF_H))) - FROM_FIXED(gz) + s_render_ctx.camera_y + TILE_HALF_H - 10;
                                    int cdx = mx - bsx;
                                    int cdy = my - bsy;
                                    if (cdx * cdx + cdy * cdy <= 22 * 22) {
                                        s_free_cam = false;
                                        golf_swing_click(&s_game_state);
                                        dispatch_audio_events();
                                    } else {
                                        /* Clicked terrain: set aiming spot */
                                        fixed target_fx, target_fy;
                                        if (screen_to_world_fixed(&s_game_state, mx, my, s_render_ctx.camera_x, s_render_ctx.camera_y, &target_fx, &target_fy)) {
                                            golf_set_aim_target(&s_game_state, target_fx, target_fy);
                                        }
                                    }
                                } else {
                                    /* Swing in progress: click to lock power / accuracy */
                                    s_free_cam = false;
                                    golf_swing_click(&s_game_state);
                                    dispatch_audio_events();
                                }
                            }
                        }
                    }
                    break;
                    
                case SDL_MOUSEBUTTONUP:
                    if (event.button.button == SDL_BUTTON_RIGHT) {
                        s_rbutton_down = false;
                    } else if (event.button.button == SDL_BUTTON_LEFT) {
                        s_lbutton_down = false;
                    }
                    break;
            }
        }
        
        /* Edge Scrolling */
        if (s_game_state.mode != MODE_SPLASH) {
            int mouse_x, mouse_y;
            SDL_GetMouseState(&mouse_x, &mouse_y);
            if (mouse_x >= 0 && mouse_x < SCREEN_W && mouse_y >= 0 && mouse_y < SCREEN_H) {
                const int margin = 20;
                const int scroll_speed = 4;
                bool edge_scrolled = false;
                
                if (mouse_x < margin) {
                    render_pan_camera(&s_render_ctx, scroll_speed, 0);
                    edge_scrolled = true;
                } else if (mouse_x >= SCREEN_W - margin) {
                    render_pan_camera(&s_render_ctx, -scroll_speed, 0);
                    edge_scrolled = true;
                }
                
                if (mouse_y < margin && !(mouse_x >= SCREEN_W - 300)) {
                    render_pan_camera(&s_render_ctx, 0, scroll_speed);
                    edge_scrolled = true;
                } else if (mouse_y >= SCREEN_H - margin) {
                    render_pan_camera(&s_render_ctx, 0, -scroll_speed);
                    edge_scrolled = true;
                }
                
                if (edge_scrolled) {
                    s_free_cam = true;
                }
            }
        }
        
        /* Simulation Tick */
        golf_tick(&s_game_state);
        
        /* Audio SFX Dispatch */
        dispatch_audio_events();
        
        /* Audio Ambient Soundscape (Wind + Water + Clubhouse Proximity) */
        if (s_game_state.mode == MODE_PLAY || s_game_state.mode == MODE_BUILD) {
            fixed ear_x = (s_game_state.golfer.state == GOLFER_STATE_WALKING) ? s_game_state.golfer.x : s_game_state.ball.x;
            fixed ear_y = (s_game_state.golfer.state == GOLFER_STATE_WALKING) ? s_game_state.golfer.y : s_game_state.ball.y;
            int water_prox = golf_get_water_proximity(&s_game_state, ear_x, ear_y);
            int club_prox = golf_get_clubhouse_proximity(&s_game_state, ear_x, ear_y);
            golf_audio_update_ambient_pro(s_game_state.wind.speed, water_prox, club_prox);
        } else {
            golf_audio_update_ambient_pro(0, 0, 0);
        }
        
        /* Camera Tracking in Play Mode */
        if (s_game_state.mode == MODE_PLAY) {
            bool in_action = (s_game_state.ball.state == BALL_IN_FLIGHT || 
                              s_game_state.ball.state == BALL_ROLLING || 
                              s_game_state.golfer.state == GOLFER_STATE_WALKING || 
                              s_game_state.swing.state != SWING_STATE_READY);
            if (in_action) {
                s_free_cam = false;
            }
            
            if (!s_free_cam) {
                fixed focus_x, focus_y, focus_gz;
                if (s_game_state.golfer.state == GOLFER_STATE_WALKING) {
                    focus_x = s_game_state.golfer.x;
                    focus_y = s_game_state.golfer.y;
                    focus_gz = (s_game_state.golfer.current_z > 0) ? s_game_state.golfer.current_z : golf_get_ground_z_fixed(&s_game_state, focus_x, focus_y);
                    if (focus_gz < TO_FIXED(HEIGHT_STEP_PX)) focus_gz = TO_FIXED(HEIGHT_STEP_PX);
                } else {
                    focus_x = s_game_state.ball.x;
                    focus_y = s_game_state.ball.y;
                    focus_gz = s_game_state.ball.ground_z;
                }
                
                int32_t target_sx = FROM_FIXED(FMUL(focus_x - focus_y, TO_FIXED(TILE_HALF_W)));
                int32_t target_sy = FROM_FIXED(FMUL(focus_x + focus_y, TO_FIXED(TILE_HALF_H))) - FROM_FIXED(focus_gz);
                
                int32_t desired_cam_x = SCREEN_W / 2 - target_sx - TILE_HALF_W;
                int32_t desired_cam_y = SCREEN_H / 2 - target_sy - TILE_HALF_H;
                
                int smooth_factor = in_action ? 4 : 6;
                s_render_ctx.camera_x += (desired_cam_x - s_render_ctx.camera_x) / smooth_factor;
                s_render_ctx.camera_y += (desired_cam_y - s_render_ctx.camera_y) / smooth_factor;
            }
        }
        
        /* Clamp camera to keep course bounded */
        if (s_render_ctx.camera_x < -1800) s_render_ctx.camera_x = -1800;
        if (s_render_ctx.camera_x > 2200)  s_render_ctx.camera_x = 2200;
        if (s_render_ctx.camera_y < -1500) s_render_ctx.camera_y = -1500;
        if (s_render_ctx.camera_y > 600)   s_render_ctx.camera_y = 600;
        
        /* Render Frame */
        render_frame(&s_render_ctx, &s_game_state);
        
        /* Blit Framebuffer to SDL Texture */
        SDL_UpdateTexture(texture, NULL, s_framebuffer, SCREEN_W * sizeof(uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
        
        /* Frame Rate Limiting */
        uint32_t current_time = SDL_GetTicks();
        uint32_t elapsed = current_time - last_time;
        if (elapsed < 12) {
            SDL_Delay(1);
        }
        last_time = current_time;
    }
    
    /* Cleanup */
    golf_audio_shutdown();
    render_free(&s_render_ctx);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

#endif /* !USE_WIN32_GDI */
