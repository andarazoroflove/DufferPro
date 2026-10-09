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

/* Handle bottom tool click in Build Mode */
static bool handle_build_ui_click(GolfState *gs, int mx, int my) {
    if (my < SCREEN_H - 58) return false;
    
    /* Tool buttons */
    int btn_x = 10;
    for (int i = 0; i < TOOL_COUNT; ++i) {
        int bw = (i == TOOL_FAIRWAY || i == TOOL_ROUGH) ? 58 : 46;
        if (mx >= btn_x && mx <= btn_x + bw && my >= SCREEN_H - 48 && my <= SCREEN_H - 26) {
            golf_set_tool(gs, (BuildTool)i);
            return true;
        }
        btn_x += bw + 4;
    }
    
    /* Brush toggle click */
    if (my >= SCREEN_H - 22 && mx >= 10 && mx <= 120) {
        gs->brush_size = (gs->brush_size == 1) ? 2 : 1;
        return true;
    }
    
    return true; /* Consumed by UI */
}

/* Handle bottom tool click in Play Mode */
static bool handle_play_ui_click(GolfState *gs, int mx, int my) {
    if (my < SCREEN_H - 58) return false;
    
    /* Club Box click: cycle club */
    if (mx >= 10 && mx <= 160 && my >= SCREEN_H - 50 && my <= SCREEN_H - 8) {
        golf_change_club(gs, 1);
        return true;
    }
    
    /* Build toggle click */
    if (mx >= 520 && mx <= 630 && my >= SCREEN_H - 35) {
        golf_set_mode(gs, MODE_BUILD);
        return true;
    }
    
    /* Swing Gauge click (bar + hint prompt area) */
    if (mx >= 300 && mx <= 518) {
        golf_swing_click(gs);
        return true;
    }
    
    return true; /* Consumed by UI */
}

#if !defined(USE_WIN32_GDI)

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }
    
    SDL_Window *window = SDL_CreateWindow(
        "Duffer [Retro Golf Engine]",
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
        /* Fallback to software renderer */
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
    
    /* Initialize Game & Renderer */
    golf_init(&s_game_state);
    render_init(&s_render_ctx, s_framebuffer);
    
    /* Center camera on Tee */
    if (s_game_state.has_tee) {
        render_center_camera_on(&s_render_ctx, s_game_state.tee_gx, s_game_state.tee_gy, s_game_state.grid[s_game_state.tee_gx][s_game_state.tee_gy].elevation);
        render_pan_camera(&s_render_ctx, 0, 70);
    }
    
    bool running = true;
    SDL_Event event;
    uint32_t last_time = SDL_GetTicks();
    
    while (running) {
        /* Event Processing */
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                    
                case SDL_KEYDOWN:
                    if (s_game_state.mode == MODE_SPLASH) {
                        if (event.key.keysym.sym == SDLK_ESCAPE) {
                            running = false;
                        } else {
                            golf_set_mode(&s_game_state, MODE_PLAY);
                            if (s_game_state.has_tee) {
                                render_center_camera_on(&s_render_ctx, s_game_state.tee_gx, s_game_state.tee_gy, s_game_state.grid[s_game_state.tee_gx][s_game_state.tee_gy].elevation);
                                render_pan_camera(&s_render_ctx, 0, 70);
                            }
                        }
                        break;
                    }
                    switch (event.key.keysym.sym) {
                        case SDLK_ESCAPE:
                            running = false;
                            break;
                            
                        case SDLK_TAB:
                            /* Toggle Mode */
                            golf_set_mode(&s_game_state, (s_game_state.mode == MODE_PLAY) ? MODE_BUILD : MODE_PLAY);
                            break;
                            
                        case SDLK_SPACE:
                            if (s_game_state.mode == MODE_PLAY) {
                                golf_swing_click(&s_game_state);
                            }
                            break;
                            
                        case SDLK_LEFT:
                        case SDLK_a:
                            if (s_game_state.mode == MODE_PLAY) {
                                golf_adjust_aim(&s_game_state, -4);
                            } else {
                                render_pan_camera(&s_render_ctx, 16, 0);
                            }
                            break;
                            
                        case SDLK_RIGHT:
                        case SDLK_d:
                            if (s_game_state.mode == MODE_PLAY) {
                                golf_adjust_aim(&s_game_state, 4);
                            } else {
                                render_pan_camera(&s_render_ctx, -16, 0);
                            }
                            break;
                            
                        case SDLK_UP:
                        case SDLK_w:
                            if (s_game_state.mode == MODE_PLAY) {
                                golf_change_club(&s_game_state, -1);
                            } else {
                                render_pan_camera(&s_render_ctx, 0, 16);
                            }
                            break;
                            
                        case SDLK_DOWN:
                        case SDLK_s:
                            if (s_game_state.mode == MODE_PLAY) {
                                golf_change_club(&s_game_state, 1);
                            } else {
                                render_pan_camera(&s_render_ctx, 0, -16);
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
                            }
                            break;
                            
                        case SDLK_F12:
                            render_save_bmp(&s_render_ctx, "retrosimgolf.bmp");
                            break;
                            
                        case SDLK_g:
                            golf_randomize_hole(&s_game_state, SDL_GetTicks() ^ (uint32_t)rand());
                            if (s_game_state.has_tee) {
                                render_center_camera_on(&s_render_ctx, s_game_state.tee_gx, s_game_state.tee_gy, s_game_state.grid[s_game_state.tee_gx][s_game_state.tee_gy].elevation);
                                render_pan_camera(&s_render_ctx, 0, 70);
                            }
                            break;
                            
                        case SDLK_1: golf_set_tool(&s_game_state, TOOL_FAIRWAY); break;
                        case SDLK_2: golf_set_tool(&s_game_state, TOOL_GREEN); break;
                        case SDLK_3: golf_set_tool(&s_game_state, TOOL_SAND); break;
                        case SDLK_4: golf_set_tool(&s_game_state, TOOL_ROUGH); break;
                        case SDLK_5: golf_set_tool(&s_game_state, TOOL_TEE); break;
                        case SDLK_6: golf_set_tool(&s_game_state, TOOL_PIN); break;
                        case SDLK_7: golf_set_tool(&s_game_state, TOOL_PINE); break;
                        case SDLK_8: golf_set_tool(&s_game_state, TOOL_OAK); break;
                        case SDLK_9: golf_set_tool(&s_game_state, TOOL_ROCK); break;
                        case SDLK_0: golf_set_tool(&s_game_state, TOOL_BULLDOZE); break;
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
                        if (s_lbutton_down && s_game_state.mode == MODE_BUILD && s_render_ctx.mouse_valid && my < SCREEN_H - 58 && my > 28) {
                            golf_build_apply_tool(&s_game_state, s_render_ctx.mouse_gx, s_render_ctx.mouse_gy);
                        }
                        
                        /* Dynamic drag-aiming in Play Mode */
                        if (s_lbutton_down && s_game_state.mode == MODE_PLAY && s_game_state.swing.state == SWING_STATE_READY && my < SCREEN_H - 58 && my > 28) {
                            fixed target_fx, target_fy;
                            if (screen_to_world_fixed(&s_game_state, mx, my, s_render_ctx.camera_x, s_render_ctx.camera_y, &target_fx, &target_fy)) {
                                golf_set_aim_target(&s_game_state, target_fx, target_fy);
                            }
                        }
                    }
                    break;
                    
                case SDL_MOUSEBUTTONDOWN:
                    if (s_game_state.mode == MODE_SPLASH) {
                        golf_set_mode(&s_game_state, MODE_PLAY);
                        if (s_game_state.has_tee) {
                            render_center_camera_on(&s_render_ctx, s_game_state.tee_gx, s_game_state.tee_gy, s_game_state.grid[s_game_state.tee_gx][s_game_state.tee_gy].elevation);
                            render_pan_camera(&s_render_ctx, 0, 70);
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
                        
                        /* Check Randomize Hole Button click at top bar */
                        if (my <= 28 && mx >= 432 && mx <= 544) {
                            golf_randomize_hole(&s_game_state, SDL_GetTicks() ^ (uint32_t)rand());
                            if (s_game_state.has_tee) {
                                render_center_camera_on(&s_render_ctx, s_game_state.tee_gx, s_game_state.tee_gy, s_game_state.grid[s_game_state.tee_gx][s_game_state.tee_gy].elevation);
                                render_pan_camera(&s_render_ctx, 0, 70);
                            }
                            break;
                        }
                        
                        /* Check Mode Badge click at top right */
                        if (my <= 28 && mx >= 552) {
                            golf_set_mode(&s_game_state, (s_game_state.mode == MODE_PLAY) ? MODE_BUILD : MODE_PLAY);
                            break;
                        }
                        
                        /* Check bottom UI click */
                        bool handled = false;
                        if (s_game_state.mode == MODE_BUILD) {
                            handled = handle_build_ui_click(&s_game_state, mx, my);
                        } else {
                            handled = handle_play_ui_click(&s_game_state, mx, my);
                        }
                        
                        /* World click */
                        if (!handled && my > 28 && my < SCREEN_H - 58) {
                            if (s_game_state.mode == MODE_BUILD) {
                                if (s_render_ctx.mouse_valid) {
                                    golf_build_apply_tool(&s_game_state, s_render_ctx.mouse_gx, s_render_ctx.mouse_gy);
                                }
                            } else {
                                /* Play Mode */
                                if (s_game_state.swing.state == SWING_STATE_READY) {
                                    /* Check if clicking directly on the golfer / ball (within 18 pixels) to start swing */
                                    fixed fx = s_game_state.ball.x;
                                    fixed fy = s_game_state.ball.y;
                                    fixed gz = s_game_state.ball.ground_z;
                                    int32_t bsx = FROM_FIXED(FMUL(fx - fy, TO_FIXED(TILE_HALF_W))) + s_render_ctx.camera_x + TILE_HALF_W;
                                    int32_t bsy = FROM_FIXED(FMUL(fx + fy, TO_FIXED(TILE_HALF_H))) - FROM_FIXED(gz) + s_render_ctx.camera_y + TILE_HALF_H - 10;
                                    int cdx = mx - bsx;
                                    int cdy = my - bsy;
                                    if (cdx * cdx + cdy * cdy <= 18 * 18) {
                                        golf_swing_click(&s_game_state);
                                    } else {
                                        /* Clicked terrain: set aiming target spot! */
                                        fixed target_fx, target_fy;
                                        if (screen_to_world_fixed(&s_game_state, mx, my, s_render_ctx.camera_x, s_render_ctx.camera_y, &target_fx, &target_fy)) {
                                            golf_set_aim_target(&s_game_state, target_fx, target_fy);
                                        }
                                    }
                                } else {
                                    /* Swing in progress: click anywhere to lock power or accuracy */
                                    golf_swing_click(&s_game_state);
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
        
        /* Simulation Tick */
        golf_tick(&s_game_state);
        
        /* Camera follows ball dynamically during flight or rolling */
        if (s_game_state.ball.state == BALL_IN_FLIGHT || s_game_state.ball.state == BALL_ROLLING) {
            int32_t bgx = FROM_FIXED(s_game_state.ball.x);
            int32_t bgy = FROM_FIXED(s_game_state.ball.y);
            int32_t target_sx = (bgx - bgy) * TILE_HALF_W;
            int32_t target_sy = (bgx + bgy) * TILE_HALF_H - FROM_FIXED(s_game_state.ball.ground_z);
            
            int32_t desired_cam_x = SCREEN_W / 2 - target_sx - TILE_HALF_W;
            int32_t desired_cam_y = SCREEN_H / 2 - target_sy - TILE_HALF_H;
            
            /* Smooth camera interpolation */
            s_render_ctx.camera_x += (desired_cam_x - s_render_ctx.camera_x) / 8;
            s_render_ctx.camera_y += (desired_cam_y - s_render_ctx.camera_y) / 8;
        }
        
        /* Render Frame */
        render_frame(&s_render_ctx, &s_game_state);
        
        /* Blit Framebuffer to SDL Texture */
        SDL_UpdateTexture(texture, NULL, s_framebuffer, SCREEN_W * sizeof(uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
        
        /* Frame Rate Limiting (60 Hz) */
        uint32_t current_time = SDL_GetTicks();
        uint32_t elapsed = current_time - last_time;
        if (elapsed < FRAME_DELAY_MS) {
            SDL_Delay(FRAME_DELAY_MS - elapsed);
        }
        last_time = SDL_GetTicks();
    }
    
    /* Cleanup */
    render_free(&s_render_ctx);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

#endif /* !USE_WIN32_GDI */
