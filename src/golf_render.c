#include "golf_render.h"
#include "font8x8.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#ifndef COLOR_ARGB
#define COLOR_ARGB(a, r, g, b) (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))
#define COLOR_RGB(r, g, b)     COLOR_ARGB(255, r, g, b)
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* Procedural Sprite Buffers */
static uint32_t s_tile_rough[4][TILE_W * TILE_H];
static uint32_t s_tile_fairway[16][TILE_W * TILE_H];
static uint32_t s_tile_green[16][TILE_W * TILE_H];
static uint32_t s_tile_sand[16][TILE_W * TILE_H];
static uint32_t s_tile_water[4][TILE_W * TILE_H];
static uint32_t s_tile_cartpath[16][TILE_W * TILE_H];
static uint32_t s_tile_bridge[4][TILE_W * TILE_H];
static uint32_t s_tile_tee[TILE_W * TILE_H];

/* Cliff Strata Textures (up to 6 levels * 12px = 72px) */
static uint32_t s_cliff_sw[64 * 72];
static uint32_t s_cliff_se[64 * 72];

/* Dynamic Scenery Sprites (Mid-2000s High Detail) */
#define SPRITE_PINE_W 44
#define SPRITE_PINE_H 64
static uint32_t s_sprite_pine[SPRITE_PINE_W * SPRITE_PINE_H];

#define SPRITE_OAK_W  52
#define SPRITE_OAK_H  60
static uint32_t s_sprite_oak[SPRITE_OAK_W * SPRITE_OAK_H];

#define SPRITE_PALM_W 46
#define SPRITE_PALM_H 62
static uint32_t s_sprite_palm[SPRITE_PALM_W * SPRITE_PALM_H];

#define SPRITE_FLOWERING_W 48
#define SPRITE_FLOWERING_H 58
static uint32_t s_sprite_flowering[SPRITE_FLOWERING_W * SPRITE_FLOWERING_H];

#define SPRITE_BUSH_W 32
#define SPRITE_BUSH_H 24
static uint32_t s_sprite_bush[SPRITE_BUSH_W * SPRITE_BUSH_H];

#define SPRITE_FLOWERS_W 34
#define SPRITE_FLOWERS_H 20
static uint32_t s_sprite_flowers[SPRITE_FLOWERS_W * SPRITE_FLOWERS_H];

#define SPRITE_ROCK_W 36
#define SPRITE_ROCK_H 26
static uint32_t s_sprite_rock[SPRITE_ROCK_W * SPRITE_ROCK_H];

#define SPRITE_BENCH_W 34
#define SPRITE_BENCH_H 22
static uint32_t s_sprite_bench[SPRITE_BENCH_W * SPRITE_BENCH_H];

#define SPRITE_CART_W 44
#define SPRITE_CART_H 36
static uint32_t s_sprite_cart[SPRITE_CART_W * SPRITE_CART_H];

#define SPRITE_BEV_CART_W 46
#define SPRITE_BEV_CART_H 38
static uint32_t s_sprite_bev_cart[SPRITE_BEV_CART_W * SPRITE_BEV_CART_H];

#define SPRITE_FLAG_W 20
#define SPRITE_FLAG_H 46
static uint32_t s_sprite_flag[4][SPRITE_FLAG_W * SPRITE_FLAG_H];

/* Characters & People */
#define SPRITE_GOLFER_W 28
#define SPRITE_GOLFER_H 38
static uint32_t s_sprite_golfer[SPRITE_GOLFER_W * SPRITE_GOLFER_H];
static uint32_t s_sprite_golfer_walk[6][SPRITE_GOLFER_W * SPRITE_GOLFER_H];
static uint32_t s_sprite_golfer_jump[SPRITE_GOLFER_W * SPRITE_GOLFER_H];
static uint32_t s_sprite_golfer_drink[SPRITE_GOLFER_W * SPRITE_GOLFER_H];

#define SPRITE_CADDY_W 28
#define SPRITE_CADDY_H 38
static uint32_t s_sprite_caddy[SPRITE_CADDY_W * SPRITE_CADDY_H];
static uint32_t s_sprite_caddy_walk[6][SPRITE_CADDY_W * SPRITE_CADDY_H];
static uint32_t s_sprite_caddy_celebrate[SPRITE_CADDY_W * SPRITE_CADDY_H];

#define SPRITE_SPEC_W 20
#define SPRITE_SPEC_H 30
static uint32_t s_sprite_spec[5][4][SPRITE_SPEC_W * SPRITE_SPEC_H]; /* [attire][anim] */

/* Gopher & Gopher Hole (Caddyshack Style) */
#define SPRITE_GOPHER_HOLE_W 28
#define SPRITE_GOPHER_HOLE_H 18
static uint32_t s_sprite_gopher_hole[SPRITE_GOPHER_HOLE_W * SPRITE_GOPHER_HOLE_H];

#define SPRITE_GOPHER_W 26
#define SPRITE_GOPHER_H 28
static uint32_t s_sprite_gopher[4][SPRITE_GOPHER_W * SPRITE_GOPHER_H]; /* [anim_frame] */

/* Carl Spackler Greenskeeper & TNT Blasting Plunger */
#define SPRITE_CARL_W 32
#define SPRITE_CARL_H 40
static uint32_t s_sprite_carl_walk[4][SPRITE_CARL_W * SPRITE_CARL_H];
static uint32_t s_sprite_carl_plant[2][SPRITE_CARL_W * SPRITE_CARL_H];
static uint32_t s_sprite_carl_plunge[2][SPRITE_CARL_W * SPRITE_CARL_H];
static uint32_t s_sprite_carl_hose[2][SPRITE_CARL_W * SPRITE_CARL_H];
static uint32_t s_sprite_carl_celebrate[2][SPRITE_CARL_W * SPRITE_CARL_H];

#define SPRITE_TNT_BOX_W 16
#define SPRITE_TNT_BOX_H 18
static uint32_t s_sprite_tnt_box[2][SPRITE_TNT_BOX_W * SPRITE_TNT_BOX_H];

/* Depth Buffer for screen-space occlusion (1024x768) */
static int32_t s_depth_buffer[SCREEN_W * SCREEN_H];

/* Fast PRNG for procedural textures */
static uint32_t s_tex_rand = 987654321;
static uint32_t trand(void) {
    s_tex_rand = s_tex_rand * 1664525 + 1013904223;
    return (s_tex_rand >> 16) & 0x7FFF;
}

/* Helper: Check if (px, py) is inside 64x32 diamond */
static inline bool is_in_diamond(int px, int py, int *dist_from_edge) {
    int min_x, max_x;
    if (py < 16) {
        min_x = 31 - (py * 2);
        max_x = 32 + (py * 2);
    } else {
        int r = 31 - py;
        min_x = 31 - (r * 2);
        max_x = 32 + (r * 2);
    }
    
    if (px >= min_x && px <= max_x) {
        if (dist_from_edge) {
            int d1 = px - min_x;
            int d2 = max_x - px;
            *dist_from_edge = (d1 < d2) ? d1 : d2;
        }
        return true;
    }
    return false;
}

/* Color blend helper */
static inline uint32_t blend_color(uint32_t c1, uint32_t c2, int t256) {
    if (t256 <= 0) return c1;
    if (t256 >= 256) return c2;
    int r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF;
    int r2 = (c2 >> 16) & 0xFF, g2 = (c2 >> 8) & 0xFF, b2 = c2 & 0xFF;
    int r = r1 + (((r2 - r1) * t256) >> 8);
    int g = g1 + (((g2 - g1) * t256) >> 8);
    int b = b1 + (((b2 - b1) * t256) >> 8);
    return COLOR_RGB(r, g, b);
}

static inline uint32_t shade_color(uint32_t c, float factor) {
    int r = (int)(((c >> 16) & 0xFF) * factor);
    int g = (int)(((c >> 8) & 0xFF) * factor);
    int b = (int)((c & 0xFF) * factor);
    if (r > 255) r = 255; if (r < 0) r = 0;
    if (g > 255) g = 255; if (g < 0) g = 0;
    if (b > 255) b = 255; if (b < 0) b = 0;
    return COLOR_RGB(r, g, b);
}

/* Alpha blend pixel over target */
static inline uint32_t alpha_blend_pixel(uint32_t dst, uint32_t src, uint8_t alpha) {
    int r_d = (dst >> 16) & 0xFF, g_d = (dst >> 8) & 0xFF, b_d = dst & 0xFF;
    int r_s = (src >> 16) & 0xFF, g_s = (src >> 8) & 0xFF, b_s = src & 0xFF;
    int a = alpha;
    int r = (r_s * a + r_d * (255 - a)) / 255;
    int g = (g_s * a + g_d * (255 - a)) / 255;
    int b = (b_s * a + b_d * (255 - a)) / 255;
    return COLOR_RGB(r, g, b);
}

/* Draw a soft elliptical drop shadow on the terrain */
static void draw_drop_shadow(RenderContext *rc, int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint8_t max_alpha) {
    int x0 = cx - rx;
    int x1 = cx + rx;
    int y0 = cy - ry;
    int y1 = cy + ry;
    if (x0 < 0) x0 = 0; if (x1 >= SCREEN_W) x1 = SCREEN_W - 1;
    if (y0 < 0) y0 = 0; if (y1 >= SCREEN_H) y1 = SCREEN_H - 1;
    
    int rx2 = rx * rx;
    int ry2 = ry * ry;
    if (rx2 <= 0 || ry2 <= 0) return;
    
    for (int y = y0; y <= y1; ++y) {
        int dy = y - cy;
        int dy2 = dy * dy * rx2;
        for (int x = x0; x <= x1; ++x) {
            int dx = x - cx;
            int dist2 = dx * dx * ry2 + dy2;
            if (dist2 <= rx2 * ry2) {
                int factor = 255 - (dist2 * 255) / (rx2 * ry2);
                int a = (max_alpha * factor) / 255;
                if (a > 0) {
                    int idx = y * SCREEN_W + x;
                    rc->framebuffer[idx] = alpha_blend_pixel(rc->framebuffer[idx], COLOR_RGB(12, 18, 14), (uint8_t)a);
                }
            }
        }
    }
}

/* Procedural Texture Generation for Duffer-Pro (Mid-2000s Quality) */
static void generate_procedural_graphics_themed(CourseTheme theme) {
    s_tex_rand = 54321;
    
    /* 1. Rough Grass with Multi-Tone Tufting */
    for (int v = 0; v < 4; ++v) {
        for (int py = 0; py < TILE_H; ++py) {
            for (int px = 0; px < TILE_W; ++px) {
                int idx = py * TILE_W + px;
                if (!is_in_diamond(px, py, NULL)) {
                    s_tile_rough[v][idx] = 0;
                    continue;
                }
                int noise = (trand() % 14) - 7;
                int tuft = ((px * 7 + py * 13 + v * 19) % 11 == 0) ? 14 : 0;
                int r, g, b;
                if (theme == THEME_LA_PLAYA) {
                    /* Desert Beige */
                    r = 230 + noise - tuft / 3;
                    g = 215 + noise * 2 / 3;
                    b = 178 + noise / 2;
                } else if (theme == THEME_NOCTURNAL_GREEN) {
                    /* Transylvania Purple-Grey */
                    r = 68 + noise;
                    g = 58 + noise / 2;
                    b = 82 + noise;
                } else {
                    /* Bushwood Green */
                    r = 52 + noise - (tuft / 2);
                    g = 108 + noise * 2 + tuft;
                    b = 38 + noise / 2;
                }
                if (r < 0) r = 0; if (r > 255) r = 255;
                if (g < 0) g = 0; if (g > 255) g = 255;
                if (b < 0) b = 0; if (b > 255) b = 255;
                s_tile_rough[v][idx] = COLOR_RGB(r, g, b);
            }
        }
    }
    
    /* 2. Championship Fairway with Alternating Mower Stripes & Velvet Sheen */
    for (int m = 0; m < 16; ++m) {
        for (int py = 0; py < TILE_H; ++py) {
            for (int px = 0; px < TILE_W; ++px) {
                int idx = py * TILE_W + px;
                int dist_edge = 0;
                if (!is_in_diamond(px, py, &dist_edge)) {
                    s_tile_fairway[m][idx] = 0;
                    continue;
                }
                int stripe = ((px + py * 2) / 6) & 1;
                int noise = (trand() % 8) - 4;
                int r, g, b;
                if (theme == THEME_LA_PLAYA) {
                    /* Darker beige than background desert rough */
                    r = stripe ? (198 + noise) : (180 + noise);
                    g = stripe ? (174 + noise) : (158 + noise);
                    b = stripe ? (132 + noise) : (120 + noise);
                } else if (theme == THEME_NOCTURNAL_GREEN) {
                    /* Deep twilight violet grass */
                    r = stripe ? (92 + noise) : (78 + noise);
                    g = stripe ? (72 + noise) : (62 + noise);
                    b = stripe ? (128 + noise) : (112 + noise);
                } else {
                    /* Bushwood Emerald */
                    r = stripe ? (102 + noise) : (84 + noise);
                    g = stripe ? (186 + noise * 2) : (158 + noise * 2);
                    b = stripe ? (46 + noise) : (38 + noise);
                }
                if (r < 0) r = 0; if (r > 255) r = 255;
                if (g < 0) g = 0; if (g > 255) g = 255;
                if (b < 0) b = 0; if (b > 255) b = 255;
                uint32_t fw_col = COLOR_RGB(r, g, b);
                
                if (dist_edge < 6) {
                    bool edge_rough = false;
                    if (py < 16 && px < 32 && !(m & MASK_NW)) edge_rough = true;
                    if (py < 16 && px >= 32 && !(m & MASK_NE)) edge_rough = true;
                    if (py >= 16 && px >= 32 && !(m & MASK_SE)) edge_rough = true;
                    if (py >= 16 && px < 32 && !(m & MASK_SW)) edge_rough = true;
                    if (edge_rough) {
                        int t256 = (dist_edge * 256) / 6;
                        fw_col = blend_color(s_tile_rough[0][idx], fw_col, t256);
                    }
                }
                s_tile_fairway[m][idx] = fw_col;
            }
        }
    }
    
    /* 3. Putting Green (Smooth Bentgrass) */
    for (int m = 0; m < 16; ++m) {
        for (int py = 0; py < TILE_H; ++py) {
            for (int px = 0; px < TILE_W; ++px) {
                int idx = py * TILE_W + px;
                int dist_edge = 0;
                if (!is_in_diamond(px, py, &dist_edge)) {
                    s_tile_green[m][idx] = 0;
                    continue;
                }
                int grain = ((px * 3 + py * 7) & 3) - 1;
                uint32_t gr_col;
                if (theme == THEME_NOCTURNAL_GREEN) {
                    gr_col = COLOR_RGB(48 + grain, 148 + grain * 2, 92 + grain);
                } else {
                    /* Green and tees are green on Bushwood and La Playa */
                    gr_col = COLOR_RGB(110 + grain, 216 + grain * 2, 66 + grain);
                }
                if (dist_edge < 6) {
                    bool edge_rough = false;
                    if (py < 16 && px < 32 && !(m & MASK_NW)) edge_rough = true;
                    if (py < 16 && px >= 32 && !(m & MASK_NE)) edge_rough = true;
                    if (py >= 16 && px >= 32 && !(m & MASK_SE)) edge_rough = true;
                    if (py >= 16 && px < 32 && !(m & MASK_SW)) edge_rough = true;
                    if (edge_rough) {
                        int t256 = (dist_edge * 256) / 6;
                        gr_col = blend_color(s_tile_rough[1][idx], gr_col, t256);
                    }
                }
                s_tile_green[m][idx] = gr_col;
            }
        }
    }
    
    /* 4. Sand Bunkers */
    for (int m = 0; m < 16; ++m) {
        for (int py = 0; py < TILE_H; ++py) {
            for (int px = 0; px < TILE_W; ++px) {
                int idx = py * TILE_W + px;
                int dist_edge = 0;
                if (!is_in_diamond(px, py, &dist_edge)) {
                    s_tile_sand[m][idx] = 0;
                    continue;
                }
                int wave = (int)(sinf((float)px * 0.45f) * 4.0f);
                int noise = (trand() % 8) - 4;
                uint32_t sand_col;
                if (theme == THEME_LA_PLAYA) {
                    sand_col = COLOR_RGB(246 + noise, 240 + noise, 222 + wave / 2);
                } else if (theme == THEME_NOCTURNAL_GREEN) {
                    sand_col = COLOR_RGB(125 + noise, 118 + noise, 134 + wave / 2);
                } else {
                    sand_col = COLOR_RGB(232 + wave + noise, 214 + wave + noise, 152 + wave / 2);
                }
                if (dist_edge < 5) {
                    int t256 = (dist_edge * 256) / 5;
                    sand_col = blend_color(COLOR_RGB(110, 95, 60), sand_col, t256);
                }
                s_tile_sand[m][idx] = sand_col;
            }
        }
    }
    
    /* 5. Animated Water */
    for (int v = 0; v < 4; ++v) {
        for (int py = 0; py < TILE_H; ++py) {
            for (int px = 0; px < TILE_W; ++px) {
                int idx = py * TILE_W + px;
                int dist_edge = 0;
                if (!is_in_diamond(px, py, &dist_edge)) {
                    s_tile_water[v][idx] = 0;
                    continue;
                }
                float wave = sinf((float)px * 0.22f + (float)v * 1.57f) * 6.0f +
                             cosf((float)py * 0.35f - (float)v * 1.57f) * 4.0f;
                int r, g, b;
                if (theme == THEME_LA_PLAYA) {
                    /* Creek with foaming rapids! */
                    int creek_mid = 16 + (int)(sinf((float)px * 0.25f + (float)v) * 3.0f);
                    int d_mid = abs(py - creek_mid);
                    if (d_mid <= 3) {
                        r = 240; g = 248; b = 255; /* White foam rapids */
                    } else if (d_mid <= 6) {
                        r = 180 + (int)wave * 2; g = 225 + (int)wave; b = 245;
                    } else {
                        r = 48 + (int)wave; g = 145 + (int)(wave * 1.5f); b = 175 + (int)(wave * 2.0f);
                    }
                } else if (theme == THEME_NOCTURNAL_GREEN) {
                    /* Very dark abyssal blue, still blue */
                    r = 14 + (int)(wave * 0.5f);
                    g = 26 + (int)(wave * 0.8f);
                    b = 64 + (int)(wave * 1.2f);
                    if (dist_edge < 4) { r += 6; g += 10; b += 18; }
                } else {
                    /* Bushwood Sapphire Blue */
                    r = 36 + (int)wave;
                    g = 120 + (int)(wave * 1.5f);
                    b = 196 + (int)(wave * 2.0f);
                    if (dist_edge < 6) {
                        r += (6 - dist_edge) * 6;
                        g += (6 - dist_edge) * 10;
                        b += (6 - dist_edge) * 4;
                    }
                    if ((px * 13 + py * 17 + v * 31) % 43 == 0) {
                        r = 230; g = 245; b = 255;
                    }
                }
                if (r > 255) r = 255; if (r < 0) r = 0;
                if (g > 255) g = 255; if (g < 0) g = 0;
                if (b > 255) b = 255; if (b < 0) b = 0;
                s_tile_water[v][idx] = COLOR_RGB(r, g, b);
            }
        }
    }
    
    /* 6. Cart Path */
    for (int m = 0; m < 16; ++m) {
        for (int py = 0; py < TILE_H; ++py) {
            for (int px = 0; px < TILE_W; ++px) {
                int idx = py * TILE_W + px;
                int dist_edge = 0;
                if (!is_in_diamond(px, py, &dist_edge)) {
                    s_tile_cartpath[m][idx] = 0;
                    continue;
                }
                int noise = (trand() % 10) - 5;
                int r, g, b;
                if (theme == THEME_LA_PLAYA) {
                    r = 205 + noise; g = 188 + noise; b = 155 + noise;
                } else if (theme == THEME_NOCTURNAL_GREEN) {
                    r = 75 + noise; g = 70 + noise; b = 80 + noise;
                } else {
                    r = 165 + noise; g = 162 + noise; b = 158 + noise;
                }
                uint32_t col = COLOR_RGB(r, g, b);
                if (dist_edge < 4) {
                    col = blend_color(COLOR_RGB(110, 105, 95), col, (dist_edge * 256) / 4);
                }
                s_tile_cartpath[m][idx] = col;
            }
        }
    }
    
    /* 7. Wooden Bridge */
    for (int v = 0; v < 4; ++v) {
        for (int py = 0; py < TILE_H; ++py) {
            for (int px = 0; px < TILE_W; ++px) {
                int idx = py * TILE_W + px;
                if (!is_in_diamond(px, py, NULL)) {
                    s_tile_bridge[v][idx] = 0;
                    continue;
                }
                int plank_idx = (py / 3) % 2;
                int r = plank_idx ? 142 : 128;
                int g = plank_idx ? 94 : 84;
                int b = plank_idx ? 56 : 48;
                if ((py % 3) == 0) { r -= 35; g -= 28; b -= 20; }
                if (px <= 4 || px >= TILE_W - 5) {
                    r += 30; g += 20; b += 14;
                    if ((py % 6) == 2 && (px == 2 || px == TILE_W - 3)) {
                        r = 230; g = 195; b = 50;
                    }
                }
                s_tile_bridge[v][idx] = COLOR_RGB(r, g, b);
            }
        }
    }
    
    /* 8. Tee Box */
    for (int py = 0; py < TILE_H; ++py) {
        for (int px = 0; px < TILE_W; ++px) {
            int idx = py * TILE_W + px;
            if (!is_in_diamond(px, py, NULL)) {
                s_tile_tee[idx] = 0;
                continue;
            }
            int dither = ((px ^ py) & 1) ? 3 : -3;
            if (theme == THEME_NOCTURNAL_GREEN) {
                s_tile_tee[idx] = COLOR_RGB(52 + dither, 120 + dither, 88);
            } else {
                s_tile_tee[idx] = COLOR_RGB(94 + dither, 172 + dither, 46);
            }
        }
    }
    
    /* 9. Stratified Cliff Skirts */
    for (int y = 0; y < 72; ++y) {
        for (int x = 0; x < 64; ++x) {
            int idx = y * 64 + x;
            int layer = (y / 6) % 4;
            int r_sw, g_sw, b_sw;
            if (theme == THEME_LA_PLAYA) {
                if (layer == 0) { r_sw = 195; g_sw = 125; b_sw = 82; }
                else if (layer == 1) { r_sw = 175; g_sw = 105; b_sw = 68; }
                else if (layer == 2) { r_sw = 210; g_sw = 140; b_sw = 95; }
                else { r_sw = 155; g_sw = 90; b_sw = 56; }
            } else if (theme == THEME_NOCTURNAL_GREEN) {
                if (layer == 0) { r_sw = 52; g_sw = 46; b_sw = 60; }
                else if (layer == 1) { r_sw = 42; g_sw = 36; b_sw = 50; }
                else if (layer == 2) { r_sw = 58; g_sw = 52; b_sw = 66; }
                else { r_sw = 36; g_sw = 32; b_sw = 44; }
            } else {
                if (layer == 0) { r_sw = 112; g_sw = 104; b_sw = 88; }
                else if (layer == 1) { r_sw = 90; g_sw = 98; b_sw = 110; }
                else if (layer == 2) { r_sw = 126; g_sw = 120; b_sw = 100; }
                else { r_sw = 75; g_sw = 78; b_sw = 82; }
            }
            int noise = (trand() % 12) - 6;
            r_sw += noise; g_sw += noise; b_sw += noise;
            if ((x * 19 + y * 7) % 29 == 0) {
                r_sw -= 25; g_sw -= 25; b_sw -= 25;
            }
            if (y < 4) { r_sw -= 25; g_sw += 28; b_sw -= 20; }
            s_cliff_sw[idx] = COLOR_RGB(r_sw, g_sw, b_sw);
            s_cliff_se[idx] = COLOR_RGB((r_sw * 66) / 100, (g_sw * 66) / 100, (b_sw * 72) / 100);
        }
    }
}

static void generate_procedural_graphics(void) {
    generate_procedural_graphics_themed(THEME_BUSHWOOD);
    
    /* 10. Scots Pine Sprite (44x64, Volumetric Layered Boughs) */
    memset(s_sprite_pine, 0, sizeof(s_sprite_pine));
    for (int y = 0; y < SPRITE_PINE_H; ++y) {
        for (int x = 0; x < SPRITE_PINE_W; ++x) {
            int idx = y * SPRITE_PINE_W + x;
            int cx = SPRITE_PINE_W / 2;
            /* Trunk */
            if (y >= 44 && abs(x - cx) <= 2) {
                int bark = ((x ^ y) & 1) ? 10 : -10;
                s_sprite_pine[idx] = COLOR_RGB(96 + bark, 66 + bark, 44);
                continue;
            }
            /* 4 Tiers of Pyramidal Pine Needles */
            for (int t = 0; t < 4; ++t) {
                int ty = 4 + t * 11;
                int th = 18;
                if (y >= ty && y < ty + th) {
                    int rel_y = y - ty;
                    int max_w = 4 + rel_y * (t + 1);
                    if (max_w > 19) max_w = 19;
                    if (abs(x - cx) <= max_w) {
                        int highlight = (rel_y < 4 && x < cx) ? 25 : ((rel_y > 10) ? -25 : 0);
                        int r = 32 + highlight;
                        int g = 86 + highlight * 2;
                        int b = 38 + highlight / 2;
                        s_sprite_pine[idx] = COLOR_RGB(r, g, b);
                    }
                }
            }
        }
    }
    
    /* 11. English Oak Sprite (52x60, Lush Deciduous Canopy) */
    memset(s_sprite_oak, 0, sizeof(s_sprite_oak));
    for (int y = 0; y < SPRITE_OAK_H; ++y) {
        for (int x = 0; x < SPRITE_OAK_W; ++x) {
            int idx = y * SPRITE_OAK_W + x;
            int cx = SPRITE_OAK_W / 2;
            /* Trunk and Inner Branches */
            if (y >= 40 && abs(x - cx) <= 3) {
                int bark = (x % 2 == 0) ? 8 : -8;
                s_sprite_oak[idx] = COLOR_RGB(86 + bark, 56 + bark, 34);
                continue;
            }
            /* Volumetric Leaf Clusters (Circle centers) */
            int clusters[5][3] = {
                { cx, 16, 15 },
                { cx - 12, 24, 13 },
                { cx + 12, 23, 14 },
                { cx - 6, 32, 14 },
                { cx + 7, 33, 14 }
            };
            for (int c = 0; c < 5; ++c) {
                int dx = x - clusters[c][0];
                int dy = y - clusters[c][1];
                int rad = clusters[c][2];
                if (dx * dx + dy * dy <= rad * rad) {
                    int light = (dy < -2 && dx < 2) ? 28 : ((dy > 5) ? -22 : 0);
                    int noise = (trand() % 10) - 5;
                    int r = 58 + light + noise;
                    int g = 132 + light * 2 + noise;
                    int b = 44 + light / 2;
                    s_sprite_oak[idx] = COLOR_RGB(r, g, b);
                }
            }
        }
    }
    
    /* 12. Royal Palm Sprite (46x62, Arched Fronds & Coconuts) */
    memset(s_sprite_palm, 0, sizeof(s_sprite_palm));
    for (int y = 0; y < SPRITE_PALM_H; ++y) {
        for (int x = 0; x < SPRITE_PALM_W; ++x) {
            int idx = y * SPRITE_PALM_H + x; /* safeguard */
            idx = y * SPRITE_PALM_W + x;
            int cx = 22;
            /* Curved Ringed Trunk */
            int curve_x = cx + (int)(sinf((float)y * 0.08f) * 4.0f);
            if (y >= 20 && abs(x - curve_x) <= 2) {
                int ring = (y % 4 == 0) ? -16 : 8;
                s_sprite_palm[idx] = COLOR_RGB(158 + ring, 136 + ring, 98 + ring);
                continue;
            }
            /* Coconuts */
            if (y >= 18 && y <= 21 && abs(x - curve_x) <= 4) {
                s_sprite_palm[idx] = COLOR_RGB(92, 60, 32);
                continue;
            }
            /* Arched Palm Fronds */
            for (int a = 0; a < 6; ++a) {
                float ang = (float)a * 1.047f - 2.6f;
                for (int l = 4; l < 22; ++l) {
                    int fx = curve_x + (int)(cosf(ang) * l);
                    int fy = 18 + (int)(sinf(ang) * l * 0.7f + (l * l) * 0.025f);
                    if (abs(x - fx) <= 1 && abs(y - fy) <= 1) {
                        int highlight = (fy < 14) ? 25 : 0;
                        s_sprite_palm[idx] = COLOR_RGB(52 + highlight, 142 + highlight * 2, 42);
                    }
                }
            }
        }
    }
    
    /* 13. Flowering Cherry Blossom (48x58, Pink Sakura Clouds) */
    memset(s_sprite_flowering, 0, sizeof(s_sprite_flowering));
    for (int y = 0; y < SPRITE_FLOWERING_H; ++y) {
        for (int x = 0; x < SPRITE_FLOWERING_W; ++x) {
            int idx = y * SPRITE_FLOWERING_W + x;
            int cx = SPRITE_FLOWERING_W / 2;
            /* Gnarled Dark Trunk */
            if (y >= 38 && abs(x - cx) <= 2) {
                s_sprite_flowering[idx] = COLOR_RGB(68, 45, 34);
                continue;
            }
            /* Sakura Blossom Clusters */
            int clusters[4][3] = {
                { cx, 18, 14 },
                { cx - 11, 24, 12 },
                { cx + 11, 22, 13 },
                { cx, 30, 13 }
            };
            for (int c = 0; c < 4; ++c) {
                int dx = x - clusters[c][0];
                int dy = y - clusters[c][1];
                int rad = clusters[c][2];
                if (dx * dx + dy * dy <= rad * rad) {
                    int light = (dy < 0) ? 22 : -18;
                    int noise = (trand() % 10) - 5;
                    s_sprite_flowering[idx] = COLOR_RGB(246 + noise, 168 + light, 192 + light);
                }
            }
        }
    }
    
    /* 14. Azalea Bush (32x24) */
    memset(s_sprite_bush, 0, sizeof(s_sprite_bush));
    for (int y = 0; y < SPRITE_BUSH_H; ++y) {
        for (int x = 0; x < SPRITE_BUSH_W; ++x) {
            int idx = y * SPRITE_BUSH_W + x;
            int dx = x - 16;
            int dy = y - 13;
            if (dx * dx * 2 + dy * dy * 4 <= 190) {
                bool is_bloom = ((x * 11 + y * 17) % 7 == 0);
                if (is_bloom) {
                    s_sprite_bush[idx] = COLOR_RGB(235, 45, 125); /* bright magenta */
                } else {
                    int light = (dy < 0) ? 20 : -18;
                    s_sprite_bush[idx] = COLOR_RGB(42 + light, 115 + light * 2, 38);
                }
            }
        }
    }
    
    /* 15. Wildflower Bed (34x20) */
    memset(s_sprite_flowers, 0, sizeof(s_sprite_flowers));
    for (int y = 0; y < SPRITE_FLOWERS_H; ++y) {
        for (int x = 0; x < SPRITE_FLOWERS_W; ++x) {
            int idx = y * SPRITE_FLOWERS_W + x;
            int dx = x - 17;
            int dy = y - 11;
            if (dx * dx + dy * dy * 3 <= 180) {
                int ftype = (x * 7 + y * 13) % 11;
                if (ftype == 1) s_sprite_flowers[idx] = COLOR_RGB(245, 215, 45);  /* Yellow poppy */
                else if (ftype == 2) s_sprite_flowers[idx] = COLOR_RGB(165, 85, 235); /* Lavender */
                else if (ftype == 3) s_sprite_flowers[idx] = COLOR_RGB(245, 65, 65);  /* Red poppy */
                else if (ftype == 4) s_sprite_flowers[idx] = COLOR_RGB(245, 245, 250);/* Daisy */
                else s_sprite_flowers[idx] = COLOR_RGB(65, 142, 55);                /* Foliage */
            }
        }
    }
    
    /* 16. Granite Boulder (36x26, Multi-Faceted with Lichen) */
    memset(s_sprite_rock, 0, sizeof(s_sprite_rock));
    for (int y = 0; y < SPRITE_ROCK_H; ++y) {
        for (int x = 0; x < SPRITE_ROCK_W; ++x) {
            int idx = y * SPRITE_ROCK_W + x;
            int dx = x - 18;
            int dy = y - 14;
            if (dx * dx + dy * dy * 2 <= 180) {
                int facet = (dx + dy * 2 < -4) ? 35 : ((dx - dy > 4) ? -35 : 0);
                int noise = (trand() % 10) - 5;
                int r = 118 + facet + noise;
                int g = 120 + facet + noise;
                int b = 126 + facet + noise;
                if ((x * 5 + y * 9) % 13 == 0) { r -= 15; g += 18; b -= 15; } /* Lichen */
                s_sprite_rock[idx] = COLOR_RGB(r, g, b);
            }
        }
    }
    
    /* 17. Park Bench (34x22, Cedar Slats & Wrought Iron) */
    memset(s_sprite_bench, 0, sizeof(s_sprite_bench));
    for (int y = 0; y < SPRITE_BENCH_H; ++y) {
        for (int x = 0; x < SPRITE_BENCH_W; ++x) {
            int idx = y * SPRITE_BENCH_W + x;
            /* Cast Iron Legs */
            if (y >= 12 && (x == 4 || x == 10 || x == 23 || x == 29)) {
                s_sprite_bench[idx] = COLOR_RGB(28, 48, 38);
                continue;
            }
            /* Wood Slats */
            if (y >= 4 && y <= 9 && x >= 4 && x <= 29) {
                int slat = (y % 2 == 0) ? -16 : 14;
                s_sprite_bench[idx] = COLOR_RGB(168 + slat, 92 + slat, 46 + slat);
            }
            if (y >= 10 && y <= 13 && x >= 5 && x <= 28) {
                s_sprite_bench[idx] = COLOR_RGB(146, 78, 38);
            }
        }
    }
    
    /* 18. Golf Cart (44x36, Modern Club Car with Canopy & Bags) */
    memset(s_sprite_cart, 0, sizeof(s_sprite_cart));
    for (int y = 0; y < SPRITE_CART_H; ++y) {
        for (int x = 0; x < SPRITE_CART_W; ++x) {
            int idx = y * SPRITE_CART_H + x; /* safeguard */
            idx = y * SPRITE_CART_W + x;
            /* Wheels */
            if (y >= 26 && ((x >= 6 && x <= 12) || (x >= 28 && x <= 34))) {
                s_sprite_cart[idx] = (y == 29 && (x == 9 || x == 31)) ? COLOR_RGB(180, 185, 190) : COLOR_RGB(30, 32, 35);
                continue;
            }
            /* Body (Glossy White) */
            if (y >= 18 && y <= 27 && x >= 4 && x <= 36) {
                int shade = (y > 23) ? -20 : 15;
                s_sprite_cart[idx] = COLOR_RGB(232 + shade, 235 + shade, 240 + shade);
                if (x == 5 && y >= 20 && y <= 22) s_sprite_cart[idx] = COLOR_RGB(255, 235, 140); /* Headlight */
            }
            /* Roll Struts */
            if (y >= 6 && y <= 18 && (x == 8 || x == 24 || x == 32)) {
                s_sprite_cart[idx] = COLOR_RGB(45, 50, 55);
            }
            /* Roof Canopy */
            if (y >= 3 && y <= 6 && x >= 6 && x <= 34) {
                s_sprite_cart[idx] = COLOR_RGB(215, 210, 195);
            }
            /* Windshield with Glare */
            if (y >= 8 && y <= 17 && x >= 8 && x <= 16) {
                s_sprite_cart[idx] = (x + y == 20) ? COLOR_RGB(245, 255, 255) : COLOR_RGB(145, 185, 215);
            }
            /* Dual Golf Bags in rear rack (x = 33..42, y = 14..26) */
            if (y >= 14 && y <= 25 && x >= 33 && x <= 41) {
                s_sprite_cart[idx] = (x < 37) ? COLOR_RGB(185, 42, 42) : COLOR_RGB(35, 75, 145);
            }
            if (y >= 11 && y <= 13 && (x == 35 || x == 39)) {
                s_sprite_cart[idx] = COLOR_RGB(215, 220, 225); /* Irons */
            }
        }
    }
    
    /* 19. Animated Flagstick & Red Pennant */
    for (int f = 0; f < 4; ++f) {
        memset(s_sprite_flag[f], 0, sizeof(s_sprite_flag[0]));
        for (int y = 0; y < SPRITE_FLAG_H; ++y) {
            for (int x = 0; x < SPRITE_FLAG_W; ++x) {
                int idx = y * SPRITE_FLAG_W + x;
                /* Chrome Pole */
                if (x == 4 && y >= 2) s_sprite_flag[f][idx] = COLOR_RGB(225, 230, 235);
                /* Pennant */
                if (x >= 5 && x <= 17 && y >= 2 && y <= 14) {
                    int wave = (int)(sinf((float)x * 0.6f + (float)f * 1.57f) * 1.5f);
                    if (y >= 3 + wave && y <= 13 + wave) {
                        s_sprite_flag[f][idx] = COLOR_RGB(225, 45, 45);
                    }
                }
            }
        }
    }
    
    /* 20. Golfer Character (28x38, Athletic Polo, Visor, Glove & Club in Hand) */
    memset(s_sprite_golfer, 0, sizeof(s_sprite_golfer));
    for (int y = 0; y < SPRITE_GOLFER_H; ++y) {
        for (int x = 0; x < SPRITE_GOLFER_W; ++x) {
            int idx = y * SPRITE_GOLFER_W + x;
            int cx = 13;
            /* White Tour Visor / Cap */
            if (y >= 2 && y <= 5 && abs(x - cx) <= 4) {
                s_sprite_golfer[idx] = (y == 5 && x > cx) ? COLOR_RGB(45, 55, 65) : COLOR_RGB(245, 245, 248);
            }
            /* Face & Sunglasses */
            if (y >= 6 && y <= 9 && abs(x - cx) <= 3) {
                s_sprite_golfer[idx] = (y == 7 && x >= cx - 1 && x <= cx + 2) ? COLOR_RGB(35, 40, 48) : COLOR_RGB(238, 192, 155);
            }
            /* Red Athletic Polo Shirt with Collar */
            if (y >= 10 && y <= 18 && abs(x - cx) <= 5) {
                int fold = (y % 3 == 0) ? -15 : 12;
                s_sprite_golfer[idx] = COLOR_RGB(218 + fold, 42 + fold / 2, 42);
                if (y == 10 && abs(x - cx) <= 2) s_sprite_golfer[idx] = COLOR_RGB(245, 245, 245); /* Collar */
            }
            /* Left White Glove, Right Arm */
            if (y >= 12 && y <= 18) {
                if (x == cx - 6) s_sprite_golfer[idx] = COLOR_RGB(238, 192, 155);
                if (x == cx + 6) s_sprite_golfer[idx] = COLOR_RGB(245, 245, 245); /* White Glove */
            }
            /* Khaki Shorts & Leather Belt */
            if (y >= 19 && y <= 24 && abs(x - cx) <= 4) {
                if (y == 19) {
                    s_sprite_golfer[idx] = (x == cx) ? COLOR_RGB(230, 200, 50) : COLOR_RGB(82, 50, 28); /* Belt */
                } else {
                    int pleat = (x == cx - 2 || x == cx + 2) ? -16 : 8;
                    s_sprite_golfer[idx] = COLOR_RGB(196 + pleat, 170 + pleat, 132 + pleat);
                }
            }
            /* Athletic Calves */
            if (y >= 25 && y <= 31 && (abs(x - (cx - 2)) <= 1 || abs(x - (cx + 2)) <= 1)) {
                s_sprite_golfer[idx] = COLOR_RGB(238, 192, 155);
            }
            /* White Saddle Golf Shoes */
            if (y >= 32 && y <= 35 && (abs(x - (cx - 2)) <= 2 || abs(x - (cx + 2)) <= 2)) {
                s_sprite_golfer[idx] = (x == cx - 2 || x == cx + 2) ? COLOR_RGB(45, 45, 50) : COLOR_RGB(242, 244, 248);
            }
            /* Golf Club Resting on Turf (Chrome Shaft & Titanium Head) */
            if (x == cx + 8 && y >= 16 && y <= 34) {
                s_sprite_golfer[idx] = COLOR_RGB(215, 222, 230);
            }
            if (y >= 33 && y <= 35 && x >= cx + 7 && x <= cx + 10) {
                s_sprite_golfer[idx] = COLOR_RGB(35, 40, 48); /* Driver Head */
            }
        }
    }
    
    /* 6 Smooth Walking Animation Frames for Golfer */
    for (int f = 0; f < 6; ++f) {
        memcpy(s_sprite_golfer_walk[f], s_sprite_golfer, sizeof(s_sprite_golfer));
        /* Leg strides offset */
        int offsets[6] = { 3, 1, -1, -3, -1, 1 };
        int off = offsets[f];
        int bob = (f == 1 || f == 4) ? 1 : 0;
        
        for (int y = 25; y < SPRITE_GOLFER_H; ++y) {
            for (int x = 0; x < SPRITE_GOLFER_W; ++x) {
                s_sprite_golfer_walk[f][y * SPRITE_GOLFER_W + x] = 0;
            }
        }
        int cx = 13;
        for (int y = 25; y <= 31; ++y) {
            int x1 = cx - 2 - off, x2 = cx + 2 + off;
            if (x1 >= 0 && x1 < SPRITE_GOLFER_W) s_sprite_golfer_walk[f][(y + bob) * SPRITE_GOLFER_W + x1] = COLOR_RGB(238, 192, 155);
            if (x2 >= 0 && x2 < SPRITE_GOLFER_W) s_sprite_golfer_walk[f][(y + bob) * SPRITE_GOLFER_W + x2] = COLOR_RGB(238, 192, 155);
        }
        for (int y = 32; y <= 35; ++y) {
            int x1 = cx - 2 - off, x2 = cx + 2 + off;
            for (int dx = -1; dx <= 1; ++dx) {
                if (x1 + dx >= 0 && x1 + dx < SPRITE_GOLFER_W) s_sprite_golfer_walk[f][(y + bob) * SPRITE_GOLFER_W + x1 + dx] = COLOR_RGB(242, 244, 248);
                if (x2 + dx >= 0 && x2 + dx < SPRITE_GOLFER_W) s_sprite_golfer_walk[f][(y + bob) * SPRITE_GOLFER_W + x2 + dx] = COLOR_RGB(242, 244, 248);
            }
        }
    }
    
    /* Jumping Golfer Sprite (Tucked knees, arms poised for athletic leap) */
    memcpy(s_sprite_golfer_jump, s_sprite_golfer, sizeof(s_sprite_golfer));
    for (int y = 20; y < SPRITE_GOLFER_H; ++y) {
        for (int x = 0; x < SPRITE_GOLFER_W; ++x) {
            s_sprite_golfer_jump[y * SPRITE_GOLFER_W + x] = 0;
        }
    }
    {
        int cx = 13;
        /* Athletic jumping legs: bent knees tucked up */
        for (int y = 19; y <= 24; ++y) {
            for (int x = cx - 5; x <= cx + 5; ++x) {
                int pleat = (x == cx - 2 || x == cx + 2) ? -16 : 8;
                s_sprite_golfer_jump[y * SPRITE_GOLFER_W + x] = COLOR_RGB(196 + pleat, 170 + pleat, 132 + pleat);
            }
        }
        /* Tucked athletic calves */
        for (int y = 24; y <= 28; ++y) {
            if (cx - 4 >= 0) s_sprite_golfer_jump[y * SPRITE_GOLFER_W + (cx - 4)] = COLOR_RGB(238, 192, 155);
            if (cx - 3 >= 0) s_sprite_golfer_jump[y * SPRITE_GOLFER_W + (cx - 3)] = COLOR_RGB(238, 192, 155);
            if (cx + 3 < SPRITE_GOLFER_W) s_sprite_golfer_jump[y * SPRITE_GOLFER_W + (cx + 3)] = COLOR_RGB(238, 192, 155);
            if (cx + 4 < SPRITE_GOLFER_W) s_sprite_golfer_jump[y * SPRITE_GOLFER_W + (cx + 4)] = COLOR_RGB(238, 192, 155);
        }
        /* Tucked saddle shoes */
        for (int y = 28; y <= 31; ++y) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (cx - 3 + dx >= 0) s_sprite_golfer_jump[y * SPRITE_GOLFER_W + (cx - 3 + dx)] = COLOR_RGB(242, 244, 248);
                if (cx + 3 + dx < SPRITE_GOLFER_W) s_sprite_golfer_jump[y * SPRITE_GOLFER_W + (cx + 3 + dx)] = COLOR_RGB(242, 244, 248);
            }
        }
        /* Dynamic leaping arms raised up high for balance */
        for (int y = 9; y <= 15; ++y) {
            if (cx - 6 >= 0) s_sprite_golfer_jump[y * SPRITE_GOLFER_W + (cx - 6)] = COLOR_RGB(238, 192, 155);
            if (cx - 7 >= 0) s_sprite_golfer_jump[y * SPRITE_GOLFER_W + (cx - 7)] = COLOR_RGB(238, 192, 155);
            if (cx + 6 < SPRITE_GOLFER_W) s_sprite_golfer_jump[y * SPRITE_GOLFER_W + (cx + 6)] = COLOR_RGB(245, 245, 245);
            if (cx + 7 < SPRITE_GOLFER_W) s_sprite_golfer_jump[y * SPRITE_GOLFER_W + (cx + 7)] = COLOR_RGB(245, 245, 245);
        }
    }

    /* Drinking Golfer Sprite (Tipping head back, raising frosty cold drink can to mouth) */
    memcpy(s_sprite_golfer_drink, s_sprite_golfer, sizeof(s_sprite_golfer));
    {
        int cx = 13;
        /* Frosty cold drink can in right hand up by mouth (x = cx + 4..cx + 7, y = 6..12) */
        for (int y = 6; y <= 12; ++y) {
            for (int x = cx + 4; x <= cx + 7; ++x) {
                if (x < SPRITE_GOLFER_W) {
                    uint32_t can_col = (x == cx + 5) ? COLOR_RGB(220, 240, 255) : COLOR_RGB(40, 110, 220);
                    if (y == 6 || y == 12) can_col = COLOR_RGB(200, 210, 225);
                    s_sprite_golfer_drink[y * SPRITE_GOLFER_W + x] = can_col;
                }
            }
        }
        /* Right hand gripping can */
        for (int y = 9; y <= 11; ++y) {
            if (cx + 3 < SPRITE_GOLFER_W) s_sprite_golfer_drink[y * SPRITE_GOLFER_W + (cx + 3)] = COLOR_RGB(245, 245, 245);
        }
    }
    
    /* 21. Caddy Character (28x38, Tour Bib with Gold #1, Tour Bag with Pom-Poms) */
    memset(s_sprite_caddy, 0, sizeof(s_sprite_caddy));
    for (int y = 0; y < SPRITE_CADDY_H; ++y) {
        for (int x = 0; x < SPRITE_CADDY_W; ++x) {
            int idx = y * SPRITE_CADDY_W + x;
            int cx = 11;
            /* Green PGA Visor */
            if (y >= 2 && y <= 5 && abs(x - cx) <= 3) s_sprite_caddy[idx] = COLOR_RGB(35, 115, 55);
            /* Face */
            if (y >= 6 && y <= 9 && abs(x - cx) <= 3) s_sprite_caddy[idx] = COLOR_RGB(235, 190, 150);
            /* Crisp White Caddy Bib with Gold #1 */
            if (y >= 10 && y <= 18 && abs(x - cx) <= 4) {
                s_sprite_caddy[idx] = (x == cx && y >= 12 && y <= 16) ? COLOR_RGB(235, 195, 45) : COLOR_RGB(245, 246, 250);
            }
            /* Navy Shorts */
            if (y >= 19 && y <= 24 && abs(x - cx) <= 4) s_sprite_caddy[idx] = COLOR_RGB(35, 52, 92);
            /* Legs */
            if (y >= 25 && y <= 31 && (x == cx - 2 || x == cx + 2)) s_sprite_caddy[idx] = COLOR_RGB(235, 190, 150);
            /* Shoes */
            if (y >= 32 && y <= 35 && (abs(x - (cx - 2)) <= 1 || abs(x - (cx + 2)) <= 1)) s_sprite_caddy[idx] = COLOR_RGB(55, 55, 60);
            /* Heavy Leather Tour Staff Bag over right shoulder (x = 18..26, y = 8..28) */
            if (y >= 8 && y <= 27 && x >= 18 && x <= 25) {
                int shade = (x > 22) ? -22 : 12;
                s_sprite_caddy[idx] = COLOR_RGB(146 + shade, 82 + shade, 42 + shade);
                if (y == 12 || y == 22) s_sprite_caddy[idx] = COLOR_RGB(225, 190, 55); /* Brass buckle */
            }
            /* White Tour Towel draped over bag ring */
            if (y >= 18 && y <= 24 && x >= 16 && x <= 18) s_sprite_caddy[idx] = COLOR_RGB(240, 242, 245);
            /* 3 Wood Pom-Pom Headcovers & Chrome Iron Blade protruding */
            if (y >= 5 && y <= 7) {
                if (x == 18) s_sprite_caddy[idx] = COLOR_RGB(215, 42, 42);  /* Red Pom-Pom */
                if (x == 20) s_sprite_caddy[idx] = COLOR_RGB(42, 95, 215);  /* Blue Pom-Pom */
                if (x == 22) s_sprite_caddy[idx] = COLOR_RGB(235, 205, 45); /* Yellow Pom-Pom */
                if (x == 24) s_sprite_caddy[idx] = COLOR_RGB(220, 225, 230);/* Chrome Iron */
            }
        }
    }
    for (int f = 0; f < 6; ++f) memcpy(s_sprite_caddy_walk[f], s_sprite_caddy, sizeof(s_sprite_caddy));
    
    /* Celebration Frame (Fist Pump & Raised Arms) */
    memcpy(s_sprite_caddy_celebrate, s_sprite_caddy, sizeof(s_sprite_caddy));
    for (int y = 4; y <= 11; ++y) {
        int cx = 11;
        s_sprite_caddy_celebrate[y * SPRITE_CADDY_W + (cx - 5)] = COLOR_RGB(235, 190, 150);
        s_sprite_caddy_celebrate[y * SPRITE_CADDY_W + (cx + 5)] = COLOR_RGB(235, 190, 150);
    }
    
    /* 22. Spectators (20x30, 5 Distinct Styles & Cheering/Clapping Frames) */
    uint32_t spec_shirt_colors[5] = {
        COLOR_RGB(235, 205, 55),  /* Yellow Polo */
        COLOR_RGB(45, 105, 215),  /* Royal Blue Rugby */
        COLOR_RGB(235, 95, 85),   /* Coral Polo */
        COLOR_RGB(45, 135, 65),   /* Forest Windshirt */
        COLOR_RGB(242, 244, 248)  /* Crisp White */
    };
    for (int c = 0; c < 5; ++c) {
        for (int f = 0; f < 4; ++f) {
            memset(s_sprite_spec[c][f], 0, sizeof(s_sprite_spec[0][0]));
            for (int y = 0; y < SPRITE_SPEC_H; ++y) {
                for (int x = 0; x < SPRITE_SPEC_W; ++x) {
                    int idx = y * SPRITE_SPEC_W + x;
                    int cx = 10;
                    /* Hats & Hair */
                    if (y >= 2 && y <= 5 && abs(x - cx) <= 3) {
                        s_sprite_spec[c][f][idx] = (c == 0) ? COLOR_RGB(245, 245, 240) : COLOR_RGB(85, 55, 38);
                    }
                    /* Face */
                    if (y >= 6 && y <= 9 && abs(x - cx) <= 2) s_sprite_spec[c][f][idx] = COLOR_RGB(235, 190, 150);
                    /* Colorful Shirt */
                    if (y >= 10 && y <= 17 && abs(x - cx) <= 4) s_sprite_spec[c][f][idx] = spec_shirt_colors[c];
                    /* Arms / Cheering / Clapping */
                    if ((f == 1 || f == 2) && y >= 5 && y <= 11 && (abs(x - (cx - 5)) <= 1 || abs(x - (cx + 5)) <= 1)) {
                        s_sprite_spec[c][f][idx] = COLOR_RGB(235, 190, 150); /* Raised cheering arms */
                        if (y == 5 && x == cx + 5) s_sprite_spec[c][f][idx] = COLOR_RGB(42, 125, 55); /* Masters Cup */
                    } else if (f == 3 && y >= 11 && y <= 13 && abs(x - cx) <= 2) {
                        s_sprite_spec[c][f][idx] = COLOR_RGB(235, 190, 150); /* Clapping hands */
                    }
                    /* Trousers */
                    if (y >= 18 && y <= 24 && abs(x - cx) <= 3) s_sprite_spec[c][f][idx] = COLOR_RGB(55, 65, 82);
                    /* Shoes */
                    if (y >= 25 && y <= 28 && (x == cx - 2 || x == cx + 2)) s_sprite_spec[c][f][idx] = COLOR_RGB(42, 42, 45);
                }
            }
        }
    }
    
    /* 23. Beverage Cart Girl (46x38, Refreshment Cart with Canopy, Cold Drink Cooler & Girl Driver) */
    memset(s_sprite_bev_cart, 0, sizeof(s_sprite_bev_cart));
    for (int y = 0; y < SPRITE_BEV_CART_H; ++y) {
        for (int x = 0; x < SPRITE_BEV_CART_W; ++x) {
            int idx = y * SPRITE_BEV_CART_W + x;
            
            /* Wheels with chrome hubcaps (x = 7..13 and x = 31..37, y = 28..36) */
            if (y >= 28 && ((x >= 7 && x <= 13) || (x >= 31 && x <= 37))) {
                if (y >= 30 && y <= 34 && (x == 10 || x == 34)) {
                    s_sprite_bev_cart[idx] = COLOR_RGB(220, 225, 235); /* Chrome hubcap */
                } else {
                    s_sprite_bev_cart[idx] = COLOR_RGB(28, 30, 32);     /* Rubber tire */
                }
                continue;
            }
            
            /* Front chrome bumper & headlight */
            if (y >= 22 && y <= 25 && x >= 4 && x <= 6) {
                s_sprite_bev_cart[idx] = (y == 23 && x == 5) ? COLOR_RGB(255, 240, 150) : COLOR_RGB(210, 215, 220);
                continue;
            }
            
            /* Cart Body - White chassis with vibrant pink/teal trim */
            if (y >= 19 && y <= 28 && x >= 6 && x <= 39) {
                if (y == 22 || y == 23) {
                    s_sprite_bev_cart[idx] = COLOR_RGB(245, 60, 130); /* Sporty Pink Stripe */
                } else if (y >= 24) {
                    s_sprite_bev_cart[idx] = COLOR_RGB(225, 228, 235); /* Lower body */
                } else {
                    s_sprite_bev_cart[idx] = COLOR_RGB(248, 250, 252); /* Upper glossy white */
                }
            }
            
            /* Windshield (slanted glass with reflection) */
            if (y >= 9 && y <= 18 && x >= 8 && x <= 16) {
                if (x + y == 21 || x + y == 22) {
                    s_sprite_bev_cart[idx] = COLOR_RGB(240, 255, 255); /* Sun reflection */
                } else if (x >= 10 && x <= 14) {
                    s_sprite_bev_cart[idx] = COLOR_RGB(150, 195, 225); /* Tinted glass */
                }
            }
            
            /* Steering column & wheel */
            if (y >= 16 && y <= 20 && x >= 16 && x <= 18) {
                s_sprite_bev_cart[idx] = COLOR_RGB(35, 40, 45);
            }
            
            /* Beverage Girl Driver:
               Blonde Ponytail: x = 24..27, y = 10..15
               Sun Visor: x = 18..24, y = 7..9
               Face: x = 19..23, y = 9..13
               Pink Polo Top: x = 18..24, y = 14..20
               Arms: x = 16..19, y = 16..19
            */
            /* Sun Visor */
            if (y >= 7 && y <= 9 && x >= 18 && x <= 24) {
                s_sprite_bev_cart[idx] = (y == 7) ? COLOR_RGB(250, 250, 255) : COLOR_RGB(245, 60, 130);
            }
            /* Face & Smile */
            if (y >= 10 && y <= 13 && x >= 19 && x <= 23) {
                if (y == 12 && x == 20) s_sprite_bev_cart[idx] = COLOR_RGB(220, 70, 70); /* smile */
                else if (y == 11 && x == 20) s_sprite_bev_cart[idx] = COLOR_RGB(45, 50, 60); /* eye */
                else s_sprite_bev_cart[idx] = COLOR_RGB(255, 205, 170); /* skin */
            }
            /* Golden Blonde Ponytail */
            if (y >= 9 && y <= 15 && x >= 23 && x <= 27) {
                s_sprite_bev_cart[idx] = (y >= 13) ? COLOR_RGB(225, 185, 40) : COLOR_RGB(250, 215, 65);
            }
            /* Pink Polo Shirt */
            if (y >= 14 && y <= 20 && x >= 18 && x <= 24) {
                s_sprite_bev_cart[idx] = COLOR_RGB(245, 60, 130);
            }
            /* Arms holding steering wheel */
            if (y >= 16 && y <= 19 && x >= 16 && x <= 18) {
                s_sprite_bev_cart[idx] = COLOR_RGB(255, 205, 170);
            }
            
            /* Rear Refreshment Cooler Box (x = 28..42, y = 13..27) */
            if (y >= 17 && y <= 27 && x >= 28 && x <= 42) {
                s_sprite_bev_cart[idx] = COLOR_RGB(32, 90, 180); /* Deep royal blue cooler */
                if (x == 35 && (y == 19 || y == 20)) s_sprite_bev_cart[idx] = COLOR_RGB(225, 230, 235); /* Chrome latch */
            }
            /* Sparkling Crushed Ice in open bin (y = 13..16, x = 29..41) */
            if (y >= 13 && y <= 16 && x >= 29 && x <= 41) {
                s_sprite_bev_cart[idx] = ((x + y) % 2 == 0) ? COLOR_RGB(230, 245, 255) : COLOR_RGB(195, 225, 248);
            }
            /* Colorful Cold Beverage Cans & Bottles sitting in ice (y = 10..13) */
            if (y >= 10 && y <= 13) {
                if (x >= 29 && x <= 31) s_sprite_bev_cart[idx] = COLOR_RGB(235, 35, 35);   /* Red Cola Can */
                if (x >= 32 && x <= 34) s_sprite_bev_cart[idx] = COLOR_RGB(40, 160, 245);  /* Blue Sports Drink */
                if (x >= 35 && x <= 37) s_sprite_bev_cart[idx] = COLOR_RGB(45, 210, 65);   /* Green Lemon-Lime */
                if (x >= 38 && x <= 40) s_sprite_bev_cart[idx] = (y == 10) ? COLOR_RGB(245, 210, 45) : COLOR_RGB(155, 85, 25); /* Cold Brew Bottle */
            }
            
            /* Chrome Canopy Support Struts */
            if (y >= 6 && y <= 18 && (x == 9 || x == 26 || x == 40)) {
                s_sprite_bev_cart[idx] = COLOR_RGB(190, 195, 205);
            }
            
            /* Striped Resort Canopy Roof (y = 3..6, x = 7..42) */
            if (y >= 3 && y <= 6 && x >= 7 && x <= 42) {
                bool pink_stripe = ((x / 4) % 2 == 0);
                s_sprite_bev_cart[idx] = pink_stripe ? COLOR_RGB(245, 60, 130) : COLOR_RGB(248, 250, 255);
            }
        }
    }
    
    /* 24. Gopher Hole Dirt Mound (28x18, Caddyshack Burrow Hazard) */
    memset(s_sprite_gopher_hole, 0, sizeof(s_sprite_gopher_hole));
    for (int y = 0; y < SPRITE_GOPHER_HOLE_H; ++y) {
        for (int x = 0; x < SPRITE_GOPHER_HOLE_W; ++x) {
            int idx = y * SPRITE_GOPHER_HOLE_W + x;
            int dx = x - 14;
            int dy = y - 9;
            
            /* Elliptical Dirt Mound Rim */
            if (dx * dx * 2 + dy * dy * 5 <= 13 * 13 * 2) {
                /* Inner subterranean pit opening (dark shadow) */
                if (dx * dx * 2 + dy * dy * 6 <= 6 * 6 * 2) {
                    int pit_shade = (dy < 0) ? 8 : 0;
                    s_sprite_gopher_hole[idx] = COLOR_RGB(16 + pit_shade, 10 + pit_shade, 6 + pit_shade);
                } else {
                    /* Mound excavated earth */
                    int shade = -dx * 2 - dy * 3;
                    int r = 115 + shade;
                    int g = 75 + shade;
                    int b = 42 + shade / 2;
                    /* Crumbly dirt texture */
                    if (((x * 5 + y * 11) % 7) == 0) { r += 20; g += 15; b += 8; }
                    else if (((x * 9 + y * 7) % 5) == 0) { r -= 18; g -= 14; b -= 10; }
                    if (r < 0) r = 0; if (r > 255) r = 255;
                    if (g < 0) g = 0; if (g > 255) g = 255;
                    if (b < 0) b = 0; if (b > 255) b = 255;
                    s_sprite_gopher_hole[idx] = COLOR_RGB(r, g, b);
                }
            }
        }
    }
    
    /* 25. Caddyshack Gopher (26x28, 4 Animation Frames with Buck Teeth & Dancing Paws) */
    for (int f = 0; f < 4; ++f) {
        memset(s_sprite_gopher[f], 0, sizeof(s_sprite_gopher[f]));
        for (int y = 0; y < SPRITE_GOPHER_H; ++y) {
            for (int x = 0; x < SPRITE_GOPHER_W; ++x) {
                int idx = y * SPRITE_GOPHER_W + x;
                
                int head_cx = 13;
                int head_cy = 13;
                int body_shift = 0;
                
                if (f == 0) {
                    /* Frame 0: Peeking (only top of head & ears visible above rim) */
                    if (y < 12 || y > 24) continue;
                    head_cy = 18;
                } else if (f == 1) {
                    /* Frame 1: Alert / Rising (head & paws on mound) */
                    if (y < 6 || y > 25) continue;
                    head_cy = 13;
                } else if (f == 2) {
                    /* Frame 2: Dancing Left (tilted left, paws waving left) */
                    body_shift = -2;
                    head_cx = 11;
                    head_cy = 11;
                } else if (f == 3) {
                    /* Frame 3: Dancing Right (tilted right, paws waving right) */
                    body_shift = 2;
                    head_cx = 15;
                    head_cy = 11;
                }
                
                int dx = x - head_cx;
                int dy = y - head_cy;
                
                /* Round Ears with Pink Insides (dy = -7..-3, dx = -6..-3 and +3..+6) */
                if (dy >= -7 && dy <= -3 && ((dx >= -6 && dx <= -3) || (dx >= 3 && dx <= 6))) {
                    if (dy >= -6 && dy <= -4 && ((dx >= -5 && dx <= -4) || (dx >= 4 && dx <= 5))) {
                        s_sprite_gopher[f][idx] = COLOR_RGB(245, 168, 175); /* Pink ear inside */
                    } else {
                        s_sprite_gopher[f][idx] = COLOR_RGB(145, 85, 45);   /* Fur rim */
                    }
                    continue;
                }
                
                /* Chubby Head & Cheeks */
                if (dx * dx * 2 + dy * dy * 2 <= 7 * 7 * 2 && dy <= 5) {
                    /* Beady Eyes with White Glint (dy = -1, dx = -3, +3) */
                    if (dy == -1 && (dx == -3 || dx == 3)) {
                        s_sprite_gopher[f][idx] = COLOR_RGB(20, 15, 15);
                        continue;
                    }
                    if (dy == -2 && (dx == -3 || dx == 3)) {
                        s_sprite_gopher[f][idx] = COLOR_RGB(255, 255, 255); /* Specular shine */
                        continue;
                    }
                    
                    /* Dark Brown Button Nose (dy = 1, dx = 0) */
                    if (dy == 1 && dx == 0) {
                        s_sprite_gopher[f][idx] = COLOR_RGB(45, 28, 22);
                        continue;
                    }
                    
                    /* Signature Prominent White Buck Teeth! (dy = 3..4, dx = -1..0) */
                    if ((dy == 3 || dy == 4) && (dx == -1 || dx == 1)) {
                        s_sprite_gopher[f][idx] = COLOR_RGB(255, 255, 255); /* Pure White Buck Teeth */
                        continue;
                    }
                    if (dy == 3 && dx == 0) {
                        s_sprite_gopher[f][idx] = COLOR_RGB(40, 20, 15); /* Cleft between teeth */
                        continue;
                    }
                    
                    /* Cream Muzzle / Upper Lip (dy = 1..3, dx = -3..3) */
                    if (dy >= 1 && dy <= 3 && dx >= -3 && dx <= 3) {
                        s_sprite_gopher[f][idx] = COLOR_RGB(242, 224, 188);
                        continue;
                    }
                    
                    /* Light Fur Texture */
                    int shade = (dx < 0) ? 15 : -15;
                    s_sprite_gopher[f][idx] = COLOR_RGB(180 + shade, 118 + shade, 62 + shade / 2);
                    continue;
                }
                
                /* Body & Dancing Paws (only visible when up: f >= 1) */
                if (f >= 1) {
                    /* Torso (y = 16..25) */
                    int b_dx = x - (13 + body_shift);
                    int b_dy = y - 19;
                    if (b_dx * b_dx * 2 + b_dy * b_dy <= 6 * 6 * 2 && y <= 25) {
                        /* Cream Belly */
                        if (b_dx >= -2 && b_dx <= 2 && y >= 17) {
                            s_sprite_gopher[f][idx] = COLOR_RGB(238, 220, 185);
                        } else {
                            s_sprite_gopher[f][idx] = COLOR_RGB(165, 105, 52);
                        }
                    }
                    
                    /* Dancing Paws / Waving Hands */
                    if (f == 1) {
                        /* Alert: paws resting on rim (y = 20..22, x = 7..9 and 17..19) */
                        if (y >= 20 && y <= 22 && ((x >= 7 && x <= 9) || (x >= 17 && x <= 19))) {
                            s_sprite_gopher[f][idx] = COLOR_RGB(230, 185, 140);
                        }
                    } else if (f == 2) {
                        /* Dance Left: Left paw high in air (x = 4..7, y = 7..10), right paw (x = 17..20, y = 14..17) */
                        if ((x >= 4 && x <= 7 && y >= 7 && y <= 10) ||
                            (x >= 17 && x <= 20 && y >= 14 && y <= 17)) {
                            s_sprite_gopher[f][idx] = COLOR_RGB(230, 185, 140);
                        }
                    } else if (f == 3) {
                        /* Dance Right: Right paw high in air (x = 19..22, y = 7..10), left paw (x = 6..9, y = 14..17) */
                        if ((x >= 19 && x <= 22 && y >= 7 && y <= 10) ||
                            (x >= 6 && x <= 9 && y >= 14 && y <= 17)) {
                            s_sprite_gopher[f][idx] = COLOR_RGB(230, 185, 140);
                        }
                    }
                }
            }
        }
    }
    
    /* 26. Carl Spackler Greenskeeper & TNT Detonator Box */
    /* TNT Detonator Box (16x18, Wooden blasting machine with T-bar handle) */
    for (int f = 0; f < 2; ++f) {
        memset(s_sprite_tnt_box[f], 0, sizeof(s_sprite_tnt_box[0]));
        for (int y = 0; y < SPRITE_TNT_BOX_H; ++y) {
            for (int x = 0; x < SPRITE_TNT_BOX_W; ++x) {
                int idx = y * SPRITE_TNT_BOX_W + x;
                
                /* Hardwood Plunger T-Handle (f == 0: Raised up at y = 1..2, f == 1: Slammed flush at y = 6..7) */
                int handle_y = (f == 0) ? 1 : 6;
                if (y >= handle_y && y <= handle_y + 1 && x >= 4 && x <= 11) {
                    s_sprite_tnt_box[f][idx] = (y == handle_y) ? COLOR_RGB(135, 82, 42) : COLOR_RGB(80, 48, 25);
                    continue;
                }
                /* Brass Plunger Rod (Visible only when handle is raised) */
                if (f == 0 && y >= 3 && y <= 7 && (x == 7 || x == 8)) {
                    s_sprite_tnt_box[f][idx] = (x == 7) ? COLOR_RGB(240, 210, 95) : COLOR_RGB(180, 150, 60);
                    continue;
                }
                
                /* Oak Detonator Cabinet (y = 8..17, x = 2..13) */
                if (y >= 8 && y <= 17 && x >= 2 && x <= 13) {
                    /* Heavy Cast Iron Corner Brackets */
                    if ((x <= 3 || x >= 12) || y == 8 || y == 17) {
                        s_sprite_tnt_box[f][idx] = ((x == 3 && y == 9) || (x == 12 && y == 9) ||
                                                    (x == 3 && y == 16) || (x == 12 && y == 16)) ?
                                                    COLOR_RGB(210, 215, 220) /* Silver Rivet */ : COLOR_RGB(45, 48, 52);
                    } else {
                        /* Aged Oak Slats */
                        int wood_shade = ((x + y) % 3 == 0) ? -12 : 8;
                        s_sprite_tnt_box[f][idx] = COLOR_RGB(145 + wood_shade, 92 + wood_shade, 48 + wood_shade / 2);
                        
                        /* Red "TNT" Stencil (y = 11..14, x = 5..10) */
                        if (y >= 11 && y <= 14 && x >= 5 && x <= 10) {
                            if (y == 11 || (x == 5 && y <= 14) || (x == 7 && y <= 14) || (x == 10 && y <= 14)) {
                                s_sprite_tnt_box[f][idx] = COLOR_RGB(225, 40, 40);
                            }
                        }
                    }
                }
            }
        }
    }
    
    /* Helper to draw base Carl Spackler body (Hat, Stubble Face, Utility Vest, Muddy Pants & Boots) */
    for (int pose = 0; pose < 5; ++pose) {
        int max_f = (pose == 0) ? 4 : 2; /* 0: walk(4), 1: plant(2), 2: plunge(2), 3: hose(2), 4: celebrate(2) */
        for (int f = 0; f < max_f; ++f) {
            uint32_t *target_spr = NULL;
            if (pose == 0) target_spr = s_sprite_carl_walk[f];
            else if (pose == 1) target_spr = s_sprite_carl_plant[f];
            else if (pose == 2) target_spr = s_sprite_carl_plunge[f];
            else if (pose == 3) target_spr = s_sprite_carl_hose[f];
            else target_spr = s_sprite_carl_celebrate[f];
            
            memset(target_spr, 0, SPRITE_CARL_W * SPRITE_CARL_H * sizeof(uint32_t));
            
            int cx = 15;
            int head_y_off = 0;
            if (pose == 1) head_y_off = 2; /* crouching to plant */
            else if (pose == 2 && f == 1) head_y_off = 3; /* plunging down */
            
            for (int y = 0; y < SPRITE_CARL_H; ++y) {
                for (int x = 0; x < SPRITE_CARL_W; ++x) {
                    int idx = y * SPRITE_CARL_W + x;
                    
                    /* 1. Carl's Floppy Olive Drab Bucket Hat (Sun Hat with wavy brim) */
                    int hy = y - head_y_off;
                    if (hy >= 2 && hy <= 8 && abs(x - cx) <= 6) {
                        /* Floppy downturned brim (wrinkled and cocked sideways) */
                        if (hy >= 6 && hy <= 8) {
                            if (abs(x - cx) <= 7) {
                                int brim_shade = (x > cx) ? -15 : 12;
                                target_spr[idx] = COLOR_RGB(95 + brim_shade, 110 + brim_shade, 72 + brim_shade / 2);
                            }
                        } else if (hy >= 3 && hy <= 5 && abs(x - cx) <= 5) {
                            /* Crown with indent */
                            int crown_shade = (hy == 4) ? -18 : 8;
                            target_spr[idx] = COLOR_RGB(108 + crown_shade, 122 + crown_shade, 80 + crown_shade / 2);
                        }
                        continue;
                    }
                    
                    /* 2. Carl's Face & Scruffy Stubble */
                    if (hy >= 9 && hy <= 14 && abs(x - cx) <= 4) {
                        /* Bushy furrowed eyebrows */
                        if (hy == 9 && ((x >= cx - 3 && x <= cx - 1) || (x >= cx + 1 && x <= cx + 3))) {
                            target_spr[idx] = COLOR_RGB(65, 50, 35);
                        }
                        /* Squinting eyes */
                        else if (hy == 10 && (x == cx - 2 || x == cx + 2)) {
                            target_spr[idx] = COLOR_RGB(25, 20, 18);
                        }
                        /* Bulbous sunburnt nose */
                        else if (hy == 11 && abs(x - cx) <= 1) {
                            target_spr[idx] = COLOR_RGB(225, 155, 125);
                        }
                        /* 5 O'Clock Stubble & Crooked Bill Murray Scowl/Grin */
                        else if (hy >= 12 && hy <= 14) {
                            if (hy == 13 && x >= cx - 2 && x <= cx + 2) {
                                target_spr[idx] = COLOR_RGB(80, 40, 35); /* Crooked grin */
                            } else {
                                target_spr[idx] = COLOR_RGB(165, 142, 118); /* Scruffy stubble shadow */
                            }
                        } else {
                            target_spr[idx] = COLOR_RGB(228, 185, 145); /* Sun-baked skin */
                        }
                        continue;
                    }
                    
                    /* 3. Utility Work Vest & Dirty Undershirt */
                    if (hy >= 15 && hy <= 23 && abs(x - cx) <= 5) {
                        if (abs(x - cx) <= 2) {
                            /* Grease-stained off-white t-shirt */
                            int stain = ((x * 3 + y * 7) % 5 == 0) ? -28 : 5;
                            target_spr[idx] = COLOR_RGB(202 + stain, 198 + stain, 188 + stain);
                        } else {
                            /* Olive green utility vest with pockets */
                            int vest_shade = ((hy == 18 || hy == 19) && (x == cx - 4 || x == cx + 4)) ? 30 : 0;
                            target_spr[idx] = COLOR_RGB(78 + vest_shade, 95 + vest_shade, 64 + vest_shade / 2);
                        }
                        continue;
                    }
                    
                    /* 4. Arms & Hands depending on pose */
                    if (pose == 4) {
                        /* Celebrating: Both arms raised high in victory! (Cinderella Story!) */
                        int arm_lift = (f == 1) ? 2 : 0;
                        if (y >= 4 - arm_lift && y <= 16 && (abs(x - (cx - 7)) <= 1 || abs(x - (cx + 7)) <= 1)) {
                            target_spr[idx] = (y <= 7 - arm_lift) ? COLOR_RGB(228, 185, 145) : COLOR_RGB(78, 95, 64);
                        }
                    } else if (pose == 2) {
                        /* Plunging: Hands gripping T-handle */
                        if (f == 0) {
                            /* Hands raised up poised on handle */
                            if (y >= 14 && y <= 18 && abs(x - cx) <= 3) {
                                target_spr[idx] = COLOR_RGB(228, 185, 145);
                            }
                        } else {
                            /* Slammed down with full body weight */
                            if (y >= 21 && y <= 25 && abs(x - cx) <= 3) {
                                target_spr[idx] = COLOR_RGB(228, 185, 145);
                            }
                        }
                    } else if (pose == 3) {
                        /* Hose Spray: Holding heavy brass nozzle forward */
                        if (y >= 18 && y <= 23 && x >= cx + 2 && x <= cx + 8) {
                            if (x >= cx + 6) {
                                target_spr[idx] = COLOR_RGB(225, 188, 55); /* Brass Nozzle */
                            } else {
                                target_spr[idx] = COLOR_RGB(228, 185, 145); /* Grasping Hands */
                            }
                        }
                        /* Heavy green hose trailing from behind */
                        if (y >= 24 && y <= 30 && x >= cx - 6 && x <= cx - 2) {
                            target_spr[idx] = COLOR_RGB(35, 115, 52);
                        }
                    } else {
                        /* Stealth Walk / Plant Arms at sides */
                        if (hy >= 16 && hy <= 23 && (abs(x - (cx - 6)) <= 1 || abs(x - (cx + 6)) <= 1)) {
                            target_spr[idx] = (hy >= 21) ? COLOR_RGB(228, 185, 145) : COLOR_RGB(78, 95, 64);
                        }
                    }
                    
                    /* 5. Muddy Work Pants */
                    int py_start = (pose == 1 || (pose == 2 && f == 1)) ? 22 : 24;
                    if (y >= py_start && y <= 33 && abs(x - cx) <= 4) {
                        /* Mud splatters on knees */
                        int mud = (y >= 28 && y <= 31) ? -25 : 0;
                        target_spr[idx] = COLOR_RGB(95 + mud, 85 + mud, 72 + mud / 2);
                        continue;
                    }
                    
                    /* 6. Heavy Oiled Leather Work Boots */
                    if (y >= 34 && y <= 37) {
                        bool boot_pixel = false;
                        if (pose == 0) {
                            /* Walk cycle strides */
                            if (f == 0 && ((x >= cx - 5 && x <= cx - 2 && y <= 36) || (x >= cx + 1 && x <= cx + 4))) boot_pixel = true;
                            else if (f == 1 && (abs(x - (cx - 2)) <= 1 || abs(x - (cx + 2)) <= 1)) boot_pixel = true;
                            else if (f == 2 && ((x >= cx - 4 && x <= cx - 1) || (x >= cx + 2 && x <= cx + 5 && y <= 36))) boot_pixel = true;
                            else if (abs(x - (cx - 2)) <= 1 || abs(x - (cx + 2)) <= 1) boot_pixel = true;
                        } else {
                            if (abs(x - (cx - 3)) <= 1 || abs(x - (cx + 3)) <= 1) boot_pixel = true;
                        }
                        if (boot_pixel) {
                            target_spr[idx] = (y == 37) ? COLOR_RGB(25, 22, 20) : COLOR_RGB(52, 42, 32);
                        }
                    }
                }
            }
        }
    }
}

/* Initialization */
void render_init(RenderContext *rc, uint32_t *buffer) {
    rc->framebuffer = buffer;
    rc->camera_x = 0;
    rc->camera_y = 0;
    rc->mouse_gx = 0;
    rc->mouse_gy = 0;
    rc->mouse_sx = 0;
    rc->mouse_sy = 0;
    rc->mouse_valid = false;
    rc->tick_count = 0;
    
    generate_procedural_graphics();
}

void render_free(RenderContext *rc) {
    (void)rc;
}

/* Coordinate conversions */
void grid_to_screen(int32_t gx, int32_t gy, int32_t elevation, int32_t cam_x, int32_t cam_y, int32_t *sx, int32_t *sy) {
    *sx = (gx - gy) * TILE_HALF_W + cam_x;
    *sy = (gx + gy) * TILE_HALF_H - (elevation * HEIGHT_STEP_PX) + cam_y;
}

void world_to_screen_fixed(fixed fx, fixed fy, fixed gz, int32_t cam_x, int32_t cam_y, int32_t *sx, int32_t *sy) {
    *sx = FROM_FIXED(FMUL(fx - fy, TO_FIXED(TILE_HALF_W))) + cam_x + TILE_HALF_W;
    *sy = FROM_FIXED(FMUL(fx + fy, TO_FIXED(TILE_HALF_H))) - FROM_FIXED(gz) + cam_y;
}

bool screen_to_world_fixed(const GolfState *gs, int32_t sx, int32_t sy, int32_t cam_x, int32_t cam_y, fixed *out_fx, fixed *out_fy) {
    for (int elev = MAX_ELEVATION_TIER; elev >= 0; --elev) {
        int32_t u = sx - cam_x - TILE_HALF_W;
        int32_t v = sy - cam_y + (elev * HEIGHT_STEP_PX);
        
        fixed fx = (fixed)((u + 2 * v) * 1024);
        fixed fy = (fixed)((2 * v - u) * 1024);
        
        int32_t gx = FROM_FIXED(fx);
        int32_t gy = FROM_FIXED(fy);
        
        if (gx >= 0 && gx < GRID_W && gy >= 0 && gy < GRID_H) {
            if (gs->grid[gx][gy].elevation == elev) {
                *out_fx = fx;
                *out_fy = fy;
                return true;
            }
        }
    }
    
    /* Fallback at elevation 0 */
    int32_t u = sx - cam_x - TILE_HALF_W;
    int32_t v = sy - cam_y;
    fixed fx = (fixed)((u + 2 * v) * 1024);
    fixed fy = (fixed)((2 * v - u) * 1024);
    int32_t gx = FROM_FIXED(fx);
    int32_t gy = FROM_FIXED(fy);
    if (gx >= 0 && gx < GRID_W && gy >= 0 && gy < GRID_H) {
        *out_fx = fx;
        *out_fy = fy;
        return true;
    }
    return false;
}

bool screen_to_grid(const GolfState *gs, int32_t sx, int32_t sy, int32_t cam_x, int32_t cam_y, int32_t *out_gx, int32_t *out_gy) {
    fixed fx, fy;
    if (screen_to_world_fixed(gs, sx, sy, cam_x, cam_y, &fx, &fy)) {
        *out_gx = FROM_FIXED(fx);
        *out_gy = FROM_FIXED(fy);
        return true;
    }
    return false;
}

void render_center_camera_on(RenderContext *rc, int32_t gx, int32_t gy, int32_t elevation) {
    int32_t target_sx = (gx - gy) * TILE_HALF_W;
    int32_t target_sy = (gx + gy) * TILE_HALF_H - (elevation * HEIGHT_STEP_PX);
    rc->camera_x = SCREEN_W / 2 - target_sx - TILE_HALF_W;
    rc->camera_y = SCREEN_H / 2 - target_sy - TILE_HALF_H;
}

void render_pan_camera(RenderContext *rc, int32_t dx, int32_t dy) {
    rc->camera_x += dx;
    rc->camera_y += dy;
}

/* UI Blitting Helpers */
void draw_rect(RenderContext *rc, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    for (int dy = 0; dy < h; ++dy) {
        int py = y + dy;
        if (py < 0 || py >= SCREEN_H) continue;
        for (int dx = 0; dx < w; ++dx) {
            int px = x + dx;
            if (px < 0 || px >= SCREEN_W) continue;
            rc->framebuffer[py * SCREEN_W + px] = color;
        }
    }
}

void draw_rect_alpha(RenderContext *rc, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color, uint8_t alpha) {
    for (int dy = 0; dy < h; ++dy) {
        int py = y + dy;
        if (py < 0 || py >= SCREEN_H) continue;
        for (int dx = 0; dx < w; ++dx) {
            int px = x + dx;
            if (px < 0 || px >= SCREEN_W) continue;
            int idx = py * SCREEN_W + px;
            rc->framebuffer[idx] = alpha_blend_pixel(rc->framebuffer[idx], color, alpha);
        }
    }
}

void draw_rect_border(RenderContext *rc, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    draw_rect(rc, x, y, w, 1, color);
    draw_rect(rc, x, y + h - 1, w, 1, color);
    draw_rect(rc, x, y, 1, h, color);
    draw_rect(rc, x + w - 1, y, 1, h, color);
}

void draw_text(RenderContext *rc, int32_t x, int32_t y, const char *str, uint32_t color, bool shadow) {
    if (!str) return;
    int cur_x = x;
    while (*str) {
        char ch = *str++;
        if (ch >= 32 && ch <= 126) {
            const uint8_t *glyph = g_font8x8[ch - 32];
            for (int gy = 0; gy < 8; ++gy) {
                uint8_t row = glyph[gy];
                for (int gx = 0; gx < 8; ++gx) {
                    if (row & (0x80 >> gx)) {
                        int px = cur_x + gx;
                        int py = y + gy;
                        if (shadow) {
                            int spx = px + 1;
                            int spy = py + 1;
                            if (spx >= 0 && spx < SCREEN_W && spy >= 0 && spy < SCREEN_H) {
                                rc->framebuffer[spy * SCREEN_W + spx] = COLOR_RGB(10, 15, 20);
                            }
                        }
                        if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
                            rc->framebuffer[py * SCREEN_W + px] = color;
                        }
                    }
                }
            }
        }
        cur_x += 8;
    }
}

static void draw_text_2x(RenderContext *rc, int32_t x, int32_t y, const char *str, uint32_t color, bool shadow) {
    if (!str) return;
    int cur_x = x;
    while (*str) {
        char ch = *str++;
        if (ch >= 32 && ch <= 126) {
            const uint8_t *glyph = g_font8x8[ch - 32];
            for (int gy = 0; gy < 8; ++gy) {
                uint8_t row = glyph[gy];
                for (int gx = 0; gx < 8; ++gx) {
                    if (row & (0x80 >> gx)) {
                        for (int dy = 0; dy < 2; ++dy) {
                            for (int dx = 0; dx < 2; ++dx) {
                                int px = cur_x + gx * 2 + dx;
                                int py = y + gy * 2 + dy;
                                if (shadow) {
                                    int spx = px + 2;
                                    int spy = py + 2;
                                    if (spx >= 0 && spx < SCREEN_W && spy >= 0 && spy < SCREEN_H) {
                                        rc->framebuffer[spy * SCREEN_W + spx] = COLOR_RGB(10, 15, 20);
                                    }
                                }
                                if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
                                    rc->framebuffer[py * SCREEN_W + px] = color;
                                }
                            }
                        }
                    }
                }
            }
        }
        cur_x += 16;
    }
}

/* Render Georgian Neoclassical Luxury Clubhouse (140x95) */
static void render_clubhouse(RenderContext *rc, int32_t sx, int32_t sy) {
    int b_w = 130;
    int b_h = 85;
    int base_x = sx - 32;
    int base_y = sy - 52;
    
    /* Soft Ground Drop Shadow */
    draw_drop_shadow(rc, base_x + 65, base_y + 68, 58, 24, 150);
    
    /* Main Building Blocks (Colonial clapboard & brick) */
    for (int y = 0; y < b_h; ++y) {
        for (int x = 0; x < b_w; ++x) {
            int px = base_x + x;
            int py = base_y + y;
            if (px < 0 || px >= SCREEN_W || py < 0 || py >= SCREEN_H) continue;
            
            uint32_t col = 0;
            
            /* Slate Roof Gables (Pitched hipped slate roof with dormers) */
            if (y < 34) {
                int peak_x = 65;
                if (abs(x - peak_x) <= (y + 1) * 2) {
                    int shingle = (y % 3 == 0) ? -22 : 12;
                    col = COLOR_RGB(58 + shingle, 68 + shingle, 84 + shingle);
                    /* Dormer windows in roof */
                    if (y >= 14 && y <= 26 && ((x >= 38 && x <= 46) || (x >= 84 && x <= 92))) {
                        col = (x == 42 || x == 88 || y == 20) ? COLOR_RGB(42, 46, 52) : COLOR_RGB(245, 218, 125);
                    }
                }
            }
            /* Twin Brick Chimneys with terracotta flue pots */
            if ((x >= 24 && x <= 32) || (x >= 98 && x <= 106)) {
                if (y >= 8 && y <= 40) {
                    int brick = ((x ^ y) & 2) ? 16 : -16;
                    col = COLOR_RGB(168 + brick, 64 + brick, 46);
                    if (y <= 11) col = COLOR_RGB(215, 125, 65); /* Terracotta pot */
                }
            }
            /* White Weatherboard Walls & Portico */
            if (y >= 34 && y <= 66 && x >= 18 && x <= 112) {
                int board = (y % 4 == 0) ? -18 : 8;
                col = COLOR_RGB(236 + board, 238 + board, 242 + board);
                
                /* Neoclassical Portico Entrance (x = 52..78) */
                if (x >= 52 && x <= 78) {
                    if (y >= 34 && y <= 42) col = COLOR_RGB(248, 250, 252); /* Triangular pediment */
                    if ((x >= 54 && x <= 56) || (x >= 74 && x <= 76)) col = COLOR_RGB(248, 250, 252); /* Columns */
                }
                
                /* Leaded French Windows with Warm Amber Interior Light */
                if (y >= 44 && y <= 58 && ((x >= 28 && x <= 42) || (x >= 88 && x <= 102))) {
                    col = COLOR_RGB(248, 218, 122); /* Warm Amber Glow */
                    if (x == 35 || x == 95 || y == 51) col = COLOR_RGB(45, 50, 56); /* Window Mullion */
                }
                /* Window Flower Boxes with Red Geraniums */
                if (y >= 59 && y <= 62 && ((x >= 26 && x <= 44) || (x >= 86 && x <= 104))) {
                    col = (x % 3 == 0) ? COLOR_RGB(225, 42, 42) : COLOR_RGB(52, 115, 45);
                }
                /* Grand Mahogany Entrance Door */
                if (y >= 50 && y <= 66 && x >= 59 && x <= 71) {
                    col = COLOR_RGB(98, 48, 26);
                }
            }
            /* Stone Terrace Veranda & Steps */
            if (y > 66 && y <= 78 && x >= 12 && x <= 118) {
                col = COLOR_RGB(188, 178, 162);
                if (y == 70 || y == 74 || y == 78) col = COLOR_RGB(125, 115, 102); /* Steps */
            }
            /* Striped Patio Umbrella on Veranda */
            if (y >= 54 && y <= 65 && x >= 105 && x <= 122) {
                int u_dx = x - 114;
                if (abs(u_dx) <= (y - 54) + 1) {
                    col = (u_dx % 4 == 0) ? COLOR_RGB(225, 45, 45) : COLOR_RGB(245, 245, 250);
                }
            }
            
            if (col != 0) {
                rc->framebuffer[py * SCREEN_W + px] = col;
            }
        }
    }
    
    /* Club Pennant Flag on Rooftop Cupola */
    int flag_x = base_x + 65;
    int flag_y = base_y + 4;
    draw_rect(rc, flag_x, flag_y - 14, 1, 16, COLOR_RGB(225, 230, 235));
    draw_rect(rc, flag_x + 1, flag_y - 14, 12, 7, COLOR_RGB(220, 42, 42));
}

/* Modern Minimap Radar */
static void render_minimap(RenderContext *rc, const GolfState *gs) {
    int mm_x = SCREEN_W - 170;
    int mm_y = 52;
    int mm_w = 156;
    int mm_h = 130;
    
    /* Glass backing */
    draw_rect_alpha(rc, mm_x, mm_y, mm_w, mm_h, COLOR_RGB(18, 26, 34), 215);
    draw_rect_border(rc, mm_x, mm_y, mm_w, mm_h, COLOR_RGB(70, 95, 125));
    draw_text(rc, mm_x + 8, mm_y + 6, "COURSE RADAR", COLOR_RGB(215, 185, 95), true);
    
    int map_x = mm_x + 8;
    int map_y = mm_y + 24;
    int mw = 140;
    int mh = 98;
    
    for (int gy = 0; gy < GRID_H; ++gy) {
        int py = map_y + (gy * mh) / GRID_H;
        for (int gx = 0; gx < GRID_W; ++gx) {
            int px = map_x + (gx * mw) / GRID_W;
            const GolfTile *t = &gs->grid[gx][gy];
            uint32_t c = COLOR_RGB(35, 75, 40); /* rough */
            if (t->surface == SURF_FAIRWAY) c = COLOR_RGB(65, 155, 65);
            else if (t->surface == SURF_GREEN) c = COLOR_RGB(115, 225, 85);
            else if (t->surface == SURF_SAND) c = COLOR_RGB(215, 195, 125);
            else if (t->surface == SURF_WATER) c = COLOR_RGB(45, 115, 195);
            else if (t->surface == SURF_CARTPATH) c = COLOR_RGB(155, 155, 150);
            else if (t->surface == SURF_BRIDGE) c = COLOR_RGB(145, 95, 55);
            else if (t->surface == SURF_TEE) c = COLOR_RGB(95, 185, 55);
            rc->framebuffer[py * SCREEN_W + px] = c;
        }
    }
    
    /* Pin blip (Red) */
    if (gs->has_pin) {
        int px = map_x + (gs->pin_gx * mw) / GRID_W;
        int py = map_y + (gs->pin_gy * mh) / GRID_H;
        draw_rect(rc, px - 1, py - 1, 3, 3, COLOR_RGB(245, 45, 45));
    }
    
    /* Player / Ball blip (Yellow) */
    int bx = map_x + (FROM_FIXED(gs->ball.x) * mw) / GRID_W;
    int by = map_y + (FROM_FIXED(gs->ball.y) * mh) / GRID_H;
    draw_rect(rc, bx - 1, by - 1, 3, 3, COLOR_RGB(255, 235, 45));
}

/* ========================================================================= */
/* DRAWABLE ENTITY LIST & DEPTH-SORTED RENDERING SYSTEM                      */
/* ========================================================================= */

typedef enum {
    DRAWABLE_CLUBHOUSE,
    DRAWABLE_FEATURE,
    DRAWABLE_FLAGSTICK,
    DRAWABLE_SPECTATOR,
    DRAWABLE_CADDY,
    DRAWABLE_GOLFER,
    DRAWABLE_BALL,
    DRAWABLE_BEV_CART,
    DRAWABLE_GOPHER_HOLE,
    DRAWABLE_GOPHER,
    DRAWABLE_CARL,
    DRAWABLE_TNT_BOX,
    DRAWABLE_TUMBLEWEED,
    DRAWABLE_SEA_MONSTER,
    DRAWABLE_RANGER
} DrawableType;

typedef struct {
    int32_t sort_key;     /* Continuous depth (world fx + fy) */
    DrawableType type;
    int32_t gx, gy;
    int32_t data;         /* feature type or spectator index */
    int32_t foot_sx;      /* Foot anchor screen X */
    int32_t foot_sy;      /* Foot anchor screen Y */
} DrawableItem;

#define MAX_DRAWABLES 4096
static DrawableItem s_drawables[MAX_DRAWABLES];
static int s_drawable_count = 0;

static int compare_drawables(const void *a, const void *b) {
    const DrawableItem *da = (const DrawableItem *)a;
    const DrawableItem *db = (const DrawableItem *)b;
    if (da->sort_key < db->sort_key) return -1;
    if (da->sort_key > db->sort_key) return 1;
    return 0;
}

/* Blit Sprite Pixels with Depth Occlusion Check */
static void blit_sprite_depth(RenderContext *rc, int32_t sp_x, int32_t sp_y, int32_t w, int32_t h,
                              const uint32_t *sprite, int32_t item_depth) {
    for (int py = 0; py < h; ++py) {
        int f_py = sp_y + py;
        if (f_py < 0 || f_py >= SCREEN_H) continue;
        for (int px = 0; px < w; ++px) {
            int f_px = sp_x + px;
            if (f_px < 0 || f_px >= SCREEN_W) continue;
            uint32_t col = sprite[py * w + px];
            if (col == 0) continue;
            
            /* Depth occlusion test: only occluded if a foreground hill/cliff is higher depth */
            int idx = f_py * SCREEN_W + f_px;
            if (item_depth >= s_depth_buffer[idx] - 12) {
                rc->framebuffer[idx] = col;
            }
        }
    }
}

/* ========================================================================= */
/* THEMED ENTITIES & SCENERY RENDERERS                                       */
/* ========================================================================= */

static void render_cactus(RenderContext *rc, int32_t sx, int32_t sy, int32_t depth) {
    int w = 38, h = 56;
    int base_x = sx - w / 2;
    int base_y = sy - h + 4;
    draw_drop_shadow(rc, sx, sy, 18, 8, 130);
    
    for (int y = 0; y < h; ++y) {
        int py = base_y + y;
        if (py < 0 || py >= SCREEN_H) continue;
        for (int x = 0; x < w; ++x) {
            int px = base_x + x;
            if (px < 0 || px >= SCREEN_W) continue;
            
            bool inside = false;
            int rib = 0;
            
            /* Central Trunk (x = 14..23, y = 4..55) */
            if (x >= 14 && x <= 23 && y >= 4 && y <= 55) {
                if (y >= 6 || (x >= 16 && x <= 21)) {
                    inside = true;
                    rib = (x % 3 == 0) ? -24 : 12;
                }
            }
            /* Left Arm: branches at y = 30..36, goes left to x = 4..14, then up to y = 14..36 */
            if ((y >= 30 && y <= 36 && x >= 4 && x <= 14) ||
                (y >= 14 && y <= 35 && x >= 4 && x <= 10)) {
                if (y >= 16 || (x >= 5 && x <= 9)) {
                    inside = true;
                    rib = (x % 2 == 0) ? -20 : 10;
                }
            }
            /* Right Arm: branches at y = 24..30, goes right to x = 23..33, then up to y = 8..30 */
            if ((y >= 24 && y <= 30 && x >= 23 && x <= 33) ||
                (y >= 8 && y <= 29 && x >= 27 && x <= 33)) {
                if (y >= 10 || (x >= 28 && x <= 32)) {
                    inside = true;
                    rib = (x % 2 == 0) ? -20 : 10;
                }
            }
            
            if (inside) {
                int r = 58 + rib;
                int g = 118 + rib * 2;
                int b = 48 + rib / 2;
                if (((x * 5 + y * 7) % 11 == 0)) {
                    r = 240; g = 235; b = 180;
                }
                int idx = py * SCREEN_W + px;
                if (depth >= s_depth_buffer[idx] - 12) {
                    rc->framebuffer[idx] = COLOR_RGB(r, g, b);
                }
            }
        }
    }
}

static void render_desert_monolith_rock(RenderContext *rc, int32_t sx, int32_t sy, int32_t depth) {
    int w = 46, h = 34;
    int base_x = sx - w / 2;
    int base_y = sy - h + 4;
    draw_drop_shadow(rc, sx, sy, 22, 10, 140);
    
    for (int y = 0; y < h; ++y) {
        int py = base_y + y;
        if (py < 0 || py >= SCREEN_H) continue;
        for (int x = 0; x < w; ++x) {
            int px = base_x + x;
            if (px < 0 || px >= SCREEN_W) continue;
            
            int dx = x - w / 2;
            if (y >= 4 && abs(dx) <= (w / 2 - 2 - (34 - y) / 6)) {
                int layer = (y / 5) % 4;
                int r, g, b;
                if (layer == 0) { r = 215; g = 115; b = 65; }
                else if (layer == 1) { r = 185; g = 90; b = 50; }
                else if (layer == 2) { r = 230; g = 140; b = 80; }
                else { r = 165; g = 75; b = 42; }
                
                int shade = (dx < -4) ? 22 : ((dx > 6) ? -28 : 0);
                r += shade; g += shade; b += shade;
                if (x == 17 || x == 31) { r -= 30; g -= 20; b -= 15; }
                
                if (r > 255) r = 255; if (r < 0) r = 0;
                if (g > 255) g = 255; if (g < 0) g = 0;
                if (b > 255) b = 255; if (b < 0) b = 0;
                
                int idx = py * SCREEN_W + px;
                if (depth >= s_depth_buffer[idx] - 12) {
                    rc->framebuffer[idx] = COLOR_RGB(r, g, b);
                }
            }
        }
    }
}

static void render_tumbleweed(RenderContext *rc, int32_t sx, int32_t sy, int rot, int32_t depth) {
    int rad = 11;
    draw_drop_shadow(rc, sx, sy, 10, 5, 110);
    
    for (int dy = -rad; dy <= rad; ++dy) {
        int py = sy - rad + dy;
        if (py < 0 || py >= SCREEN_H) continue;
        for (int dx = -rad; dx <= rad; ++dx) {
            int px = sx + dx;
            if (px < 0 || px >= SCREEN_W) continue;
            
            int d2 = dx * dx + dy * dy;
            if (d2 <= rad * rad) {
                float rad_ang = (float)rot * 0.01745f;
                int rx = (int)(cosf(rad_ang) * dx - sinf(rad_ang) * dy);
                int ry = (int)(sinf(rad_ang) * dx + cosf(rad_ang) * dy);
                
                bool is_twig = ((abs(rx * 7 + ry * 13) % 9) <= 2) || (d2 >= (rad - 2) * (rad - 2));
                if (is_twig) {
                    int light = (dy < -2) ? 18 : ((dy > 3) ? -18 : 0);
                    int r = 195 + light;
                    int g = 160 + light;
                    int b = 105 + light;
                    int idx = py * SCREEN_W + px;
                    if (depth >= s_depth_buffer[idx] - 12) {
                        rc->framebuffer[idx] = COLOR_RGB(r, g, b);
                    }
                }
            }
        }
    }
}

static void render_autumn_tree(RenderContext *rc, int32_t sx, int32_t sy, int feat, int32_t depth) {
    (void)feat;
    int w = 52, h = 60;
    int base_x = sx - w / 2;
    int base_y = sy - h + 6;
    draw_drop_shadow(rc, sx, sy, 22, 11, 140);
    
    for (int y = 38; y < h; ++y) {
        int py = base_y + y;
        if (py < 0 || py >= SCREEN_H) continue;
        for (int x = 22; x <= 29; ++x) {
            int px = base_x + x;
            if (px < 0 || px >= SCREEN_W) continue;
            int idx = py * SCREEN_W + px;
            if (depth >= s_depth_buffer[idx] - 12) {
                rc->framebuffer[idx] = COLOR_RGB(55, 38, 28);
            }
        }
    }
    
    int clusters[5][4] = {
        { 26, 16, 15, 0 },
        { 14, 24, 13, 1 },
        { 38, 23, 14, 2 },
        { 20, 32, 14, 0 },
        { 33, 33, 14, 1 }
    };
    for (int c = 0; c < 5; ++c) {
        int ccx = base_x + clusters[c][0];
        int ccy = base_y + clusters[c][1];
        int rad = clusters[c][2];
        int pal = clusters[c][3];
        
        for (int dy = -rad; dy <= rad; ++dy) {
            int py = ccy + dy;
            if (py < 0 || py >= SCREEN_H) continue;
            for (int dx = -rad; dx <= rad; ++dx) {
                int px = ccx + dx;
                if (px < 0 || px >= SCREEN_W) continue;
                if (dx * dx + dy * dy <= rad * rad) {
                    int light = (dy < -2 && dx < 2) ? 25 : ((dy > 4) ? -22 : 0);
                    int r, g, b;
                    if (pal == 0) {
                        r = 235 + light; g = 105 + light / 2; b = 25;
                    } else if (pal == 1) {
                        r = 215 + light; g = 38 + light / 2; b = 32;
                    } else {
                        r = 245 + light; g = 185 + light / 2; b = 35;
                    }
                    if (r > 255) r = 255; if (r < 0) r = 0;
                    if (g > 255) g = 255; if (g < 0) g = 0;
                    if (b > 255) b = 255; if (b < 0) b = 0;
                    
                    int idx = py * SCREEN_W + px;
                    if (depth >= s_depth_buffer[idx] - 12) {
                        rc->framebuffer[idx] = COLOR_RGB(r, g, b);
                    }
                }
            }
        }
    }
}

static void render_sickly_bush(RenderContext *rc, int32_t sx, int32_t sy, int32_t depth) {
    int w = 32, h = 24;
    int base_x = sx - w / 2;
    int base_y = sy - h + 2;
    draw_drop_shadow(rc, sx, sy, 14, 6, 120);
    
    for (int y = 0; y < h; ++y) {
        int py = base_y + y;
        if (py < 0 || py >= SCREEN_H) continue;
        for (int x = 0; x < w; ++x) {
            int px = base_x + x;
            if (px < 0 || px >= SCREEN_W) continue;
            
            int dx = x - 16;
            int dy = y - 13;
            if (dx * dx * 2 + dy * dy * 4 <= 190) {
                int light = (dy < 0) ? 18 : -16;
                int r = 162 + light;
                int g = 158 + light;
                int b = 45;
                if ((x * 7 + y * 13) % 9 == 0) {
                    r = 65; g = 55; b = 30;
                }
                int idx = py * SCREEN_W + px;
                if (depth >= s_depth_buffer[idx] - 12) {
                    rc->framebuffer[idx] = COLOR_RGB(r, g, b);
                }
            }
        }
    }
}

static void render_coffin_pile(RenderContext *rc, int32_t sx, int32_t sy, int32_t depth) {
    int w = 40, h = 26;
    int base_x = sx - w / 2;
    int base_y = sy - h + 2;
    draw_drop_shadow(rc, sx, sy, 18, 8, 140);
    
    for (int c = 0; c < 2; ++c) {
        int ox = (c == 0) ? 0 : 8;
        int oy = (c == 0) ? 6 : 0;
        for (int y = 0; y < 14; ++y) {
            int py = base_y + oy + y;
            if (py < 0 || py >= SCREEN_H) continue;
            for (int x = 0; x < 28; ++x) {
                int px = base_x + ox + x;
                if (px < 0 || px >= SCREEN_W) continue;
                
                int max_dx = (x < 6) ? x * 2 : ((x > 22) ? (27 - x) * 2 : 12);
                int dy = abs(y - 7);
                if (dy <= max_dx / 2) {
                    int r = 68, g = 44, b = 28;
                    if (y == 0 || y == 13 || x == 0 || x == 27) {
                        r = 42; g = 26; b = 16;
                    }
                    if ((x >= 12 && x <= 16 && y == 7) || (x == 14 && y >= 5 && y <= 9) ||
                        (y == 2 && (x == 8 || x == 20)) || (y == 11 && (x == 8 || x == 20))) {
                        r = 195; g = 200; b = 210;
                    }
                    int idx = py * SCREEN_W + px;
                    if (depth >= s_depth_buffer[idx] - 12) {
                        rc->framebuffer[idx] = COLOR_RGB(r, g, b);
                    }
                }
            }
        }
    }
}

static void render_burning_man_effigy(RenderContext *rc, int32_t sx, int32_t sy, uint32_t tick_count) {
    int base_y = sy - 65;
    draw_drop_shadow(rc, sx, sy, 42, 18, 140);
    
    for (int y = 45; y < 65; ++y) {
        int py = base_y + y;
        if (py < 0 || py >= SCREEN_H) continue;
        int half_w = 16 + (y - 45);
        for (int dx = -half_w; dx <= half_w; ++dx) {
            int px = sx + dx;
            if (px < 0 || px >= SCREEN_W) continue;
            int timber = ((dx ^ y) & 3) ? 14 : -14;
            int r = 160 + timber, g = 115 + timber, b = 65 + timber;
            rc->framebuffer[py * SCREEN_W + px] = COLOR_RGB(r, g, b);
        }
    }
    
    int pulse = (tick_count / 12) % 3;
    uint32_t neon_col = (pulse == 0) ? COLOR_RGB(0, 255, 230) : ((pulse == 1) ? COLOR_RGB(255, 20, 160) : COLOR_RGB(255, 235, 40));
    
    for (int dy = -4; dy <= 4; ++dy) {
        for (int dx = -3; dx <= 3; ++dx) {
            if (abs(dx) + abs(dy) <= 4) {
                int px = sx + dx, py = base_y + 6 + dy;
                if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
                    rc->framebuffer[py * SCREEN_W + px] = (abs(dx) + abs(dy) >= 3) ? neon_col : COLOR_RGB(210, 155, 80);
                }
            }
        }
    }
    
    for (int y = 11; y <= 28; ++y) {
        int py = base_y + y;
        if (py < 0 || py >= SCREEN_H) continue;
        for (int dx = -2; dx <= 2; ++dx) {
            int px = sx + dx;
            if (px < 0 || px >= SCREEN_W) continue;
            rc->framebuffer[py * SCREEN_W + px] = (dx == -2 || dx == 2) ? neon_col : COLOR_RGB(195, 140, 75);
        }
    }
    
    for (int i = 0; i <= 14; ++i) {
        int ly = base_y + 18 - i;
        int lx1 = sx - 3 - i;
        int lx2 = sx + 3 + i;
        if (ly >= 0 && ly < SCREEN_H) {
            if (lx1 >= 0 && lx1 < SCREEN_W) rc->framebuffer[ly * SCREEN_W + lx1] = neon_col;
            if (lx2 >= 0 && lx2 < SCREEN_W) rc->framebuffer[ly * SCREEN_W + lx2] = neon_col;
        }
    }
    
    for (int i = 0; i <= 16; ++i) {
        int ly = base_y + 29 + i;
        int lx1 = sx - 1 - i * 8 / 16;
        int lx2 = sx + 1 + i * 8 / 16;
        if (ly >= 0 && ly < SCREEN_H) {
            if (lx1 >= 0 && lx1 < SCREEN_W) rc->framebuffer[ly * SCREEN_W + lx1] = neon_col;
            if (lx2 >= 0 && lx2 < SCREEN_W) rc->framebuffer[ly * SCREEN_W + lx2] = neon_col;
        }
    }
    
    uint32_t burner_cols[4] = { COLOR_RGB(255, 60, 120), COLOR_RGB(50, 220, 255), COLOR_RGB(255, 230, 40), COLOR_RGB(180, 50, 255) };
    for (int b = 0; b < 4; ++b) {
        int bx = sx - 35 + b * 24;
        int by = sy - 4 + ((b % 2) ? 6 : -4);
        int jump = ((tick_count / 8 + b * 3) % 2) ? -3 : 0;
        uint32_t bcol = burner_cols[b];
        
        draw_rect(rc, bx - 1, by - 12 + jump, 3, 3, COLOR_RGB(245, 205, 175));
        draw_rect(rc, bx - 2, by - 9 + jump, 5, 6, bcol);
        draw_rect(rc, bx - 4, by - 11 + jump, 2, 3, bcol);
        draw_rect(rc, bx + 3, by - 11 + jump, 2, 3, bcol);
        draw_rect(rc, bx - 2, by - 3, 2, 4, COLOR_RGB(45, 45, 55));
        draw_rect(rc, bx + 1, by - 3, 2, 4, COLOR_RGB(45, 45, 55));
    }
    
    draw_text(rc, sx - 44, sy + 14, "THE MAN BURNS!", COLOR_RGB(255, 215, 65), true);
}

static void render_transylvania_castle(RenderContext *rc, int32_t sx, int32_t sy) {
    int b_w = 120, b_h = 80;
    int base_x = sx - b_w / 2;
    int base_y = sy - b_h + 10;
    draw_drop_shadow(rc, sx, sy, 55, 22, 150);
    
    for (int y = 0; y < b_h; ++y) {
        int py = base_y + y;
        if (py < 0 || py >= SCREEN_H) continue;
        for (int x = 0; x < b_w; ++x) {
            int px = base_x + x;
            if (px < 0 || px >= SCREEN_W) continue;
            
            uint32_t col = 0;
            if ((x >= 10 && x <= 30) || (x >= 90 && x <= 110)) {
                int cx = (x < 50) ? 20 : 100;
                if (y < 28) {
                    if (abs(x - cx) <= (y + 1) * 10 / 28) {
                        col = COLOR_RGB(78, 48, 98);
                    }
                } else if (y <= 70) {
                    int brick = ((x ^ y) & 2) ? 10 : -10;
                    col = COLOR_RGB(56 + brick, 52 + brick, 66 + brick);
                    if (y >= 40 && y <= 50 && abs(x - cx) <= 2) {
                        col = COLOR_RGB(235, 42, 42);
                    }
                }
            }
            if (x >= 30 && x <= 90 && y >= 25 && y <= 72) {
                int brick = ((x ^ y) & 2) ? 10 : -10;
                col = COLOR_RGB(52 + brick, 48 + brick, 62 + brick);
                if (y >= 25 && y <= 30 && ((x / 8) % 2 == 1)) {
                    col = 0;
                }
                if (y >= 38 && y <= 54 && abs(x - 60) <= 6) {
                    col = COLOR_RGB(220, 35, 35);
                    if (x == 60 || y == 46) col = COLOR_RGB(30, 25, 35);
                }
                if (y >= 58 && y <= 72 && abs(x - 60) <= 8) {
                    col = (x % 3 == 0 || y % 3 == 0) ? COLOR_RGB(25, 20, 28) : COLOR_RGB(165, 45, 45);
                }
            }
            
            if (col != 0) {
                rc->framebuffer[py * SCREEN_W + px] = col;
            }
        }
    }
    draw_text(rc, sx - 42, sy + 14, "CASTLE DRACULA", COLOR_RGB(195, 175, 225), true);
}

static void render_raver(RenderContext *rc, int32_t sx, int32_t sy, int32_t pass_out_timer, uint32_t tick_count, int32_t depth) {
    (void)depth;
    if (pass_out_timer > 0) {
        draw_drop_shadow(rc, sx, sy, 14, 6, 120);
        for (int dx = -10; dx <= 10; ++dx) {
            for (int dy = -4; dy <= 2; ++dy) {
                int px = sx + dx, py = sy + dy;
                if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
                    uint32_t c = (dx < -4) ? COLOR_RGB(255, 60, 180) : ((dx < 4) ? COLOR_RGB(40, 230, 255) : COLOR_RGB(45, 45, 55));
                    rc->framebuffer[py * SCREEN_W + px] = c;
                }
            }
        }
        draw_rect(rc, sx - 13, sy - 5, 4, 4, COLOR_RGB(255, 230, 40));
        int z_float = (tick_count / 14) % 4;
        draw_text(rc, sx + 4 + z_float, sy - 16 - z_float * 2, "Zzz...", COLOR_RGB(255, 255, 180), true);
        return;
    }
    
    draw_drop_shadow(rc, sx, sy, 10, 5, 120);
    int step = (tick_count / 10) % 2;
    int arm_y = step ? -3 : 2;
    
    draw_rect(rc, sx - 6, sy - 28, 12, 3, COLOR_RGB(255, 235, 40));
    draw_rect(rc, sx - 7, sy - 27, 3, 6, COLOR_RGB(255, 235, 40));
    draw_rect(rc, sx + 5, sy - 27, 3, 6, COLOR_RGB(255, 235, 40));
    
    draw_rect(rc, sx - 4, sy - 25, 8, 7, COLOR_RGB(245, 205, 175));
    draw_rect(rc, sx - 4, sy - 23, 8, 2, COLOR_RGB(20, 20, 25));
    
    draw_rect(rc, sx - 9, sy - 18 + arm_y, 4, 3, COLOR_RGB(0, 255, 220));
    draw_rect(rc, sx + 6, sy - 18 - arm_y, 4, 3, COLOR_RGB(255, 40, 180));
    
    uint32_t shirt_col = (step == 0) ? COLOR_RGB(255, 40, 180) : COLOR_RGB(40, 240, 220);
    draw_rect(rc, sx - 4, sy - 18, 9, 10, shirt_col);
    
    draw_rect(rc, sx - 4, sy - 8, 4, 5, COLOR_RGB(45, 55, 65));
    draw_rect(rc, sx + 1, sy - 8, 4, 5, COLOR_RGB(45, 55, 65));
    draw_rect(rc, sx - 5, sy - 3, 4, 3, COLOR_RGB(255, 255, 255));
    draw_rect(rc, sx + 2, sy - 3, 4, 3, COLOR_RGB(255, 255, 255));
    
    const char *rmsg = "UNCE UNCE!";
    int bub_w = 92, bub_h = 18;
    int bub_x = sx - bub_w / 2;
    int bub_y = sy - 46;
    draw_rect(rc, bub_x, bub_y, bub_w, bub_h, COLOR_RGB(255, 255, 255));
    draw_rect_border(rc, bub_x, bub_y, bub_w, bub_h, COLOR_RGB(40, 40, 45));
    draw_text(rc, bub_x + 6, bub_y + 5, rmsg, COLOR_RGB(220, 20, 140), false);
}

static void render_police_cart(RenderContext *rc, int32_t sx, int32_t sy, uint32_t tick_count, int32_t depth) {
    int w = 44, h = 36;
    int base_x = sx - w / 2;
    int base_y = sy - h + 4;
    draw_drop_shadow(rc, sx, sy, 22, 10, 140);
    
    for (int y = 0; y < h; ++y) {
        int py = base_y + y;
        if (py < 0 || py >= SCREEN_H) continue;
        for (int x = 0; x < w; ++x) {
            int px = base_x + x;
            if (px < 0 || px >= SCREEN_W) continue;
            
            uint32_t col = 0;
            if (y >= 26 && ((x >= 6 && x <= 12) || (x >= 28 && x <= 34))) {
                col = (y == 29 && (x == 9 || x == 31)) ? COLOR_RGB(180, 185, 190) : COLOR_RGB(30, 32, 35);
            }
            else if (y >= 18 && y <= 27 && x >= 4 && x <= 36) {
                col = (x >= 14 && x <= 26) ? COLOR_RGB(245, 248, 252) : COLOR_RGB(28, 56, 138);
                if (x == 5 && y >= 20 && y <= 22) col = COLOR_RGB(255, 235, 140);
            }
            else if (y >= 6 && y <= 9 && x >= 8 && x <= 34) {
                col = COLOR_RGB(240, 245, 250);
            }
            else if ((x == 10 || x == 32) && y >= 9 && y <= 18) {
                col = COLOR_RGB(60, 65, 70);
            }
            else if (y >= 2 && y <= 5 && x >= 17 && x <= 25) {
                bool strobe = ((tick_count / 8) % 2 == 0);
                if (x <= 20) col = strobe ? COLOR_RGB(255, 30, 30) : COLOR_RGB(80, 10, 10);
                else col = strobe ? COLOR_RGB(10, 30, 90) : COLOR_RGB(30, 120, 255);
            }
            
            if (col != 0) {
                int idx = py * SCREEN_W + px;
                if (depth >= s_depth_buffer[idx] - 12) {
                    rc->framebuffer[idx] = col;
                }
            }
        }
    }
    
    draw_rect(rc, sx - 2, sy - 24, 6, 6, COLOR_RGB(245, 205, 175));
    draw_rect(rc, sx - 3, sy - 26, 8, 3, COLOR_RGB(22, 45, 110));
    draw_rect(rc, sx, sy - 25, 2, 2, COLOR_RGB(255, 215, 60));
    draw_text(rc, sx - 16, sy + 7, "POLICE", COLOR_RGB(255, 225, 90), true);
}

static void render_zombie(RenderContext *rc, int32_t sx, int32_t sy, int anim_f, int32_t depth) {
    (void)depth;
    draw_drop_shadow(rc, sx, sy, 12, 5, 130);
    int step = (anim_f % 2);
    
    draw_rect(rc, sx - 4, sy - 26, 8, 8, COLOR_RGB(95, 138, 85));
    draw_rect(rc, sx - 3, sy - 24, 2, 2, COLOR_RGB(255, 245, 60));
    draw_rect(rc, sx + 1, sy - 24, 2, 2, COLOR_RGB(255, 245, 60));
    draw_rect(rc, sx - 2, sy - 20, 4, 1, COLOR_RGB(45, 25, 25));
    
    draw_rect(rc, sx - 8, sy - 18, 5, 3, COLOR_RGB(95, 138, 85));
    draw_rect(rc, sx + 4, sy - 18, 5, 3, COLOR_RGB(95, 138, 85));
    
    draw_rect(rc, sx - 4, sy - 18, 9, 10, COLOR_RGB(52, 42, 60));
    
    draw_rect(rc, sx - 4, sy - 8, 3, step ? 6 : 4, COLOR_RGB(38, 32, 44));
    draw_rect(rc, sx + 1, sy - 8, 3, step ? 4 : 6, COLOR_RGB(38, 32, 44));
    
    draw_text(rc, sx - 20, sy - 36, "BRAAAINS!", COLOR_RGB(165, 225, 140), true);
}

static void render_ash_williams(RenderContext *rc, int32_t sx, int32_t sy, CarlState state, uint32_t tick_count, int32_t depth) {
    (void)depth;
    draw_drop_shadow(rc, sx, sy, 14, 6, 140);
    
    draw_rect(rc, sx - 4, sy - 28, 8, 4, COLOR_RGB(65, 45, 30));
    draw_rect(rc, sx - 4, sy - 24, 8, 7, COLOR_RGB(245, 205, 175));
    draw_rect(rc, sx + 1, sy - 22, 2, 3, COLOR_RGB(185, 30, 30));
    
    draw_rect(rc, sx - 5, sy - 17, 10, 10, COLOR_RGB(45, 95, 165));
    draw_rect(rc, sx - 4, sy - 16, 2, 8, COLOR_RGB(115, 65, 35));
    draw_rect(rc, sx + 2, sy - 16, 2, 8, COLOR_RGB(115, 65, 35));
    
    draw_rect(rc, sx - 5, sy - 7, 4, 6, COLOR_RGB(95, 75, 50));
    draw_rect(rc, sx + 1, sy - 7, 4, 6, COLOR_RGB(95, 75, 50));
    draw_rect(rc, sx - 6, sy - 1, 5, 3, COLOR_RGB(40, 30, 22));
    draw_rect(rc, sx + 1, sy - 1, 5, 3, COLOR_RGB(40, 30, 22));
    
    draw_rect(rc, sx + 5, sy - 16, 8, 5, COLOR_RGB(215, 35, 30));
    draw_rect(rc, sx + 13, sy - 15, 9, 3, COLOR_RGB(180, 185, 195));
    if ((tick_count / 3) % 2 == 0) {
        draw_rect(rc, sx + 15, sy - 17, 3, 2, COLOR_RGB(255, 235, 120));
    }
    
    draw_rect(rc, sx - 9, sy - 14, 5, 3, COLOR_RGB(115, 65, 35));
    draw_rect(rc, sx - 15, sy - 13, 7, 2, COLOR_RGB(60, 65, 70));
    
    const char *ash_msg = "THIS IS MY BOOMSTICK!";
    if (state == CARL_STATE_CELEBRATING) ash_msg = "GROOVY! LOL";
    int bub_w = (int)strlen(ash_msg) * 8 + 16;
    int bub_h = 20;
    int bub_x = sx - bub_w / 2;
    int bub_y = sy - 56;
    draw_rect(rc, bub_x, bub_y, bub_w, bub_h, COLOR_RGB(255, 255, 255));
    draw_rect_border(rc, bub_x, bub_y, bub_w, bub_h, COLOR_RGB(35, 45, 55));
    draw_text(rc, bub_x + 8, bub_y + 6, ash_msg, COLOR_RGB(30, 45, 75), false);
}

static void render_sea_monster(RenderContext *rc, int32_t sx, int32_t sy, int state, uint32_t tick_count, int32_t depth) {
    (void)depth;
    if (state == 1) {
        for (int r = 8; r <= 16; r += 4) {
            int wave = (int)(sinf((float)tick_count * 0.15f) * 2.0f);
            draw_rect_alpha(rc, sx - r - wave, sy - r / 2, (r + wave) * 2, r, COLOR_RGB(70, 110, 160), 120);
        }
        draw_rect(rc, sx - 6, sy - 8, 12, 6, COLOR_RGB(25, 75, 65));
        draw_rect(rc, sx - 3, sy - 12, 6, 4, COLOR_RGB(20, 65, 55));
        draw_rect(rc, sx - 4, sy - 7, 2, 2, COLOR_RGB(255, 245, 50));
        draw_rect(rc, sx + 2, sy - 7, 2, 2, COLOR_RGB(255, 245, 50));
    } else if (state == 2) {
        int th = 42;
        for (int y = 0; y < th; ++y) {
            int py = sy - y;
            if (py < 0 || py >= SCREEN_H) continue;
            int curve_x = (int)(sinf((float)y * 0.12f) * 9.0f);
            int half_w = 6 - (y * 4 / th);
            if (half_w < 2) half_w = 2;
            for (int dx = -half_w; dx <= half_w; ++dx) {
                int px = sx + curve_x + dx;
                if (px < 0 || px >= SCREEN_W) continue;
                uint32_t col = (dx == half_w) ? COLOR_RGB(240, 220, 200) : COLOR_RGB(75, 35, 95);
                rc->framebuffer[py * SCREEN_W + px] = col;
            }
        }
        for (int i = 0; i < 8; ++i) {
            int sp_x = sx - 12 + (i * 3);
            int sp_y = sy - 2 + (i % 3);
            if (sp_x >= 0 && sp_x < SCREEN_W && sp_y >= 0 && sp_y < SCREEN_H) {
                rc->framebuffer[sp_y * SCREEN_W + sp_x] = COLOR_RGB(240, 248, 255);
            }
        }
        draw_text(rc, sx - 35, sy - th - 14, "LEVIATHAN SWIPE!", COLOR_RGB(165, 225, 255), true);
    }
}

static void render_ranger(RenderContext *rc, const GolfState *gs, int32_t sx, int32_t sy, int32_t depth) {
    int w = 44, h = 36;
    int base_x = sx - w / 2;
    int base_y = sy - h + 4;
    draw_drop_shadow(rc, sx, sy, 22, 10, 140);
    
    for (int y = 0; y < h; ++y) {
        int py = base_y + y;
        if (py < 0 || py >= SCREEN_H) continue;
        for (int x = 0; x < w; ++x) {
            int px = base_x + x;
            if (px < 0 || px >= SCREEN_W) continue;
            
            uint32_t col = 0;
            if (y >= 26 && ((x >= 6 && x <= 12) || (x >= 28 && x <= 34))) {
                col = (y == 29 && (x == 9 || x == 31)) ? COLOR_RGB(180, 185, 190) : COLOR_RGB(30, 32, 35);
            }
            else if (y >= 18 && y <= 27 && x >= 4 && x <= 36) {
                col = COLOR_RGB(185, 160, 120);
                if (x == 5 && y >= 20 && y <= 22) col = COLOR_RGB(255, 235, 140);
            }
            else if (y >= 6 && y <= 9 && x >= 8 && x <= 34) {
                col = COLOR_RGB(45, 75, 45);
            }
            else if ((x == 10 || x == 32) && y >= 9 && y <= 18) {
                col = COLOR_RGB(60, 65, 70);
            }
            
            if (col != 0) {
                int idx = py * SCREEN_W + px;
                if (depth >= s_depth_buffer[idx] - 12) {
                    rc->framebuffer[idx] = col;
                }
            }
        }
    }
    
    draw_rect(rc, sx - 2, sy - 24, 6, 6, COLOR_RGB(245, 205, 175));
    draw_rect(rc, sx - 5, sy - 27, 12, 3, COLOR_RGB(145, 115, 75));
    draw_rect(rc, sx - 3, sy - 30, 8, 4, COLOR_RGB(130, 100, 60));
    draw_rect(rc, sx - 3, sy - 18, 8, 7, COLOR_RGB(175, 145, 105));
    
    draw_text(rc, sx - 18, sy + 7, "RANGER", COLOR_RGB(255, 235, 90), true);
    
    if (gs->ranger.speech_timer > 0) {
        int bub_w = (int)strlen(gs->ranger.speech_msg) * 8 + 16;
        int bub_h = 22;
        int bub_x = sx - bub_w / 2;
        int bub_y = sy - 52;
        draw_rect(rc, bub_x, bub_y, bub_w, bub_h, COLOR_RGB(255, 255, 255));
        draw_rect_border(rc, bub_x, bub_y, bub_w, bub_h, COLOR_RGB(35, 45, 55));
        draw_text(rc, bub_x + 8, bub_y + 7, gs->ranger.speech_msg, COLOR_RGB(180, 30, 30), false);
    }
}

/* ========================================================================= */
/* MAIN RENDER FRAME                                                         */
/* ========================================================================= */

void render_frame(RenderContext *rc, const GolfState *gs) {
    rc->tick_count++;
    
    if (gs->mode == MODE_SPLASH) {
        render_splash_screen(rc, rc->tick_count);
        return;
    }
    if (gs->mode == MODE_SETUP) {
        render_setup_screen(rc, gs, rc->tick_count);
        return;
    }
    
    static CourseTheme s_last_rendered_theme = (CourseTheme)-1;
    if (gs->theme != s_last_rendered_theme) {
        generate_procedural_graphics_themed(gs->theme);
        s_last_rendered_theme = gs->theme;
    }
    
    /* 1. Clear Framebuffer to Sky Backdrop & Depth Buffer */
    uint32_t sky_col;
    if (gs->theme == THEME_LA_PLAYA) sky_col = COLOR_RGB(82, 68, 54);
    else if (gs->theme == THEME_NOCTURNAL_GREEN) sky_col = COLOR_RGB(22, 18, 30);
    else sky_col = COLOR_RGB(34, 46, 58);
    for (int i = 0; i < SCREEN_W * SCREEN_H; ++i) {
        rc->framebuffer[i] = sky_col;
        s_depth_buffer[i] = -999999;
    }
    
    /* ===================================================================== */
    /* PASS 1: TERRAIN GRID (Diagonal Scan Order d = 0..126)                 */
    /* ===================================================================== */
    for (int d = 0; d < GRID_W + GRID_H - 1; ++d) {
        for (int x = 0; x <= d; ++x) {
            int y = d - x;
            if (x >= GRID_W || y >= GRID_H) continue;
            
            const GolfTile *tile = &gs->grid[x][y];
            int32_t sx, sy;
            grid_to_screen(x, y, tile->elevation, rc->camera_x, rc->camera_y, &sx, &sy);
            
            /* View frustum clipping */
            if (sx < -140 || sx > SCREEN_W + 140 || sy < -140 || sy > SCREEN_H + 140) {
                continue;
            }
            
            /* Base depth for this tile: (x + y) * 32 */
            int32_t tile_base_depth = (x + y) * 32;
            
            /* Cliff Skirt Walls */
            int32_t t_elev = (tile->elevation > MAX_ELEVATION_TIER) ? MAX_ELEVATION_TIER : tile->elevation;
            /* SW Edge: neighbor is in +Y direction (x, y + 1) */
            int32_t sw_elev = (y < GRID_H - 1) ? ((gs->grid[x][y + 1].elevation > MAX_ELEVATION_TIER) ? MAX_ELEVATION_TIER : gs->grid[x][y + 1].elevation) : 0;
            if (t_elev > sw_elev) {
                int drop_h = (t_elev - sw_elev) * HEIGHT_STEP_PX;
                int32_t cliff_depth = tile_base_depth + 16;
                for (int dy = 0; dy < drop_h; ++dy) {
                    for (int dx = 0; dx < TILE_HALF_W; ++dx) {
                        int r_py = 16 + (dx / 2) + dy;
                        int px = sx + dx;
                        int py = sy + r_py;
                        if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
                            int idx = py * SCREEN_W + px;
                            rc->framebuffer[idx] = s_cliff_sw[(dy % 72) * 64 + dx];
                            s_depth_buffer[idx] = cliff_depth;
                        }
                    }
                }
            }
            
            /* SE Edge: neighbor is in +X direction (x + 1, y) */
            int32_t se_elev = (x < GRID_W - 1) ? ((gs->grid[x + 1][y].elevation > MAX_ELEVATION_TIER) ? MAX_ELEVATION_TIER : gs->grid[x + 1][y].elevation) : 0;
            if (t_elev > se_elev) {
                int drop_h = (t_elev - se_elev) * HEIGHT_STEP_PX;
                int32_t cliff_depth = tile_base_depth + 16;
                for (int dy = 0; dy < drop_h; ++dy) {
                    for (int dx = 0; dx < TILE_HALF_W; ++dx) {
                        int r_py = 31 - (dx / 2) + dy;
                        int px = sx + TILE_HALF_W + dx;
                        int py = sy + r_py;
                        if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
                            int idx = py * SCREEN_W + px;
                            rc->framebuffer[idx] = s_cliff_se[(dy % 72) * 64 + dx];
                            s_depth_buffer[idx] = cliff_depth;
                        }
                    }
                }
            }
            
            /* Surface Tile Blit */
            const uint32_t *tile_pixels = s_tile_rough[tile->variant % 4];
            if (tile->surface == SURF_FAIRWAY) tile_pixels = s_tile_fairway[tile->autotile_mask & 15];
            else if (tile->surface == SURF_GREEN) tile_pixels = s_tile_green[tile->autotile_mask & 15];
            else if (tile->surface == SURF_SAND) tile_pixels = s_tile_sand[tile->autotile_mask & 15];
            else if (tile->surface == SURF_WATER) tile_pixels = s_tile_water[(rc->tick_count / 8) % 4];
            else if (tile->surface == SURF_CARTPATH) tile_pixels = s_tile_cartpath[tile->autotile_mask & 15];
            else if (tile->surface == SURF_BRIDGE) tile_pixels = s_tile_bridge[tile->variant % 4];
            else if (tile->surface == SURF_TEE) tile_pixels = s_tile_tee;
            
            /* Slope lighting modifier */
            float slope_factor = 1.0f;
            if (x > 0 && x < GRID_W - 1 && y > 0 && y < GRID_H - 1) {
                int de_x = gs->grid[x + 1][y].elevation - gs->grid[x - 1][y].elevation;
                int de_y = gs->grid[x][y + 1].elevation - gs->grid[x][y - 1].elevation;
                slope_factor += (float)(de_x - de_y) * 0.05f;
                if (slope_factor < 0.75f) slope_factor = 0.75f;
                if (slope_factor > 1.25f) slope_factor = 1.25f;
            }
            
            for (int py = 0; py < TILE_H; ++py) {
                int f_py = sy + py;
                if (f_py < 0 || f_py >= SCREEN_H) continue;
                for (int px = 0; px < TILE_W; ++px) {
                    int f_px = sx + px;
                    if (f_px < 0 || f_px >= SCREEN_W) continue;
                    uint32_t col = tile_pixels[py * TILE_W + px];
                    if (col != 0) {
                        if (slope_factor != 1.0f) col = shade_color(col, slope_factor);
                        int idx = f_py * SCREEN_W + f_px;
                        rc->framebuffer[idx] = col;
                        s_depth_buffer[idx] = tile_base_depth + py;
                    }
                }
            }
            
            /* Putting Cup Hole on Green (Flag Base) */
            if (gs->has_pin && x == gs->pin_gx && y == gs->pin_gy) {
                int cup_x = sx + TILE_HALF_W;
                int cup_y = sy + TILE_HALF_H;
                for (int dy = -2; dy <= 2; ++dy) {
                    for (int dx = -3; dx <= 3; ++dx) {
                        if (dx * dx * 2 + dy * dy * 5 <= 16) {
                            int px = cup_x + dx;
                            int py = cup_y + dy;
                            if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
                                uint32_t col = (dy == -2) ? COLOR_RGB(245, 248, 252) : COLOR_RGB(16, 22, 18);
                                rc->framebuffer[py * SCREEN_W + px] = col;
                            }
                        }
                    }
                }
            }
            
            /* Mouse Hover Cursor in Build Mode */
            if (gs->mode == MODE_BUILD && rc->mouse_valid) {
                int rad = (gs->brush_size == 2) ? 1 : 0;
                if (abs(x - rc->mouse_gx) <= rad && abs(y - rc->mouse_gy) <= rad) {
                    for (int py = 0; py < TILE_H; ++py) {
                        int f_py = sy + py;
                        if (f_py < 0 || f_py >= SCREEN_H) continue;
                        for (int px = 0; px < TILE_W; ++px) {
                            int f_px = sx + px;
                            if (f_px < 0 || f_px >= SCREEN_W) continue;
                            if (is_in_diamond(px, py, NULL)) {
                                int idx = f_py * SCREEN_W + f_px;
                                rc->framebuffer[idx] = alpha_blend_pixel(rc->framebuffer[idx], COLOR_RGB(255, 235, 90), 85);
                            }
                        }
                    }
                }
            }
        }
    }
    
    /* ===================================================================== */
    /* PASS 2: COLLECT & DEPTH-SORT ALL DRAWABLES (Scenery, Characters, Props) */
    /* ===================================================================== */
    s_drawable_count = 0;
    
    /* 1. Scenery Features & Clubhouse */
    bool clubhouse_added = false;
    for (int y = 0; y < GRID_H; ++y) {
        for (int x = 0; x < GRID_W; ++x) {
            const GolfTile *tile = &gs->grid[x][y];
            if (tile->feature == FEAT_NONE) continue;
            
            if (tile->feature == FEAT_CLUBHOUSE) {
                if (!clubhouse_added && gs->has_clubhouse) {
                    if (s_drawable_count < MAX_DRAWABLES) {
                        DrawableItem *item = &s_drawables[s_drawable_count++];
                        item->type = DRAWABLE_CLUBHOUSE;
                        item->gx = gs->clubhouse_gx;
                        item->gy = gs->clubhouse_gy;
                        item->sort_key = (gs->clubhouse_gx + gs->clubhouse_gy + 2) * 32;
                        grid_to_screen(gs->clubhouse_gx, gs->clubhouse_gy,
                                       gs->grid[gs->clubhouse_gx][gs->clubhouse_gy].elevation,
                                       rc->camera_x, rc->camera_y, &item->foot_sx, &item->foot_sy);
                    }
                    clubhouse_added = true;
                }
            } else {
                if (s_drawable_count < MAX_DRAWABLES) {
                    DrawableItem *item = &s_drawables[s_drawable_count++];
                    item->type = DRAWABLE_FEATURE;
                    item->gx = x;
                    item->gy = y;
                    item->data = tile->feature;
                    item->sort_key = (x + y) * 32 + 16;
                    grid_to_screen(x, y, tile->elevation, rc->camera_x, rc->camera_y, &item->foot_sx, &item->foot_sy);
                    item->foot_sx += TILE_HALF_W;
                    item->foot_sy += TILE_HALF_H;
                }
            }
        }
    }
    
    /* 2. Pin Flagstick */
    if (gs->has_pin && s_drawable_count < MAX_DRAWABLES) {
        DrawableItem *item = &s_drawables[s_drawable_count++];
        item->type = DRAWABLE_FLAGSTICK;
        item->gx = gs->pin_gx;
        item->gy = gs->pin_gy;
        item->sort_key = (gs->pin_gx + gs->pin_gy) * 32 + 16;
        grid_to_screen(gs->pin_gx, gs->pin_gy, gs->grid[gs->pin_gx][gs->pin_gy].elevation,
                       rc->camera_x, rc->camera_y, &item->foot_sx, &item->foot_sy);
        item->foot_sx += TILE_HALF_W;
        item->foot_sy += TILE_HALF_H;
    }
    
    /* 3. Spectators */
    for (int i = 0; i < gs->spectator_count; ++i) {
        const SpectatorEntity *sp = &gs->spectators[i];
        if (!sp->active) continue;
        if (s_drawable_count < MAX_DRAWABLES) {
            DrawableItem *item = &s_drawables[s_drawable_count++];
            item->type = DRAWABLE_SPECTATOR;
            item->gx = sp->gx;
            item->gy = sp->gy;
            item->data = i;
            item->sort_key = (sp->gx + sp->gy) * 32 + 16;
            grid_to_screen(sp->gx, sp->gy, gs->grid[sp->gx][sp->gy].elevation,
                           rc->camera_x, rc->camera_y, &item->foot_sx, &item->foot_sy);
            item->foot_sx += TILE_HALF_W;
            item->foot_sy += TILE_HALF_H;
        }
    }
    
    /* 4. Caddy */
    if (gs->mode == MODE_PLAY && s_drawable_count < MAX_DRAWABLES) {
        DrawableItem *item = &s_drawables[s_drawable_count++];
        item->type = DRAWABLE_CADDY;
        item->sort_key = FROM_FIXED(FMUL(gs->caddy.x + gs->caddy.y, TO_FIXED(32)));
        world_to_screen_fixed(gs->caddy.x, gs->caddy.y, gs->caddy.current_z, rc->camera_x, rc->camera_y, &item->foot_sx, &item->foot_sy);
    }
    
    /* 5. Golfer */
    if (gs->mode == MODE_PLAY && s_drawable_count < MAX_DRAWABLES) {
        DrawableItem *item = &s_drawables[s_drawable_count++];
        item->type = DRAWABLE_GOLFER;
        item->sort_key = FROM_FIXED(FMUL(gs->golfer.x + gs->golfer.y, TO_FIXED(32)));
        world_to_screen_fixed(gs->golfer.x, gs->golfer.y, gs->golfer.current_z, rc->camera_x, rc->camera_y, &item->foot_sx, &item->foot_sy);
    }
    
    /* 6. Ball */
    if (gs->mode == MODE_PLAY && s_drawable_count < MAX_DRAWABLES) {
        DrawableItem *item = &s_drawables[s_drawable_count++];
        item->type = DRAWABLE_BALL;
        item->sort_key = FROM_FIXED(FMUL(gs->ball.x + gs->ball.y, TO_FIXED(32)));
        world_to_screen_fixed(gs->ball.x, gs->ball.y, gs->ball.ground_z, rc->camera_x, rc->camera_y, &item->foot_sx, &item->foot_sy);
    }
    
    /* 7. Beverage Cart Girl */
    if (gs->bev_cart.active && s_drawable_count < MAX_DRAWABLES) {
        DrawableItem *item = &s_drawables[s_drawable_count++];
        item->type = DRAWABLE_BEV_CART;
        item->sort_key = FROM_FIXED(FMUL(gs->bev_cart.x + gs->bev_cart.y, TO_FIXED(32)));
        world_to_screen_fixed(gs->bev_cart.x, gs->bev_cart.y, gs->bev_cart.current_z, rc->camera_x, rc->camera_y, &item->foot_sx, &item->foot_sy);
    }
    
    /* 8. Gopher Holes (Caddyshack Hazards) */
    for (int i = 0; i < gs->gopher.hole_count; ++i) {
        if (!gs->gopher.holes[i].active) continue;
        if (s_drawable_count < MAX_DRAWABLES) {
            DrawableItem *item = &s_drawables[s_drawable_count++];
            item->type = DRAWABLE_GOPHER_HOLE;
            item->gx = gs->gopher.holes[i].gx;
            item->gy = gs->gopher.holes[i].gy;
            item->data = i;
            item->sort_key = (item->gx + item->gy) * 32 + 8;
            grid_to_screen(item->gx, item->gy, gs->grid[item->gx][item->gy].elevation,
                           rc->camera_x, rc->camera_y, &item->foot_sx, &item->foot_sy);
            item->foot_sx += TILE_HALF_W;
            item->foot_sy += TILE_HALF_H;
        }
    }
    
    /* 9. Active Caddyshack Gopher */
    if (gs->mode == MODE_PLAY && gs->gopher.visible && s_drawable_count < MAX_DRAWABLES) {
        DrawableItem *item = &s_drawables[s_drawable_count++];
        item->type = DRAWABLE_GOPHER;
        item->gx = gs->gopher.gx;
        item->gy = gs->gopher.gy;
        item->data = gs->gopher.anim_frame;
        item->sort_key = FROM_FIXED(FMUL(gs->gopher.x + gs->gopher.y, TO_FIXED(32))) + 12;
        world_to_screen_fixed(gs->gopher.x, gs->gopher.y, gs->gopher.current_z, rc->camera_x, rc->camera_y, &item->foot_sx, &item->foot_sy);
    }
    
    /* 10. Carl Spackler Greenskeeper */
    if (gs->mode == MODE_PLAY && gs->carl.active && s_drawable_count < MAX_DRAWABLES) {
        DrawableItem *item = &s_drawables[s_drawable_count++];
        item->type = DRAWABLE_CARL;
        item->gx = FROM_FIXED(gs->carl.x);
        item->gy = FROM_FIXED(gs->carl.y);
        item->data = (int32_t)gs->carl.state;
        item->sort_key = FROM_FIXED(FMUL(gs->carl.x + gs->carl.y, TO_FIXED(32))) + 14;
        world_to_screen_fixed(gs->carl.x, gs->carl.y, gs->carl.current_z, rc->camera_x, rc->camera_y, &item->foot_sx, &item->foot_sy);
    }
    
    /* 11. Carl's TNT Plunger Box */
    if (gs->mode == MODE_PLAY && gs->carl.active &&
        (gs->carl.state == CARL_STATE_DYNAMITE_PLANT || gs->carl.state == CARL_STATE_DYNAMITE_PLUNGE ||
         (gs->carl.state == CARL_STATE_CELEBRATING && gs->carl.attack_type == CARL_ATTACK_DYNAMITE)) &&
        s_drawable_count < MAX_DRAWABLES) {
        DrawableItem *item = &s_drawables[s_drawable_count++];
        item->type = DRAWABLE_TNT_BOX;
        item->gx = FROM_FIXED(gs->carl.plunger_x);
        item->gy = FROM_FIXED(gs->carl.plunger_y);
        item->data = (gs->carl.state == CARL_STATE_DYNAMITE_PLUNGE && gs->carl.timer >= 14) || gs->carl.state == CARL_STATE_CELEBRATING ? 1 : 0;
        item->sort_key = FROM_FIXED(FMUL(gs->carl.plunger_x + gs->carl.plunger_y, TO_FIXED(32))) + 10;
        world_to_screen_fixed(gs->carl.plunger_x, gs->carl.plunger_y,
                              golf_get_ground_z_fixed(gs, gs->carl.plunger_x, gs->carl.plunger_y),
                              rc->camera_x, rc->camera_y, &item->foot_sx, &item->foot_sy);
    }
    
    /* 12. Tumbleweeds in La Playa */
    if (gs->theme == THEME_LA_PLAYA) {
        for (int t = 0; t < MAX_TUMBLEWEEDS; ++t) {
            const TumbleweedEntity *tw = &gs->tumbleweeds[t];
            if (!tw->active) continue;
            if (s_drawable_count < MAX_DRAWABLES) {
                DrawableItem *item = &s_drawables[s_drawable_count++];
                item->type = DRAWABLE_TUMBLEWEED;
                item->data = tw->rotation;
                item->sort_key = FROM_FIXED(FMUL(tw->x + tw->y, TO_FIXED(32))) + 8;
                world_to_screen_fixed(tw->x, tw->y, tw->current_z, rc->camera_x, rc->camera_y, &item->foot_sx, &item->foot_sy);
            }
        }
    }
    
    /* 13. Sea Monster in Nocturnal Green */
    if (gs->theme == THEME_NOCTURNAL_GREEN && gs->sea_monster.active && s_drawable_count < MAX_DRAWABLES) {
        DrawableItem *item = &s_drawables[s_drawable_count++];
        item->type = DRAWABLE_SEA_MONSTER;
        item->data = gs->sea_monster.state;
        item->sort_key = FROM_FIXED(FMUL(gs->sea_monster.x + gs->sea_monster.y, TO_FIXED(32))) + 10;
        world_to_screen_fixed(gs->sea_monster.x, gs->sea_monster.y, gs->sea_monster.current_z, rc->camera_x, rc->camera_y, &item->foot_sx, &item->foot_sy);
    }
    
    /* 14. Course Ranger in Sneak On Mode */
    if (gs->is_sneak_on && gs->ranger.active && s_drawable_count < MAX_DRAWABLES) {
        DrawableItem *item = &s_drawables[s_drawable_count++];
        item->type = DRAWABLE_RANGER;
        item->sort_key = FROM_FIXED(FMUL(gs->ranger.x + gs->ranger.y, TO_FIXED(32))) + 12;
        world_to_screen_fixed(gs->ranger.x, gs->ranger.y, gs->ranger.current_z, rc->camera_x, rc->camera_y, &item->foot_sx, &item->foot_sy);
    }
    
    /* Sort all drawables back-to-front by sort_key */
    qsort(s_drawables, s_drawable_count, sizeof(DrawableItem), compare_drawables);
    
    /* ===================================================================== */
    /* PASS 3: RENDER SORTED DRAWABLES WITH DEPTH OCCLUSION                  */
    /* ===================================================================== */
    for (int i = 0; i < s_drawable_count; ++i) {
        const DrawableItem *item = &s_drawables[i];
        
        switch (item->type) {
            case DRAWABLE_CLUBHOUSE: {
                if (gs->theme == THEME_LA_PLAYA) {
                    render_burning_man_effigy(rc, item->foot_sx, item->foot_sy, rc->tick_count);
                } else if (gs->theme == THEME_NOCTURNAL_GREEN) {
                    render_transylvania_castle(rc, item->foot_sx, item->foot_sy);
                } else {
                    render_clubhouse(rc, item->foot_sx, item->foot_sy);
                }
                break;
            }
            case DRAWABLE_FEATURE: {
                int feat = item->data;
                int fx = item->foot_sx;
                int fy = item->foot_sy;
                
                if (gs->theme == THEME_LA_PLAYA) {
                    if (feat == FEAT_PINE || feat == FEAT_OAK || feat == FEAT_PALM || feat == FEAT_FLOWERING) {
                        render_cactus(rc, fx, fy, item->sort_key);
                    } else if (feat == FEAT_BUSH) {
                        render_tumbleweed(rc, fx, fy, 0, item->sort_key);
                    } else if (feat == FEAT_ROCK) {
                        render_desert_monolith_rock(rc, fx, fy, item->sort_key);
                    } else if (feat == FEAT_FLOWERS) {
                        blit_sprite_depth(rc, fx - SPRITE_FLOWERS_W / 2, fy - SPRITE_FLOWERS_H + 2,
                                          SPRITE_FLOWERS_W, SPRITE_FLOWERS_H, s_sprite_flowers, item->sort_key);
                    } else if (feat == FEAT_BENCH) {
                        draw_drop_shadow(rc, fx, fy, 14, 6, 110);
                        blit_sprite_depth(rc, fx - SPRITE_BENCH_W / 2, fy - SPRITE_BENCH_H + 2,
                                          SPRITE_BENCH_W, SPRITE_BENCH_H, s_sprite_bench, item->sort_key);
                    } else if (feat == FEAT_CART) {
                        draw_drop_shadow(rc, fx, fy, 22, 10, 140);
                        blit_sprite_depth(rc, fx - SPRITE_CART_W / 2, fy - SPRITE_CART_H + 4,
                                          SPRITE_CART_W, SPRITE_CART_H, s_sprite_cart, item->sort_key);
                    }
                } else if (gs->theme == THEME_NOCTURNAL_GREEN) {
                    if (feat == FEAT_PINE || feat == FEAT_OAK || feat == FEAT_PALM || feat == FEAT_FLOWERING) {
                        render_autumn_tree(rc, fx, fy, feat, item->sort_key);
                    } else if (feat == FEAT_BUSH) {
                        render_sickly_bush(rc, fx, fy, item->sort_key);
                    } else if (feat == FEAT_ROCK) {
                        render_coffin_pile(rc, fx, fy, item->sort_key);
                    } else if (feat == FEAT_FLOWERS) {
                        blit_sprite_depth(rc, fx - SPRITE_FLOWERS_W / 2, fy - SPRITE_FLOWERS_H + 2,
                                          SPRITE_FLOWERS_W, SPRITE_FLOWERS_H, s_sprite_flowers, item->sort_key);
                    } else if (feat == FEAT_BENCH) {
                        draw_drop_shadow(rc, fx, fy, 14, 6, 110);
                        blit_sprite_depth(rc, fx - SPRITE_BENCH_W / 2, fy - SPRITE_BENCH_H + 2,
                                          SPRITE_BENCH_W, SPRITE_BENCH_H, s_sprite_bench, item->sort_key);
                    } else if (feat == FEAT_CART) {
                        draw_drop_shadow(rc, fx, fy, 22, 10, 140);
                        blit_sprite_depth(rc, fx - SPRITE_CART_W / 2, fy - SPRITE_CART_H + 4,
                                          SPRITE_CART_W, SPRITE_CART_H, s_sprite_cart, item->sort_key);
                    }
                } else {
                    /* Bushwood Theme */
                    if (feat == FEAT_PINE) {
                        draw_drop_shadow(rc, fx, fy, 18, 9, 130);
                        blit_sprite_depth(rc, fx - SPRITE_PINE_W / 2, fy - SPRITE_PINE_H + 4,
                                          SPRITE_PINE_W, SPRITE_PINE_H, s_sprite_pine, item->sort_key);
                    } else if (feat == FEAT_OAK) {
                        draw_drop_shadow(rc, fx, fy, 22, 11, 140);
                        blit_sprite_depth(rc, fx - SPRITE_OAK_W / 2, fy - SPRITE_OAK_H + 6,
                                          SPRITE_OAK_W, SPRITE_OAK_H, s_sprite_oak, item->sort_key);
                    } else if (feat == FEAT_PALM) {
                        draw_drop_shadow(rc, fx, fy, 16, 8, 120);
                        blit_sprite_depth(rc, fx - SPRITE_PALM_W / 2, fy - SPRITE_PALM_H + 4,
                                          SPRITE_PALM_W, SPRITE_PALM_H, s_sprite_palm, item->sort_key);
                    } else if (feat == FEAT_FLOWERING) {
                        draw_drop_shadow(rc, fx, fy, 20, 10, 130);
                        blit_sprite_depth(rc, fx - SPRITE_FLOWERING_W / 2, fy - SPRITE_FLOWERING_H + 4,
                                          SPRITE_FLOWERING_W, SPRITE_FLOWERING_H, s_sprite_flowering, item->sort_key);
                    } else if (feat == FEAT_BUSH) {
                        draw_drop_shadow(rc, fx, fy, 14, 6, 120);
                        blit_sprite_depth(rc, fx - SPRITE_BUSH_W / 2, fy - SPRITE_BUSH_H + 2,
                                          SPRITE_BUSH_W, SPRITE_BUSH_H, s_sprite_bush, item->sort_key);
                    } else if (feat == FEAT_FLOWERS) {
                        blit_sprite_depth(rc, fx - SPRITE_FLOWERS_W / 2, fy - SPRITE_FLOWERS_H + 2,
                                          SPRITE_FLOWERS_W, SPRITE_FLOWERS_H, s_sprite_flowers, item->sort_key);
                    } else if (feat == FEAT_ROCK) {
                        draw_drop_shadow(rc, fx, fy, 16, 7, 130);
                        blit_sprite_depth(rc, fx - SPRITE_ROCK_W / 2, fy - SPRITE_ROCK_H + 2,
                                          SPRITE_ROCK_W, SPRITE_ROCK_H, s_sprite_rock, item->sort_key);
                    } else if (feat == FEAT_BENCH) {
                        draw_drop_shadow(rc, fx, fy, 14, 6, 110);
                        blit_sprite_depth(rc, fx - SPRITE_BENCH_W / 2, fy - SPRITE_BENCH_H + 2,
                                          SPRITE_BENCH_W, SPRITE_BENCH_H, s_sprite_bench, item->sort_key);
                    } else if (feat == FEAT_CART) {
                        draw_drop_shadow(rc, fx, fy, 22, 10, 140);
                        blit_sprite_depth(rc, fx - SPRITE_CART_W / 2, fy - SPRITE_CART_H + 4,
                                          SPRITE_CART_W, SPRITE_CART_H, s_sprite_cart, item->sort_key);
                    }
                }
                break;
            }
            case DRAWABLE_FLAGSTICK: {
                int fx = item->foot_sx;
                int fy = item->foot_sy;
                int anim_f = (rc->tick_count / 10) % 4;
                blit_sprite_depth(rc, fx - 4, fy - SPRITE_FLAG_H + 4,
                                  SPRITE_FLAG_W, SPRITE_FLAG_H, s_sprite_flag[anim_f], item->sort_key);
                break;
            }
            case DRAWABLE_SPECTATOR: {
                int s_idx = item->data;
                const SpectatorEntity *sp = &gs->spectators[s_idx];
                int fx = item->foot_sx;
                int fy = item->foot_sy;
                int anim_f = (sp->state == SPEC_STATE_CHEERING) ? (sp->anim_frame % 4) : 0;
                
                draw_drop_shadow(rc, fx, fy, 8, 4, 120);
                blit_sprite_depth(rc, fx - SPRITE_SPEC_W / 2, fy - SPRITE_SPEC_H + 4,
                                  SPRITE_SPEC_W, SPRITE_SPEC_H,
                                  s_sprite_spec[sp->shirt_color_idx % 5][anim_f], item->sort_key);
                break;
            }
            case DRAWABLE_CADDY: {
                int fx = item->foot_sx;
                int fy = item->foot_sy;
                if (gs->caddy_eaten) {
                    draw_drop_shadow(rc, fx, fy, 10, 5, 120);
                    draw_rect(rc, fx - 6, fy - 18, 12, 18, COLOR_RGB(120, 125, 135));
                    draw_rect(rc, fx - 4, fy - 14, 8, 2, COLOR_RGB(55, 60, 70));
                    draw_rect(rc, fx - 1, fy - 16, 2, 6, COLOR_RGB(55, 60, 70));
                    draw_text(rc, fx - 18, fy - 28, "RIP CADDY", COLOR_RGB(255, 120, 120), true);
                } else {
                    const uint32_t *c_spr = s_sprite_caddy;
                    if (gs->caddy.state == CADDY_STATE_FOLLOWING) {
                        c_spr = s_sprite_caddy_walk[gs->caddy.walk_anim_frame % 6];
                    } else if (gs->caddy.state == CADDY_STATE_CELEBRATING) {
                        c_spr = s_sprite_caddy_celebrate;
                    }
                    draw_drop_shadow(rc, fx, fy, 9, 4, 125);
                    blit_sprite_depth(rc, fx - SPRITE_CADDY_W / 2, fy - SPRITE_CADDY_H + 4,
                                      SPRITE_CADDY_W, SPRITE_CADDY_H, c_spr, item->sort_key);
                }
                break;
            }
            case DRAWABLE_GOLFER: {
                int fx = item->foot_sx;
                int fy = item->foot_sy;
                const uint32_t *g_spr = s_sprite_golfer;
                if (gs->golfer.state == GOLFER_STATE_WALKING) {
                    g_spr = s_sprite_golfer_walk[gs->golfer.walk_anim_frame % 6];
                } else if (gs->golfer.state == GOLFER_STATE_JUMPING) {
                    g_spr = s_sprite_golfer_jump;
                } else if (gs->golfer.state == GOLFER_STATE_DRINKING) {
                    g_spr = s_sprite_golfer_drink;
                }
                
                /* For jumping golfer, drop shadow stays grounded on terrain below */
                int shadow_sx = fx, shadow_sy = fy;
                if (gs->golfer.state == GOLFER_STATE_JUMPING) {
                    fixed ground_z = golf_get_ground_z_fixed(gs, gs->golfer.x, gs->golfer.y);
                    world_to_screen_fixed(gs->golfer.x, gs->golfer.y, ground_z, rc->camera_x, rc->camera_y, &shadow_sx, &shadow_sy);
                }
                draw_drop_shadow(rc, shadow_sx, shadow_sy, 9, 4, 130);
                blit_sprite_depth(rc, fx - SPRITE_GOLFER_W / 2, fy - SPRITE_GOLFER_H + 4,
                                  SPRITE_GOLFER_W, SPRITE_GOLFER_H, g_spr, item->sort_key);
                break;
            }
            case DRAWABLE_BALL: {
                int bsx = item->foot_sx;
                int bsy = item->foot_sy;
                
                /* Ground Drop Shadow */
                draw_drop_shadow(rc, bsx, bsy, 3, 2, 140);
                
                /* Elevated Ball */
                int ball_screen_y = bsy - FROM_FIXED(gs->ball.z - gs->ball.ground_z);
                for (int dy = -2; dy <= 2; ++dy) {
                    for (int dx = -2; dx <= 2; ++dx) {
                        if (dx * dx + dy * dy <= 4) {
                            int px = bsx + dx;
                            int py = ball_screen_y + dy;
                            if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
                                uint32_t bcol = (dx == -1 && dy == -1) ? COLOR_RGB(255, 255, 255) : COLOR_RGB(220, 225, 230);
                                rc->framebuffer[py * SCREEN_W + px] = bcol;
                            }
                        }
                    }
                }
                break;
            }
            case DRAWABLE_BEV_CART: {
                int fx = item->foot_sx;
                int fy = item->foot_sy;
                draw_drop_shadow(rc, fx, fy, 22, 10, 140);
                blit_sprite_depth(rc, fx - SPRITE_BEV_CART_W / 2, fy - SPRITE_BEV_CART_H + 4,
                                  SPRITE_BEV_CART_W, SPRITE_BEV_CART_H, s_sprite_bev_cart, item->sort_key);
                
                /* Floating speech bubble when whistling or selling drinks */
                if (gs->bev_cart.speech_timer > 0) {
                    const char *bmsg = "COLD DRINKS!";
                    int bub_w = 124;
                    int bub_h = 22;
                    int bub_x = fx - bub_w / 2;
                    int bub_y = fy - SPRITE_BEV_CART_H - 26;
                    /* White speech bubble with dark border */
                    draw_rect(rc, bub_x, bub_y, bub_w, bub_h, COLOR_RGB(255, 255, 255));
                    draw_rect_border(rc, bub_x, bub_y, bub_w, bub_h, COLOR_RGB(35, 45, 55));
                    /* Speech pointer downward to cart */
                    for (int dy = 0; dy < 5; ++dy) {
                        draw_rect(rc, fx - 2 + dy / 2, bub_y + bub_h + dy, 5 - dy, 1, COLOR_RGB(255, 255, 255));
                    }
                    int b_tx = bub_x + (bub_w - (int)strlen(bmsg) * 8) / 2;
                    draw_text(rc, b_tx, bub_y + 7, bmsg, COLOR_RGB(220, 20, 70), false);
                }
                break;
            }
            case DRAWABLE_GOPHER_HOLE: {
                int fx = item->foot_sx;
                int fy = item->foot_sy;
                blit_sprite_depth(rc, fx - SPRITE_GOPHER_HOLE_W / 2, fy - SPRITE_GOPHER_HOLE_H / 2,
                                  SPRITE_GOPHER_HOLE_W, SPRITE_GOPHER_HOLE_H, s_sprite_gopher_hole, item->sort_key);
                break;
            }
            case DRAWABLE_GOPHER: {
                int fx = item->foot_sx;
                int fy = item->foot_sy;
                if (gs->theme == THEME_LA_PLAYA) {
                    render_raver(rc, fx, fy, gs->raver_passed_out_timer, rc->tick_count, item->sort_key);
                } else if (gs->theme == THEME_NOCTURNAL_GREEN) {
                    render_zombie(rc, fx, fy, item->data, item->sort_key);
                } else {
                    int anim_f = item->data % 4;
                    blit_sprite_depth(rc, fx - SPRITE_GOPHER_W / 2, fy - SPRITE_GOPHER_H + 4,
                                      SPRITE_GOPHER_W, SPRITE_GOPHER_H, s_sprite_gopher[anim_f], item->sort_key);
                    if (gs->gopher.state == GOPHER_STATE_TAUNTING) {
                        int note_y = fy - SPRITE_GOPHER_H - 4 - (int)((rc->tick_count / 10) % 4);
                        draw_text(rc, fx - 24, note_y, "HEE HEE!", COLOR_RGB(255, 235, 95), true);
                    }
                }
                break;
            }
            case DRAWABLE_TNT_BOX: {
                int fx = item->foot_sx;
                int fy = item->foot_sy;
                int plunger_down = item->data;
                
                draw_drop_shadow(rc, fx, fy, 8, 4, 120);
                blit_sprite_depth(rc, fx - SPRITE_TNT_BOX_W / 2, fy - SPRITE_TNT_BOX_H + 2,
                                  SPRITE_TNT_BOX_W, SPRITE_TNT_BOX_H, s_sprite_tnt_box[plunger_down], item->sort_key);
                
                int hole_sx = FROM_FIXED(FMUL(TO_FIXED(gs->carl.target_gx) - TO_FIXED(gs->carl.target_gy), TO_FIXED(TILE_HALF_W))) + rc->camera_x + TILE_HALF_W;
                int hole_sy = FROM_FIXED(FMUL(TO_FIXED(gs->carl.target_gx) + TO_FIXED(gs->carl.target_gy), TO_FIXED(TILE_HALF_H))) -
                              FROM_FIXED(golf_get_ground_z_fixed(gs, TO_FIXED(gs->carl.target_gx) + FIXED_HALF, TO_FIXED(gs->carl.target_gy) + FIXED_HALF)) + rc->camera_y + TILE_HALF_H;
                
                int steps = 24;
                for (int s = 0; s <= steps; ++s) {
                    float t = (float)s / (float)steps;
                    int wx = (int)(fx + t * (hole_sx - fx));
                    int wy = (int)(fy + t * (hole_sy - fy) + sinf(t * (float)M_PI) * 4.0f);
                    if (wx >= 0 && wx < SCREEN_W && wy >= 0 && wy < SCREEN_H) {
                        rc->framebuffer[wy * SCREEN_W + wx] = ((s & 3) == 0) ? COLOR_RGB(220, 45, 45) : COLOR_RGB(30, 25, 25);
                    }
                }
                break;
            }
            case DRAWABLE_CARL: {
                int fx = item->foot_sx;
                int fy = item->foot_sy;
                if (gs->theme == THEME_LA_PLAYA) {
                    render_police_cart(rc, fx, fy, rc->tick_count, item->sort_key);
                } else if (gs->theme == THEME_NOCTURNAL_GREEN) {
                    render_ash_williams(rc, fx, fy, gs->carl.state, rc->tick_count, item->sort_key);
                } else {
                    const uint32_t *carl_spr = s_sprite_carl_walk[gs->carl.anim_frame % 4];
                    
                    switch (gs->carl.state) {
                        case CARL_STATE_APPROACHING:
                        case CARL_STATE_LEAVING:
                            carl_spr = s_sprite_carl_walk[gs->carl.anim_frame % 4];
                            break;
                        case CARL_STATE_DYNAMITE_PLANT:
                            carl_spr = s_sprite_carl_plant[gs->carl.anim_frame % 2];
                            break;
                        case CARL_STATE_DYNAMITE_PLUNGE:
                            carl_spr = s_sprite_carl_plunge[gs->carl.anim_frame % 2];
                            break;
                        case CARL_STATE_HOSE_SPRAY:
                            carl_spr = s_sprite_carl_hose[gs->carl.anim_frame % 2];
                            break;
                        case CARL_STATE_CELEBRATING:
                            carl_spr = s_sprite_carl_celebrate[gs->carl.anim_frame % 2];
                            break;
                        default:
                            break;
                    }
                    
                    draw_drop_shadow(rc, fx, fy, 10, 5, 135);
                    blit_sprite_depth(rc, fx - SPRITE_CARL_W / 2, fy - SPRITE_CARL_H + 4,
                                      SPRITE_CARL_W, SPRITE_CARL_H, carl_spr, item->sort_key);
                    
                    int hole_sx = FROM_FIXED(FMUL(TO_FIXED(gs->carl.target_gx) - TO_FIXED(gs->carl.target_gy), TO_FIXED(TILE_HALF_W))) + rc->camera_x + TILE_HALF_W;
                    int hole_sy = FROM_FIXED(FMUL(TO_FIXED(gs->carl.target_gx) + TO_FIXED(gs->carl.target_gy), TO_FIXED(TILE_HALF_H))) -
                                  FROM_FIXED(golf_get_ground_z_fixed(gs, TO_FIXED(gs->carl.target_gx) + FIXED_HALF, TO_FIXED(gs->carl.target_gy) + FIXED_HALF)) + rc->camera_y + TILE_HALF_H;
                    
                    if (gs->carl.state == CARL_STATE_HOSE_SPRAY) {
                        int nozzle_sx = fx + 7;
                        int nozzle_sy = fy - 18;
                        for (int stream = -1; stream <= 1; ++stream) {
                            int steps = 30;
                            for (int s = 0; s <= steps; ++s) {
                                float t = (float)s / (float)steps;
                                int wx = (int)(nozzle_sx + t * (hole_sx - nozzle_sx) + stream * (1.0f - t) * 1.5f);
                                int wy = (int)(nozzle_sy + t * (hole_sy - nozzle_sy) - sinf(t * (float)M_PI) * 12.0f);
                                if (wx >= 0 && wx < SCREEN_W && wy >= 0 && wy < SCREEN_H) {
                                    uint32_t wcol = ((s + rc->tick_count * 2) % 4 == 0) ? COLOR_RGB(255, 255, 255) : COLOR_RGB(80, 185, 245);
                                    rc->framebuffer[wy * SCREEN_W + wx] = wcol;
                                    if (wy + 1 < SCREEN_H && (s % 2 == 0)) {
                                        rc->framebuffer[(wy + 1) * SCREEN_W + wx] = COLOR_RGB(45, 140, 215);
                                    }
                                }
                            }
                        }
                        for (int sp = 0; sp < 12; ++sp) {
                            int f_dx = ((sp * 7 + rc->tick_count * 5) % 25) - 12;
                            int f_dy = ((sp * 11 + rc->tick_count * 3) % 15) - 7;
                            int px = hole_sx + f_dx;
                            int py = hole_sy + f_dy;
                            if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
                                rc->framebuffer[py * SCREEN_W + px] = (sp % 2 == 0) ? COLOR_RGB(255, 255, 255) : COLOR_RGB(130, 220, 255);
                            }
                        }
                    }
                    
                    if (gs->carl.state == CARL_STATE_DYNAMITE_PLUNGE && gs->carl.timer >= 14 && gs->carl.timer <= 48) {
                        int blast_t = gs->carl.timer - 14;
                        int max_rad = 12 + blast_t * 2;
                        if (max_rad > 38) max_rad = 38;
                        
                        for (int dy = -max_rad; dy <= max_rad; ++dy) {
                            for (int dx = -max_rad; dx <= max_rad; ++dx) {
                                int r2 = dx * dx + dy * dy;
                                if (r2 <= max_rad * max_rad) {
                                    int px = hole_sx + dx;
                                    int py = hole_sy - 12 + dy;
                                    if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
                                        int dist = (int)sqrtf((float)r2);
                                        if (dist < max_rad / 3) {
                                            rc->framebuffer[py * SCREEN_W + px] = COLOR_RGB(255, 250, 190);
                                        } else if (dist < (max_rad * 2) / 3) {
                                            rc->framebuffer[py * SCREEN_W + px] = COLOR_RGB(255, 140, 25);
                                        } else {
                                            if (((dx * 7 + dy * 13 + blast_t) & 3) != 0) {
                                                rc->framebuffer[py * SCREEN_W + px] = COLOR_RGB(75, 68, 62);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        for (int p = 0; p < 16; ++p) {
                            float angle = (float)p * 0.3926f + (float)blast_t * 0.1f;
                            float spd = 4.0f + (float)(p % 5) * 2.5f;
                            int clod_x = hole_sx + (int)(cosf(angle) * spd * (float)blast_t * 0.7f);
                            int clod_y = hole_sy - 12 + (int)(sinf(angle) * spd * (float)blast_t * 0.4f + 0.35f * blast_t * blast_t);
                            for (int cdy = 0; cdy <= 1; ++cdy) {
                                for (int cdx = 0; cdx <= 1; ++cdx) {
                                    int px = clod_x + cdx;
                                    int py = clod_y + cdy;
                                    if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
                                        rc->framebuffer[py * SCREEN_W + px] = COLOR_RGB(55, 38, 25);
                                    }
                                }
                            }
                        }
                    }
                    
                    const char *cmsg = NULL;
                    if (gs->carl.state == CARL_STATE_DYNAMITE_PLUNGE) {
                        cmsg = "DIE GOPHER! lol";
                    } else if (gs->carl.state == CARL_STATE_HOSE_SPRAY) {
                        cmsg = "HAVE SOME WATER! lol";
                    } else if (gs->carl.state == CARL_STATE_CELEBRATING) {
                        cmsg = (gs->carl.attack_type == CARL_ATTACK_DYNAMITE) ? "CINDERELLA STORY! lol" : "IN THE HOLE! lol";
                    }
                    
                    if (cmsg != NULL) {
                        int bub_w = (int)strlen(cmsg) * 8 + 16;
                        int bub_h = 22;
                        int bub_x = fx - bub_w / 2;
                        int bub_y = fy - SPRITE_CARL_H - 24;
                        draw_rect(rc, bub_x, bub_y, bub_w, bub_h, COLOR_RGB(255, 255, 255));
                        draw_rect_border(rc, bub_x, bub_y, bub_w, bub_h, COLOR_RGB(35, 45, 55));
                        for (int dy = 0; dy < 5; ++dy) {
                            draw_rect(rc, fx - 2 + dy / 2, bub_y + bub_h + dy, 5 - dy, 1, COLOR_RGB(255, 255, 255));
                        }
                        int b_tx = bub_x + 8;
                        draw_text(rc, b_tx, bub_y + 7, cmsg, COLOR_RGB(25, 40, 60), false);
                    }
                }
                break;
            }
            case DRAWABLE_TUMBLEWEED: {
                render_tumbleweed(rc, item->foot_sx, item->foot_sy, item->data, item->sort_key);
                break;
            }
            case DRAWABLE_SEA_MONSTER: {
                render_sea_monster(rc, item->foot_sx, item->foot_sy, item->data, rc->tick_count, item->sort_key);
                break;
            }
            case DRAWABLE_RANGER: {
                render_ranger(rc, gs, item->foot_sx, item->foot_sy, item->sort_key);
                break;
            }
        }
    }
    
    /* 3. Aim Line & Reticle in Play Mode */
    if (gs->mode == MODE_PLAY && gs->ball.state == BALL_IDLE) {
        int bsx, bsy;
        world_to_screen_fixed(gs->ball.x, gs->ball.y, gs->ball.ground_z, rc->camera_x, rc->camera_y, &bsx, &bsy);
        
        fixed gz_aim = golf_get_ground_z_fixed(gs, gs->aim_target_x, gs->aim_target_y);
        int asx, asy;
        world_to_screen_fixed(gs->aim_target_x, gs->aim_target_y, gz_aim, rc->camera_x, rc->camera_y, &asx, &asy);
        
        /* Dashed Line */
        int steps = 24;
        for (int i = 0; i <= steps; ++i) {
            if ((i % 2) == 0) continue;
            int px = bsx + ((asx - bsx) * i) / steps;
            int py = bsy + ((asy - bsy) * i) / steps;
            if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
                rc->framebuffer[py * SCREEN_W + px] = COLOR_RGB(255, 235, 90);
            }
        }
        
        /* Landing Reticle Ring */
        for (int a = 0; a < 256; a += 16) {
            int rx = asx + FROM_FIXED(FMUL(TO_FIXED(8), fixed_cos((uint8_t)a)));
            int ry = asy + FROM_FIXED(FMUL(TO_FIXED(4), fixed_sin((uint8_t)a)));
            if (rx >= 0 && rx < SCREEN_W && ry >= 0 && ry < SCREEN_H) {
                rc->framebuffer[ry * SCREEN_W + rx] = COLOR_RGB(255, 235, 90);
            }
        }
        
        char aim_buf[32];
        snprintf(aim_buf, sizeof(aim_buf), "%d YDS", gs->aim_distance_yds);
        draw_text(rc, asx - 18, asy - 14, aim_buf, COLOR_RGB(255, 245, 120), true);
    }
    
    /* 4. Minimap Radar */
    render_minimap(rc, gs);
    
    /* 5. Modern Glassmorphism HUD Overlay */
    /* Top Status Bar (0..46 px) */
    draw_rect_alpha(rc, 0, 0, SCREEN_W, 46, COLOR_RGB(16, 24, 32), 225);
    draw_rect(rc, 0, 46, SCREEN_W, 1, COLOR_RGB(65, 85, 110));
    
    /* Title Badge */
    draw_text_2x(rc, 14, 8, "DUFFER PRO", COLOR_RGB(235, 195, 65), true);
    draw_text(rc, 14, 30, "CHAMPIONSHIP GOLF CLUB", COLOR_RGB(155, 185, 215), false);
    
    /* Hole & Score Panel */
    char stat_buf[128];
    snprintf(stat_buf, sizeof(stat_buf), "HOLE 1  PAR %d  %d YDS", gs->par, gs->hole_yards);
    draw_text(rc, 220, 10, stat_buf, COLOR_RGB(240, 245, 250), true);
    char stroke_buf[64];
    snprintf(stroke_buf, sizeof(stroke_buf), "STROKE %d", gs->strokes);
    draw_text(rc, 220, 26, stroke_buf, COLOR_RGB(200, 220, 240), true);
    
    /* SimGolf Fun Rating Stars */
    int stars = (gs->fun_rating * 5) / 100;
    char star_buf[64];
    snprintf(star_buf, sizeof(star_buf), "FUN: %d/5 STARS (%d%%)", stars, gs->fun_rating);
    draw_text(rc, 425, 10, star_buf, COLOR_RGB(255, 215, 65), true);
    
    /* Economy Funds */
    char fund_buf[64];
    snprintf(fund_buf, sizeof(fund_buf), "FUNDS: $%d", gs->club_funds);
    draw_text(rc, 425, 26, fund_buf, COLOR_RGB(115, 225, 115), true);
    
    /* Top Bar Buttons: Replay Hole, Randomize Hole and Mode Selector */
    int btn_replay_x = SCREEN_W - 410;
    draw_rect_alpha(rc, btn_replay_x, 8, 125, 30, COLOR_RGB(35, 65, 55), 210);
    draw_rect_border(rc, btn_replay_x, 8, 125, 30, COLOR_RGB(95, 175, 135));
    draw_text(rc, btn_replay_x + 18, 17, "REPLAY HOLE", COLOR_RGB(255, 255, 255), true);
    
    int btn_rand_x = SCREEN_W - 275;
    draw_rect_alpha(rc, btn_rand_x, 8, 125, 30, COLOR_RGB(35, 55, 75), 210);
    draw_rect_border(rc, btn_rand_x, 8, 125, 30, COLOR_RGB(85, 125, 165));
    draw_text(rc, btn_rand_x + 10, 17, "RANDOM HOLE", COLOR_RGB(255, 255, 255), true);
    
    int btn_mode_x = SCREEN_W - 140;
    uint32_t mode_col = (gs->mode == MODE_PLAY) ? COLOR_RGB(45, 135, 65) : COLOR_RGB(175, 115, 35);
    draw_rect_alpha(rc, btn_mode_x, 8, 125, 30, mode_col, 225);
    draw_rect_border(rc, btn_mode_x, 8, 125, 30, COLOR_RGB(255, 255, 255));
    draw_text(rc, btn_mode_x + 18, 17, (gs->mode == MODE_PLAY) ? "PLAY MODE" : "DESIGN MODE", COLOR_RGB(255, 255, 255), true);
    
    /* 6. Bottom Control Deck (Height 70 px) */
    int btm_y = SCREEN_H - 70;
    draw_rect_alpha(rc, 0, btm_y, SCREEN_W, 70, COLOR_RGB(16, 24, 32), 235);
    draw_rect(rc, 0, btm_y, SCREEN_W, 1, COLOR_RGB(65, 85, 110));
    
    if (gs->mode == MODE_BUILD) {
        /* Design Mode: Category Tabs at Top of Deck */
        int tab_w = 110;
        for (int c = 0; c < BUILD_CAT_COUNT; ++c) {
            int tx = 10 + c * (tab_w + 6);
            int ty = btm_y - 24;
            bool active = (gs->build_category == (BuildCategory)c);
            draw_rect_alpha(rc, tx, ty, tab_w, 24, active ? COLOR_RGB(45, 75, 105) : COLOR_RGB(25, 35, 45), 220);
            draw_rect_border(rc, tx, ty, tab_w, 24, active ? COLOR_RGB(235, 195, 65) : COLOR_RGB(60, 75, 90));
            const char *cat_names[3] = { "1. TERRAIN", "2. COURSE", "3. SCENERY" };
            draw_text(rc, tx + 12, ty + 7, cat_names[c], active ? COLOR_RGB(255, 255, 255) : COLOR_RGB(165, 175, 185), true);
        }
        
        /* Tool buttons based on active tab */
        int btn_x = 10;
        int btn_y = btm_y + 8;
        int bw = 85;
        int bh = 32;
        
        if (gs->build_category == BUILD_CAT_TERRAIN) {
            BuildTool t_list[8] = { TOOL_FAIRWAY, TOOL_GREEN, TOOL_SAND, TOOL_ROUGH, TOOL_WATER, TOOL_CARTPATH, TOOL_RAISE, TOOL_LOWER };
            const char *t_names[8] = { "FAIRWAY", "GREEN", "SAND", "ROUGH", "WATER", "PATH", "RAISE +", "LOWER -" };
            for (int i = 0; i < 8; ++i) {
                bool sel = (gs->current_tool == t_list[i]);
                draw_rect_alpha(rc, btn_x, btn_y, bw, bh, sel ? COLOR_RGB(55, 105, 75) : COLOR_RGB(28, 42, 54), 210);
                draw_rect_border(rc, btn_x, btn_y, bw, bh, sel ? COLOR_RGB(255, 235, 90) : COLOR_RGB(70, 90, 110));
                draw_text(rc, btn_x + 12, btn_y + 11, t_names[i], COLOR_RGB(255, 255, 255), true);
                btn_x += bw + 8;
            }
            /* Bulldoze button */
            bool sel_b = (gs->current_tool == TOOL_BULLDOZE);
            draw_rect_alpha(rc, btn_x, btn_y, bw, bh, sel_b ? COLOR_RGB(165, 45, 45) : COLOR_RGB(45, 25, 25), 210);
            draw_rect_border(rc, btn_x, btn_y, bw, bh, sel_b ? COLOR_RGB(255, 120, 120) : COLOR_RGB(95, 50, 50));
            draw_text(rc, btn_x + 10, btn_y + 11, "BULLDOZE", COLOR_RGB(255, 255, 255), true);
        } else if (gs->build_category == BUILD_CAT_COURSE) {
            BuildTool t_list[5] = { TOOL_TEE, TOOL_PIN, TOOL_BRIDGE, TOOL_BENCH, TOOL_CART };
            const char *t_names[5] = { "TEE BOX", "CUP / PIN", "BRIDGE", "BENCH", "GOLF CART" };
            for (int i = 0; i < 5; ++i) {
                bool sel = (gs->current_tool == t_list[i]);
                draw_rect_alpha(rc, btn_x, btn_y, bw + 10, bh, sel ? COLOR_RGB(55, 105, 75) : COLOR_RGB(28, 42, 54), 210);
                draw_rect_border(rc, btn_x, btn_y, bw + 10, bh, sel ? COLOR_RGB(255, 235, 90) : COLOR_RGB(70, 90, 110));
                draw_text(rc, btn_x + 10, btn_y + 11, t_names[i], COLOR_RGB(255, 255, 255), true);
                btn_x += bw + 18;
            }
        } else if (gs->build_category == BUILD_CAT_SCENERY) {
            BuildTool t_list[8] = { TOOL_PINE, TOOL_OAK, TOOL_PALM, TOOL_FLOWERING, TOOL_BUSH, TOOL_FLOWERS, TOOL_ROCK, TOOL_CLUBHOUSE };
            const char *t_names[8] = { "PINE", "OAK", "PALM", "CHERRY", "AZALEA", "FLOWERS", "ROCK", "CLUBHOUSE" };
            for (int i = 0; i < 8; ++i) {
                bool sel = (gs->current_tool == t_list[i]);
                int w_btn = (i == 7) ? 95 : bw;
                draw_rect_alpha(rc, btn_x, btn_y, w_btn, bh, sel ? COLOR_RGB(55, 105, 75) : COLOR_RGB(28, 42, 54), 210);
                draw_rect_border(rc, btn_x, btn_y, w_btn, bh, sel ? COLOR_RGB(255, 235, 90) : COLOR_RGB(70, 90, 110));
                draw_text(rc, btn_x + 8, btn_y + 11, t_names[i], COLOR_RGB(255, 255, 255), true);
                btn_x += w_btn + 6;
            }
        }
        
        /* Brush Size toggle right */
        int brush_x = SCREEN_W - 130;
        draw_rect_alpha(rc, brush_x, btn_y, 115, bh, COLOR_RGB(35, 55, 75), 210);
        draw_rect_border(rc, brush_x, btn_y, 115, bh, COLOR_RGB(105, 145, 185));
        char b_buf[32];
        snprintf(b_buf, sizeof(b_buf), "BRUSH: %s", (gs->brush_size == 1) ? "1x1" : "3x3");
        draw_text(rc, brush_x + 12, btn_y + 11, b_buf, COLOR_RGB(255, 255, 255), true);
        
        /* Tooltip Guidance */
        draw_text(rc, 14, btm_y + 48, "LEFT CLICK to paint / sculpt / place  |  RIGHT DRAG to pan camera  |  TAB to switch modes", COLOR_RGB(145, 175, 195), false);
    } else {
        /* Play Mode HUD: Club Card, Aim Info, and Modern Swing Gauge */
        /* Club Card */
        draw_rect_alpha(rc, 14, btm_y + 8, 160, 52, COLOR_RGB(25, 38, 52), 220);
        draw_rect_border(rc, 14, btm_y + 8, 160, 52, COLOR_RGB(75, 115, 155));
        const ClubData *cdata = golf_get_club_data(gs->current_club);
        draw_text(rc, 24, btm_y + 14, cdata->name, COLOR_RGB(255, 235, 90), true);
        char c_yds[32];
        snprintf(c_yds, sizeof(c_yds), "MAX: %d YARDS", cdata->nominal_yards);
        draw_text(rc, 24, btm_y + 28, c_yds, COLOR_RGB(215, 225, 235), false);
        draw_text(rc, 24, btm_y + 42, "[C] CHANGE CLUB", COLOR_RGB(135, 175, 205), false);
        
        /* Wind & Target Card */
        draw_rect_alpha(rc, 184, btm_y + 8, 150, 52, COLOR_RGB(25, 38, 52), 220);
        draw_rect_border(rc, 184, btm_y + 8, 150, 52, COLOR_RGB(75, 115, 155));
        char w_buf[32];
        snprintf(w_buf, sizeof(w_buf), "WIND: %d MPH", gs->wind.speed);
        draw_text(rc, 194, btm_y + 14, w_buf, COLOR_RGB(135, 215, 255), true);
        char pin_buf[32];
        snprintf(pin_buf, sizeof(pin_buf), "TO PIN: %d YDS", golf_distance_to_pin_yards(gs));
        draw_text(rc, 194, btm_y + 28, pin_buf, COLOR_RGB(240, 245, 250), false);
        draw_text(rc, 194, btm_y + 42, "CLICK MAP TO AIM", COLOR_RGB(135, 175, 205), false);
        
        /* 3-Click Precision Swing Gauge */
        int sg_x = 350;
        int sg_y = btm_y + 10;
        int sg_w = 340;
        int sg_h = 48;
        draw_rect_alpha(rc, sg_x, sg_y, sg_w, sg_h, COLOR_RGB(22, 32, 44), 235);
        draw_rect_border(rc, sg_x, sg_y, sg_w, sg_h, COLOR_RGB(95, 135, 175));
        
        /* Power meter track */
        int trk_x = sg_x + 15;
        int trk_y = sg_y + 10;
        int trk_w = 310;
        int trk_h = 16;
        draw_rect(rc, trk_x, trk_y, trk_w, trk_h, COLOR_RGB(35, 45, 55));
        draw_rect_border(rc, trk_x, trk_y, trk_w, trk_h, COLOR_RGB(75, 90, 110));
        
        /* Golden Sweet Spot Zone */
        int sw_x = trk_x + (trk_w * gs->swing.sweetspot_center) / FIXED_ONE;
        int sw_w = (trk_w * gs->swing.sweetspot_width * 2) / FIXED_ONE;
        draw_rect(rc, sw_x - sw_w / 2, trk_y + 1, sw_w, trk_h - 2, COLOR_RGB(235, 195, 45));
        
        /* Locked Power fill if locked */
        if (gs->swing.state >= SWING_STATE_ACCURACY) {
            int fill_w = (trk_w * gs->swing.locked_power) / FIXED_ONE;
            draw_rect_alpha(rc, trk_x + 1, trk_y + 1, fill_w, trk_h - 2, COLOR_RGB(65, 185, 95), 180);
        }
        
        /* Moving Needle Cursor */
        int needle_x = trk_x + (trk_w * gs->swing.cursor_pos) / FIXED_ONE;
        draw_rect(rc, needle_x - 1, trk_y - 3, 3, trk_h + 6, COLOR_RGB(255, 255, 255));
        
        /* Status prompt text */
        if (gs->swing.state == SWING_STATE_READY) {
            if (gs->golfer.state == GOLFER_STATE_WALKING) {
                draw_text(rc, sg_x + 65, sg_y + 32, "WALKING TO BALL... (SPACE TO SKIP)", COLOR_RGB(175, 215, 255), true);
            } else {
                draw_text(rc, sg_x + 40, sg_y + 32, "PRESS 1: SPACE OR CLICK TO START SWING", COLOR_RGB(255, 235, 90), true);
            }
        } else if (gs->swing.state == SWING_STATE_POWER) {
            draw_text(rc, sg_x + 35, sg_y + 32, "PRESS 2: LOCK POWER (OR AUTO-BOUNCE AT 100%)", COLOR_RGB(255, 245, 120), true);
        } else if (gs->swing.state == SWING_STATE_ACCURACY) {
            draw_text(rc, sg_x + 55, sg_y + 32, "PRESS IN GOLD SWEETSPOT TO STRIKE!", COLOR_RGB(255, 205, 45), true);
        } else {
            draw_text(rc, sg_x + 115, sg_y + 32, "BALL IN FLIGHT...", COLOR_RGB(165, 215, 255), true);
        }
    }
    
    /* 7. Floating Notification Banner */
    if (gs->banner_timer > 0) {
        int ban_w = 480;
        int ban_h = 36;
        int ban_x = SCREEN_W / 2 - ban_w / 2;
        int ban_y = 65;
        draw_rect_alpha(rc, ban_x, ban_y, ban_w, ban_h, COLOR_RGB(15, 25, 35), 235);
        draw_rect_border(rc, ban_x, ban_y, ban_w, ban_h, COLOR_RGB(255, 215, 65));
        int text_len = (int)strlen(gs->banner_text);
        int tx = ban_x + (ban_w - text_len * 8) / 2;
        draw_text(rc, tx, ban_y + 13, gs->banner_text, COLOR_RGB(255, 245, 120), true);
    }
}

static void draw_text_scaled(RenderContext *rc, int32_t x, int32_t y, const char *str, uint32_t color, int scale, bool shadow) {
    if (!str || scale <= 0) return;
    int cur_x = x;
    while (*str) {
        char ch = *str++;
        if (ch >= 32 && ch <= 126) {
            const uint8_t *glyph = g_font8x8[ch - 32];
            for (int gy = 0; gy < 8; ++gy) {
                uint8_t row = glyph[gy];
                for (int gx = 0; gx < 8; ++gx) {
                    if (row & (0x80 >> gx)) {
                        int px0 = cur_x + gx * scale;
                        int py0 = y + gy * scale;
                        if (shadow) {
                            for (int dy = 0; dy < scale; ++dy) {
                                for (int dx = 0; dx < scale; ++dx) {
                                    int spx = px0 + dx + scale;
                                    int spy = py0 + dy + scale;
                                    if (spx >= 0 && spx < SCREEN_W && spy >= 0 && spy < SCREEN_H) {
                                        rc->framebuffer[spy * SCREEN_W + spx] = COLOR_RGB(15, 12, 18);
                                    }
                                }
                            }
                        }
                        for (int dy = 0; dy < scale; ++dy) {
                            for (int dx = 0; dx < scale; ++dx) {
                                int px = px0 + dx;
                                int py = py0 + dy;
                                if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
                                    rc->framebuffer[py * SCREEN_W + px] = color;
                                }
                            }
                        }
                    }
                }
            }
        }
        cur_x += 8 * scale;
    }
}

/* Native High-Definition 1024x768 Splash Screen Renderer */
void render_splash_screen(RenderContext *rc, uint32_t tick_count) {
    /* 1. Golden Hour Sunset Sky Gradient (y = 0..440) */
    for (int y = 0; y < 440; ++y) {
        float t = (float)y / 440.0f;
        int r, g, b;
        if (t < 0.40f) {
            float f = t / 0.40f;
            r = (int)(20.0f * (1.0f - f) + 95.0f * f);
            g = (int)(32.0f * (1.0f - f) + 55.0f * f);
            b = (int)(75.0f * (1.0f - f) + 88.0f * f);
        } else if (t < 0.75f) {
            float f = (t - 0.40f) / 0.35f;
            r = (int)(95.0f * (1.0f - f) + 215.0f * f);
            g = (int)(55.0f * (1.0f - f) + 105.0f * f);
            b = (int)(88.0f * (1.0f - f) + 55.0f * f);
        } else {
            float f = (t - 0.75f) / 0.25f;
            r = (int)(215.0f * (1.0f - f) + 255.0f * f);
            g = (int)(105.0f * (1.0f - f) + 195.0f * f);
            b = (int)(55.0f * (1.0f - f) + 90.0f * f);
        }
        uint32_t col = COLOR_RGB(r, g, b);
        for (int x = 0; x < SCREEN_W; ++x) {
            rc->framebuffer[y * SCREEN_W + x] = col;
        }
    }
    
    /* 2. Setting Sun Orb & Glowing Corona at (780, 230) */
    int sun_cx = 780;
    int sun_cy = 230;
    for (int dy = -75; dy <= 75; ++dy) {
        int py = sun_cy + dy;
        if (py < 0 || py >= SCREEN_H) continue;
        for (int dx = -75; dx <= 75; ++dx) {
            int px = sun_cx + dx;
            if (px < 0 || px >= SCREEN_W) continue;
            int d2 = dx * dx + dy * dy;
            if (d2 <= 50 * 50) {
                rc->framebuffer[py * SCREEN_W + px] = COLOR_RGB(255, 252, 215);
            } else if (d2 <= 75 * 75) {
                int a = 200 - ((d2 - 50 * 50) * 200) / (75 * 75 - 50 * 50);
                rc->framebuffer[py * SCREEN_W + px] = alpha_blend_pixel(rc->framebuffer[py * SCREEN_W + px], COLOR_RGB(255, 210, 95), (uint8_t)a);
            }
        }
    }
    
    /* Subtle Golden Clouds */
    for (int cy = 160; cy <= 280; cy += 50) {
        for (int x = 0; x < SCREEN_W; ++x) {
            int cloud = (int)(sinf((float)(x + cy * 7) * 0.015f) * 12.0f);
            if (cloud > 4) {
                int py = cy + cloud;
                if (py >= 0 && py < SCREEN_H) {
                    rc->framebuffer[py * SCREEN_W + x] = alpha_blend_pixel(rc->framebuffer[py * SCREEN_W + x], COLOR_RGB(245, 175, 120), 80);
                }
            }
        }
    }
    
    /* 3. Distant Purple Mountain Ridge */
    for (int x = 0; x < SCREEN_W; ++x) {
        int my = 330 + (int)(sinf((float)x * 0.008f) * 36.0f + cosf((float)x * 0.021f) * 16.0f);
        for (int y = my; y < 440; ++y) {
            int light = (y - my < 10) ? 22 : 0;
            rc->framebuffer[y * SCREEN_W + x] = COLOR_RGB(65 + light, 48 + light, 78 + light);
        }
    }
    
    /* 4. Pine Foothills Silhouette (y = 370..440) */
    for (int x = 0; x < SCREEN_W; ++x) {
        int fy = 375 + (int)(sinf((float)x * 0.018f) * 20.0f + cosf((float)x * 0.045f) * 10.0f);
        for (int y = fy; y < 440; ++y) {
            rc->framebuffer[y * SCREEN_W + x] = COLOR_RGB(38, 55, 48);
        }
    }
    
    /* 5. Rolling Emerald Fairway Turf & Course Landscape (y = 370..768) */
    for (int y = 370; y < SCREEN_H; ++y) {
        float t = (float)(y - 370) / (float)(SCREEN_H - 370);
        for (int x = 0; x < SCREEN_W; ++x) {
            /* Rolling contour hill curve */
            int hill_offset = (int)(sinf((float)x * 0.007f) * 20.0f + cosf((float)x * 0.016f) * 10.0f);
            if (y < 395 + hill_offset) continue;
            
            /* Fairway stripes */
            int stripe = ((x + y) / 24) & 1;
            int r = stripe ? (int)(55.0f * (1.0f - t) + 42.0f * t) : (int)(45.0f * (1.0f - t) + 34.0f * t);
            int g = stripe ? (int)(155.0f * (1.0f - t) + 115.0f * t) : (int)(135.0f * (1.0f - t) + 98.0f * t);
            int b = stripe ? (int)(45.0f * (1.0f - t) + 32.0f * t) : (int)(38.0f * (1.0f - t) + 26.0f * t);
            rc->framebuffer[y * SCREEN_W + x] = COLOR_RGB(r, g, b);
        }
    }
    
    /* 6. Grand Georgian Clubhouse on High Bluff (x = 173..303, y = 368..453 - 100% Dry Ground!) */
    render_clubhouse(rc, 205, 420);
    
    /* 6c. Weathered Caddyshack Building with Caddy Kids */
    int shack_x = 95;
    int shack_y = 445;
    draw_drop_shadow(rc, shack_x + 35, shack_y + 40, 36, 14, 140);
    for (int y = 14; y <= 40; ++y) {
        for (int x = 6; x <= 64; ++x) {
            int plank = (y % 3 == 0) ? -16 : 8;
            rc->framebuffer[(shack_y + y) * SCREEN_W + (shack_x + x)] = COLOR_RGB(110 + plank, 95 + plank, 78 + plank);
        }
    }
    for (int y = 0; y <= 14; ++y) {
        for (int x = 2; x <= 68; ++x) {
            if (x >= 2 + (14 - y) && x <= 68 - (14 - y)) {
                int tin = (x % 4 == 0) ? 18 : -14;
                rc->framebuffer[(shack_y + y) * SCREEN_W + (shack_x + x)] = COLOR_RGB(85 + tin, 88 + tin, 92 + tin);
            }
        }
    }
    draw_rect(rc, shack_x + 28, shack_y + 20, 14, 20, COLOR_RGB(28, 22, 18));
    draw_rect(rc, shack_x + 16, shack_y + 15, 40, 8, COLOR_RGB(65, 48, 35));
    draw_text(rc, shack_x + 18, shack_y + 15, "CADDY", COLOR_RGB(255, 225, 120), false);
    
    /* 3 Caddy Kids hanging around in white pants and red & black shirts */
    /* Kid 1: Sitting by doorway, red shirt, white pants */
    draw_rect(rc, shack_x + 18, shack_y + 24, 5, 5, COLOR_RGB(245, 205, 175));
    draw_rect(rc, shack_x + 17, shack_y + 29, 7, 7, COLOR_RGB(220, 35, 35));
    draw_rect(rc, shack_x + 17, shack_y + 36, 7, 6, COLOR_RGB(245, 248, 252));
    
    /* Kid 2: Leaning against wall with golf bag, black shirt, white pants */
    draw_rect(rc, shack_x + 48, shack_y + 22, 5, 5, COLOR_RGB(245, 205, 175));
    draw_rect(rc, shack_x + 47, shack_y + 27, 7, 8, COLOR_RGB(32, 32, 38));
    draw_rect(rc, shack_x + 47, shack_y + 35, 7, 7, COLOR_RGB(245, 248, 252));
    draw_rect(rc, shack_x + 55, shack_y + 26, 4, 16, COLOR_RGB(95, 55, 35));
    draw_rect(rc, shack_x + 56, shack_y + 22, 2, 4, COLOR_RGB(190, 195, 205));
    
    /* Kid 3: Lounging on grass, red & black shirt, white pants */
    draw_rect(rc, shack_x + 72, shack_y + 32, 5, 5, COLOR_RGB(245, 205, 175));
    draw_rect(rc, shack_x + 71, shack_y + 37, 7, 6, COLOR_RGB(200, 30, 30));
    draw_rect(rc, shack_x + 70, shack_y + 43, 9, 4, COLOR_RGB(245, 248, 252));
    
    /* Water Hazard Lake down on lower left (x = 20..170, y = 545..585 - nestled in valley) */
    for (int dy = -20; dy <= 20; ++dy) {
        int py = 565 + dy;
        if (py < 0 || py >= SCREEN_H) continue;
        for (int dx = -75; dx <= 75; ++dx) {
            int px = 95 + dx;
            if (px < 0 || px >= SCREEN_W) continue;
            if (dx * dx * 2 + dy * dy * 18 <= 75 * 75 * 2) {
                int wave = (int)(sinf((float)px * 0.1f + (float)tick_count * 0.08f) * 1.5f);
                rc->framebuffer[(py + wave) * SCREEN_W + px] = COLOR_RGB(42, 95, 165);
            }
        }
    }
    
    /* White Sand Bunker Guarding Putting Green (x = 765..835, y = 450..480) */
    for (int dy = -16; dy <= 16; ++dy) {
        int py = 465 + dy;
        if (py < 0 || py >= SCREEN_H) continue;
        for (int dx = -36; dx <= 36; ++dx) {
            int px = 800 + dx;
            if (px < 0 || px >= SCREEN_W) continue;
            if (dx * dx * 2 + dy * dy * 10 <= 36 * 36 * 2) {
                int shade = (dy < -8) ? -28 : 12;
                rc->framebuffer[py * SCREEN_W + px] = COLOR_RGB(225 + shade, 210 + shade, 160 + shade);
            }
        }
    }
    
    /* Flagstick on Putting Green (x = 840, y = 450) */
    draw_rect(rc, 840, 415, 2, 35, COLOR_RGB(230, 235, 240));
    draw_rect(rc, 842, 415, 14, 8, COLOR_RGB(225, 45, 45)); /* Red flag */
    
    /* Trees along ridge */
    blit_sprite_depth(rc, 40, 390, SPRITE_PINE_W, SPRITE_PINE_H, s_sprite_pine, 999999);
    blit_sprite_depth(rc, 280, 400, SPRITE_OAK_W, SPRITE_OAK_H, s_sprite_oak, 999999);
    blit_sprite_depth(rc, 910, 420, SPRITE_PALM_W, SPRITE_PALM_H, s_sprite_palm, 999999);
    blit_sprite_depth(rc, 870, 450, SPRITE_FLOWERING_W, SPRITE_FLOWERING_H, s_sprite_flowering, 999999);
    
    /* 6b. Carved Wooden "Bushwood" Sign by Clubhouse (drawn on top of lawn & ridge, fully visible) */
    int sign_x = 338;
    int sign_y = 436;
    draw_rect(rc, sign_x + 6, sign_y + 12, 3, 16, COLOR_RGB(78, 52, 34));
    draw_rect(rc, sign_x + 62, sign_y + 12, 3, 16, COLOR_RGB(78, 52, 34));
    draw_rect(rc, sign_x, sign_y, 72, 18, COLOR_RGB(145, 96, 56));
    draw_rect_border(rc, sign_x, sign_y, 72, 18, COLOR_RGB(68, 42, 24));
    draw_rect(rc, sign_x + 2, sign_y + 2, 2, 2, COLOR_RGB(220, 195, 60));
    draw_rect(rc, sign_x + 68, sign_y + 2, 2, 2, COLOR_RGB(220, 195, 60));
    draw_text(rc, sign_x + 5, sign_y + 5, "BUSHWOOD", COLOR_RGB(255, 235, 140), true);
    
    /* 7. Foreground Golfer on Left (x = 270, y = 520) */
    draw_drop_shadow(rc, 284, 555, 14, 6, 140);
    blit_sprite_depth(rc, 270, 520, SPRITE_GOLFER_W, SPRITE_GOLFER_H, s_sprite_golfer, 999999);
    draw_text(rc, 252, 563, "CHAMPION", COLOR_RGB(255, 235, 110), true);
    
    /* 8. Foreground Beverage Cart Girl on Right (x = 710, y = 520) */
    draw_drop_shadow(rc, 733, 556, 26, 10, 140);
    blit_sprite_depth(rc, 710, 520, SPRITE_BEV_CART_W, SPRITE_BEV_CART_H, s_sprite_bev_cart, 999999);
    
    /* Floating Melodic Whistle Speech Bubble above Beverage Cart */
    const char *bev_msg = "COLD DRINKS!";
    int bub_w = 126;
    int bub_h = 22;
    int bub_x = 647;
    int bub_y = 485;
    draw_rect(rc, bub_x, bub_y, bub_w, bub_h, COLOR_RGB(255, 255, 255));
    draw_rect_border(rc, bub_x, bub_y, bub_w, bub_h, COLOR_RGB(35, 45, 55));
    for (int dy = 0; dy < 6; ++dy) {
        draw_rect(rc, bub_x + 60 + dy / 2, bub_y + bub_h + dy, 6 - dy, 1, COLOR_RGB(255, 255, 255));
    }
    int b_tx = bub_x + (bub_w - (int)strlen(bev_msg) * 8) / 2;
    draw_text(rc, b_tx, bub_y + 7, bev_msg, COLOR_RGB(220, 20, 80), false);
    
    /* 8b. Foreground Caddyshack Gopher Dancing in Center (x = 490, y = 525) */
    int goph_x = 490;
    int goph_y = 525;
    
    /* Gopher hole dirt mound */
    draw_drop_shadow(rc, goph_x + 14, goph_y + 24, 20, 8, 150);
    blit_sprite_depth(rc, goph_x, goph_y + 16, SPRITE_GOPHER_HOLE_W, SPRITE_GOPHER_HOLE_H, s_sprite_gopher_hole, 999999);
    
    /* Dancing Caddyshack Gopher (grooving left/right to "I'm Alright" beat!) */
    int g_dance_frame = 2 + ((tick_count / 14) % 2);
    blit_sprite_depth(rc, goph_x + 1, goph_y, SPRITE_GOPHER_W, SPRITE_GOPHER_H, s_sprite_gopher[g_dance_frame], 999999);
    draw_text(rc, goph_x - 10, goph_y + 38, "CADDY GOPHER", COLOR_RGB(255, 235, 110), true);
    
    /* Floating Caddyshack Groove Speech Bubble above Gopher */
    const char *goph_msg = "I'M ALRIGHT!";
    int gbub_w = 120;
    int gbub_h = 22;
    int gbub_x = goph_x - 44;
    int gbub_y = goph_y - 32;
    draw_rect(rc, gbub_x, gbub_y, gbub_w, gbub_h, COLOR_RGB(255, 255, 255));
    draw_rect_border(rc, gbub_x, gbub_y, gbub_w, gbub_h, COLOR_RGB(35, 45, 55));
    for (int dy = 0; dy < 6; ++dy) {
        draw_rect(rc, gbub_x + 55 + dy / 2, gbub_y + gbub_h + dy, 6 - dy, 1, COLOR_RGB(255, 255, 255));
    }
    int gbtx = gbub_x + (gbub_w - (int)strlen(goph_msg) * 8) / 2;
    draw_text(rc, gbtx, gbub_y + 7, goph_msg, COLOR_RGB(220, 120, 20), false);
    
    /* 9. Hero Title Logo: "DUFFER PRO" with 3D Embossing (All 3 framing boxes removed!) */
    int title_y = 65;
    const char *title_text = "DUFFER PRO";
    int title_scale = 4;
    int title_w = (int)strlen(title_text) * 8 * title_scale;
    int title_x = SCREEN_W / 2 - title_w / 2;
    
    /* 3D Drop Shadows */
    draw_text_scaled(rc, title_x + 4, title_y + 4, title_text, COLOR_RGB(20, 15, 10), title_scale, false);
    draw_text_scaled(rc, title_x + 2, title_y + 2, title_text, COLOR_RGB(45, 30, 15), title_scale, false);
    /* Specular Bevel Top Highlight */
    draw_text_scaled(rc, title_x - 1, title_y - 1, title_text, COLOR_RGB(255, 255, 235), title_scale, false);
    /* Metallic Gold Face */
    draw_text_scaled(rc, title_x, title_y, title_text, COLOR_RGB(255, 215, 65), title_scale, false);
    
    /* Subtitle Banner (Clean Text, no enclosing box) */
    const char *ribbon_text = "★  CHAMPIONSHIP RESORT GOLF  ★";
    int r_tx = SCREEN_W / 2 - (int)strlen(ribbon_text) * 8 / 2;
    draw_text(rc, r_tx, title_y + 42, ribbon_text, COLOR_RGB(255, 240, 140), true);
    
    /* Tagline */
    const char *sub_text = "The Definitive Isometric Country Club Simulation";
    int s_tx = SCREEN_W / 2 - (int)strlen(sub_text) * 8 / 2;
    draw_text(rc, s_tx, title_y + 58, sub_text, COLOR_RGB(255, 225, 145), true);
    
    /* 10. Interactive Dual Mode Buttons: [ SNEAK ON ] and [ PLAY A HOLE ] */
    int btn_w = 230;
    int btn_h = 46;
    
    /* SNEAK ON Button */
    int btn_sneak_x = 240;
    int btn_sneak_y = 632;
    draw_rect_alpha(rc, btn_sneak_x, btn_sneak_y, btn_w, btn_h, COLOR_RGB(25, 60, 35), 235);
    draw_rect_border(rc, btn_sneak_x, btn_sneak_y, btn_w, btn_h, COLOR_RGB(255, 215, 65));
    draw_text_2x(rc, btn_sneak_x + 51, btn_sneak_y + 15, "SNEAK ON", COLOR_RGB(255, 255, 255), true);
    
    /* PLAY A HOLE Button */
    int btn_play_x = 554;
    int btn_play_y = 632;
    draw_rect_alpha(rc, btn_play_x, btn_play_y, btn_w, btn_h, COLOR_RGB(25, 45, 80), 235);
    draw_rect_border(rc, btn_play_x, btn_play_y, btn_w, btn_h, COLOR_RGB(255, 215, 65));
    draw_text_2x(rc, btn_play_x + 27, btn_play_y + 15, "PLAY A HOLE", COLOR_RGB(255, 255, 255), true);
    
    /* Clear Instruction & Controls */
    const char *note_text = "Click SNEAK ON for instant round  |  PLAY A HOLE (or ENTER) to customize obstacles";
    int ntx = SCREEN_W / 2 - (int)strlen(note_text) * 8 / 2;
    draw_text(rc, ntx, 694, note_text, COLOR_RGB(255, 225, 130), true);
    
    const char *keys_hint = "Controls: TAB Switch Modes | Mouse Aim & Drag | Arrow Keys Pan Camera | R Replay Hole";
    int ktx = SCREEN_W / 2 - (int)strlen(keys_hint) * 8 / 2;
    draw_text(rc, ktx, 714, keys_hint, COLOR_RGB(165, 195, 215), false);
    draw_text(rc, SCREEN_W - 120, SCREEN_H - 18, "v" DUFFER_VERSION, COLOR_RGB(135, 165, 195), false);
    
    /* Outer screen vignette border */
    draw_rect_border(rc, 0, 0, SCREEN_W, SCREEN_H, COLOR_RGB(35, 45, 55));
}

/* Championship Setup Screen Renderer (1024x768) */
void render_setup_screen(RenderContext *rc, const GolfState *gs, uint32_t tick_count) {
    /* 1. Backdrop */
    for (int y = 0; y < SCREEN_H; ++y) {
        float t = (float)y / (float)SCREEN_H;
        int r = (int)(22.0f * (1.0f - t) + 12.0f * t);
        int g = (int)(32.0f * (1.0f - t) + 18.0f * t);
        int b = (int)(46.0f * (1.0f - t) + 26.0f * t);
        uint32_t c = COLOR_RGB(r, g, b);
        for (int x = 0; x < SCREEN_W; ++x) {
            rc->framebuffer[y * SCREEN_W + x] = c;
        }
    }
    
    /* 2. Glassmorphism Main Dashboard Panel */
    int pan_x = 100, pan_y = 50, pan_w = 824, pan_h = 660;
    draw_rect_alpha(rc, pan_x, pan_y, pan_w, pan_h, COLOR_RGB(18, 26, 36), 235);
    draw_rect_border(rc, pan_x, pan_y, pan_w, pan_h, COLOR_RGB(75, 115, 165));
    
    /* Top Title Header */
    draw_text_2x(rc, pan_x + 30, pan_y + 24, "COURSE SETUP & TEE CUSTOMIZATION", COLOR_RGB(255, 215, 65), true);
    draw_text(rc, pan_x + 32, pan_y + 54, "Select natural obstacle scales (1-5) and environmental course theme", COLOR_RGB(180, 205, 230), false);
    draw_rect(rc, pan_x + 30, pan_y + 74, pan_w - 60, 1, COLOR_RGB(60, 85, 115));
    
    /* 3. Obstacle Density Radio Buttons */
    const char *row_names[4] = { "TREES:", "BUSHES:", "ROCKS:", "WATER:" };
    int scales[4] = { gs->tree_scale, gs->bush_scale, gs->rock_scale, gs->water_scale };
    int row_y_starts[4] = { pan_y + 105, pan_y + 155, pan_y + 205, pan_y + 255 };
    
    for (int r = 0; r < 4; ++r) {
        int ry = row_y_starts[r];
        draw_text_2x(rc, pan_x + 40, ry - 4, row_names[r], COLOR_RGB(240, 245, 250), true);
        
        for (int v = 1; v <= 5; ++v) {
            int cx = pan_x + 280 + (v - 1) * 65;
            int cy = ry + 4;
            bool active = (scales[r] == v);
            
            for (int dy = -10; dy <= 10; ++dy) {
                for (int dx = -10; dx <= 10; ++dx) {
                    int d2 = dx * dx + dy * dy;
                    int spx = cx + dx;
                    int spy = cy + dy;
                    if (spx >= 0 && spx < SCREEN_W && spy >= 0 && spy < SCREEN_H) {
                        if (d2 <= 100 && d2 >= 64) {
                            rc->framebuffer[spy * SCREEN_W + spx] = active ? COLOR_RGB(255, 220, 75) : COLOR_RGB(100, 120, 145);
                        }
                        if (active && d2 <= 36) {
                            rc->framebuffer[spy * SCREEN_W + spx] = COLOR_RGB(255, 215, 60);
                        }
                    }
                }
            }
            char num_str[4];
            snprintf(num_str, sizeof(num_str), "%d", v);
            draw_text(rc, cx - 4, cy + 15, num_str, active ? COLOR_RGB(255, 235, 90) : COLOR_RGB(165, 175, 190), true);
        }
        
        const char *desc[5] = { "1: Very Light", "2: Light", "3: Medium (Default)", "4: Heavy", "5: Maximum" };
        int cur_val = scales[r];
        if (cur_val < 1) cur_val = 1; if (cur_val > 5) cur_val = 5;
        draw_text(rc, pan_x + 630, ry + 2, desc[cur_val - 1], COLOR_RGB(145, 215, 175), true);
    }
    
    /* Playability Guarantee Banner */
    int g_y = pan_y + 305;
    draw_rect_alpha(rc, pan_x + 35, g_y, pan_w - 70, 32, COLOR_RGB(25, 45, 35), 200);
    draw_rect_border(rc, pan_x + 35, g_y, pan_w - 70, 32, COLOR_RGB(65, 155, 95));
    draw_text(rc, pan_x + 50, g_y + 10, "[OK] FAIRWAY OVERLAY ACTIVE: Map obstacles hard-capped at 70% coverage. 100% playable!", COLOR_RGB(180, 245, 190), true);
    
    /* 4. Course Theme Selector Cards */
    draw_text_2x(rc, pan_x + 40, pan_y + 355, "COURSE THEME:", COLOR_RGB(255, 215, 65), true);
    
    struct {
        CourseTheme theme;
        const char *title;
        const char *sub1;
        const char *sub2;
        int x;
        uint32_t color;
    } theme_cards[3] = {
        { THEME_BUSHWOOD, "BUSHWOOD", "Classic Country Club Greens", "Gopher & Carl Spackler", pan_x + 34, COLOR_RGB(45, 140, 75) },
        { THEME_LA_PLAYA, "LA PLAYA", "Desert Sands & Saguaro Cactus", "Burning Man & Raver vs Cop", pan_x + 288, COLOR_RGB(195, 145, 65) },
        { THEME_NOCTURNAL_GREEN, "NOCTURNAL GREEN", "Transylvania Purples & Greys", "Zombies & Ash vs Monster", pan_x + 542, COLOR_RGB(115, 65, 165) }
    };
    
    for (int t = 0; t < 3; ++t) {
        int tx = theme_cards[t].x;
        int ty = pan_y + 390;
        int tw = 248, th = 115;
        bool sel = (gs->theme == theme_cards[t].theme);
        
        draw_rect_alpha(rc, tx, ty, tw, th, sel ? COLOR_RGB(35, 55, 75) : COLOR_RGB(24, 32, 42), 230);
        draw_rect_border(rc, tx, ty, tw, th, sel ? COLOR_RGB(255, 215, 65) : COLOR_RGB(70, 85, 105));
        if (sel) {
            draw_rect_border(rc, tx + 1, ty + 1, tw - 2, th - 2, COLOR_RGB(255, 235, 120));
        }
        
        draw_rect(rc, tx + 8, ty + 8, 12, 12, theme_cards[t].color);
        if (strlen(theme_cards[t].title) > 10) {
            draw_text(rc, tx + 25, ty + 10, theme_cards[t].title, sel ? COLOR_RGB(255, 235, 90) : COLOR_RGB(230, 235, 240), true);
            draw_text(rc, tx + 26, ty + 10, theme_cards[t].title, sel ? COLOR_RGB(255, 235, 90) : COLOR_RGB(230, 235, 240), false);
        } else {
            draw_text_2x(rc, tx + 26, ty + 8, theme_cards[t].title, sel ? COLOR_RGB(255, 235, 90) : COLOR_RGB(230, 235, 240), true);
        }
        draw_text(rc, tx + 8, ty + 42, theme_cards[t].sub1, COLOR_RGB(190, 205, 220), false);
        draw_text(rc, tx + 8, ty + 62, theme_cards[t].sub2, COLOR_RGB(165, 185, 205), false);
        draw_text(rc, tx + 8, ty + 90, sel ? "[ SELECTED ACTIVE ]" : "Click to Select", sel ? COLOR_RGB(255, 215, 65) : COLOR_RGB(120, 140, 160), true);
    }
    
    /* 5. Bottom Action Buttons */
    int btn_action_y = pan_y + 535;
    
    /* Back Button */
    int b_back_x = pan_x + 40;
    draw_rect_alpha(rc, b_back_x, btn_action_y, 160, 48, COLOR_RGB(45, 55, 65), 220);
    draw_rect_border(rc, b_back_x, btn_action_y, 160, 48, COLOR_RGB(105, 125, 145));
    draw_text_2x(rc, b_back_x + 22, btn_action_y + 14, "< BACK", COLOR_RGB(220, 230, 240), true);
    
    /* Randomize Button */
    int b_rand_x = pan_x + 220;
    draw_rect_alpha(rc, b_rand_x, btn_action_y, 180, 48, COLOR_RGB(35, 65, 85), 220);
    draw_rect_border(rc, b_rand_x, btn_action_y, 180, 48, COLOR_RGB(85, 145, 185));
    draw_text_2x(rc, b_rand_x + 14, btn_action_y + 14, "RANDOMIZE", COLOR_RGB(220, 240, 255), true);
    
    /* Start Play Button */
    int b_play_x = pan_x + 420;
    int b_play_w = pan_w - 460;
    bool pulse = ((tick_count / 20) % 2 == 0);
    uint32_t pcol = pulse ? COLOR_RGB(55, 155, 85) : COLOR_RGB(45, 135, 75);
    draw_rect_alpha(rc, b_play_x, btn_action_y, b_play_w, 48, pcol, 240);
    draw_rect_border(rc, b_play_x, btn_action_y, b_play_w, 48, COLOR_RGB(255, 235, 90));
    draw_text_2x(rc, b_play_x + 50, btn_action_y + 14, ">> START PLAY <<", COLOR_RGB(255, 255, 255), true);
}

/* BMP Screenshot Saver (1024x768) */
bool render_save_bmp(const RenderContext *rc, const char *filename) {
    FILE *f = fopen(filename, "wb");
    if (!f) return false;
    
    uint16_t bfType = 0x4D42;
    uint32_t bfSize = 54 + (SCREEN_W * SCREEN_H * 3);
    uint32_t bfReserved = 0;
    uint32_t bfOffBits = 54;
    
    fwrite(&bfType, 2, 1, f);
    fwrite(&bfSize, 4, 1, f);
    fwrite(&bfReserved, 4, 1, f);
    fwrite(&bfOffBits, 4, 1, f);
    
    uint32_t biSize = 40;
    int32_t biWidth = SCREEN_W;
    int32_t biHeight = SCREEN_H;
    uint16_t biPlanes = 1;
    uint16_t biBitCount = 24;
    uint32_t biCompression = 0;
    uint32_t biSizeImage = SCREEN_W * SCREEN_H * 3;
    int32_t biXPelsPerMeter = 2835;
    int32_t biYPelsPerMeter = 2835;
    uint32_t biClrUsed = 0;
    uint32_t biClrImportant = 0;
    
    fwrite(&biSize, 4, 1, f);
    fwrite(&biWidth, 4, 1, f);
    fwrite(&biHeight, 4, 1, f);
    fwrite(&biPlanes, 2, 1, f);
    fwrite(&biBitCount, 2, 1, f);
    fwrite(&biCompression, 4, 1, f);
    fwrite(&biSizeImage, 4, 1, f);
    fwrite(&biXPelsPerMeter, 4, 1, f);
    fwrite(&biYPelsPerMeter, 4, 1, f);
    fwrite(&biClrUsed, 4, 1, f);
    fwrite(&biClrImportant, 4, 1, f);
    
    for (int y = SCREEN_H - 1; y >= 0; --y) {
        for (int x = 0; x < SCREEN_W; ++x) {
            uint32_t p = rc->framebuffer[y * SCREEN_W + x];
            uint8_t b = p & 0xFF;
            uint8_t g = (p >> 8) & 0xFF;
            uint8_t r = (p >> 16) & 0xFF;
            fputc(b, f);
            fputc(g, f);
            fputc(r, f);
        }
    }
    
    fclose(f);
    return true;
}
