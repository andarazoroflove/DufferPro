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

#define SPRITE_CADDY_W 28
#define SPRITE_CADDY_H 38
static uint32_t s_sprite_caddy[SPRITE_CADDY_W * SPRITE_CADDY_H];
static uint32_t s_sprite_caddy_walk[6][SPRITE_CADDY_W * SPRITE_CADDY_H];
static uint32_t s_sprite_caddy_celebrate[SPRITE_CADDY_W * SPRITE_CADDY_H];

#define SPRITE_SPEC_W 20
#define SPRITE_SPEC_H 30
static uint32_t s_sprite_spec[5][4][SPRITE_SPEC_W * SPRITE_SPEC_H]; /* [attire][anim] */

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
static void generate_procedural_graphics(void) {
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
                int r = 52 + noise - (tuft / 2);
                int g = 108 + noise * 2 + tuft;
                int b = 38 + noise / 2;
                if (r < 25) r = 25; if (r > 80) r = 80;
                if (g < 70) g = 70; if (g > 150) g = 150;
                if (b < 15) b = 15; if (b > 60) b = 60;
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
                int r = stripe ? (102 + noise) : (84 + noise);
                int g = stripe ? (186 + noise * 2) : (158 + noise * 2);
                int b = stripe ? (46 + noise) : (38 + noise);
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
                uint32_t gr_col = COLOR_RGB(110 + grain, 216 + grain * 2, 66 + grain);
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
    
    /* 4. Sand Bunkers (Raked Sand Waves & Shaded Lip) */
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
                uint32_t sand_col = COLOR_RGB(232 + wave + noise, 214 + wave + noise, 152 + wave / 2);
                if (dist_edge < 5) {
                    int t256 = (dist_edge * 256) / 5;
                    sand_col = blend_color(COLOR_RGB(135, 110, 68), sand_col, t256);
                }
                s_tile_sand[m][idx] = sand_col;
            }
        }
    }
    
    /* 5. Animated Water (Turquoise-to-Deep-Blue Gradient & Sparkles) */
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
                int r = 36 + (int)wave;
                int g = 120 + (int)(wave * 1.5f);
                int b = 196 + (int)(wave * 2.0f);
                
                /* Shallows near shore */
                if (dist_edge < 6) {
                    r += (6 - dist_edge) * 6;
                    g += (6 - dist_edge) * 10;
                    b += (6 - dist_edge) * 4;
                }
                
                /* Specular sunlight glint sparkles */
                if ((px * 13 + py * 17 + v * 31) % 43 == 0) {
                    r = 230; g = 245; b = 255;
                }
                if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
                s_tile_water[v][idx] = COLOR_RGB(r, g, b);
            }
        }
    }
    
    /* 6. Cart Path (Macadam with Worn Wheel Tracks) */
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
                int r = 165 + noise, g = 162 + noise, b = 158 + noise;
                /* Wheel tracks */
                int track_d1 = abs(px - 22);
                int track_d2 = abs(px - 42);
                if (track_d1 <= 2 || track_d2 <= 2) {
                    r -= 14; g -= 14; b -= 14;
                }
                uint32_t col = COLOR_RGB(r, g, b);
                if (dist_edge < 4) {
                    col = blend_color(COLOR_RGB(110, 105, 95), col, (dist_edge * 256) / 4);
                }
                s_tile_cartpath[m][idx] = col;
            }
        }
    }
    
    /* 7. Wooden Bridge (Stained Planks with Brass Bolts) */
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
                if ((py % 3) == 0) { r -= 35; g -= 28; b -= 20; } /* gap */
                /* Side railings & brass bolts */
                if (px <= 4 || px >= TILE_W - 5) {
                    r += 30; g += 20; b += 14;
                    if ((py % 6) == 2 && (px == 2 || px == TILE_W - 3)) {
                        r = 230; g = 195; b = 50; /* Brass bolt */
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
            s_tile_tee[idx] = COLOR_RGB(94 + dither, 172 + dither, 46);
        }
    }
    
    /* 9. Stratified Sedimentary Cliff Skirts */
    for (int y = 0; y < 72; ++y) {
        for (int x = 0; x < 64; ++x) {
            int idx = y * 64 + x;
            int layer = (y / 6) % 4;
            int r_sw, g_sw, b_sw;
            if (layer == 0) { r_sw = 112; g_sw = 104; b_sw = 88; }      /* Sandstone */
            else if (layer == 1) { r_sw = 90; g_sw = 98; b_sw = 110; }   /* Slate */
            else if (layer == 2) { r_sw = 126; g_sw = 120; b_sw = 100; } /* Limestone */
            else { r_sw = 75; g_sw = 78; b_sw = 82; }                   /* Shale */
            
            int noise = (trand() % 12) - 6;
            r_sw += noise; g_sw += noise; b_sw += noise;
            /* Vertical strata fissure lines */
            if ((x * 19 + y * 7) % 29 == 0) {
                r_sw -= 25; g_sw -= 25; b_sw -= 25;
            }
            if (y < 4) { r_sw -= 25; g_sw += 28; b_sw -= 20; } /* Hanging moss */
            s_cliff_sw[idx] = COLOR_RGB(r_sw, g_sw, b_sw);
            
            /* SE Face in directional hill shadow */
            int r_se = (r_sw * 66) / 100;
            int g_se = (g_sw * 66) / 100;
            int b_se = (b_sw * 72) / 100;
            s_cliff_se[idx] = COLOR_RGB(r_se, g_se, b_se);
        }
    }
    
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

bool screen_to_grid(const GolfState *gs, int32_t sx, int32_t sy, int32_t cam_x, int32_t cam_y, int32_t *out_gx, int32_t *out_gy) {
    for (int elev = MAX_ELEVATION_TIER; elev >= 0; --elev) {
        int32_t adj_sy = sy + (elev * HEIGHT_STEP_PX) - cam_y;
        int32_t adj_sx = sx - cam_x;
        
        int32_t gx = (adj_sx / TILE_HALF_W + adj_sy / TILE_HALF_H) / 2;
        int32_t gy = (adj_sy / TILE_HALF_H - adj_sx / TILE_HALF_W) / 2;
        
        if (gx >= 0 && gx < GRID_W && gy >= 0 && gy < GRID_H) {
            if (gs->grid[gx][gy].elevation == elev) {
                *out_gx = gx;
                *out_gy = gy;
                return true;
            }
        }
    }
    
    /* Fallback at elevation 0 */
    int32_t adj_sy = sy - cam_y;
    int32_t adj_sx = sx - cam_x;
    int32_t gx = (adj_sx / TILE_HALF_W + adj_sy / TILE_HALF_H) / 2;
    int32_t gy = (adj_sy / TILE_HALF_H - adj_sx / TILE_HALF_W) / 2;
    if (gx >= 0 && gx < GRID_W && gy >= 0 && gy < GRID_H) {
        *out_gx = gx;
        *out_gy = gy;
        return true;
    }
    return false;
}

bool screen_to_world_fixed(const GolfState *gs, int32_t sx, int32_t sy, int32_t cam_x, int32_t cam_y, fixed *out_fx, fixed *out_fy) {
    int32_t gx, gy;
    if (screen_to_grid(gs, sx, sy, cam_x, cam_y, &gx, &gy)) {
        *out_fx = TO_FIXED(gx) + FIXED_HALF;
        *out_fy = TO_FIXED(gy) + FIXED_HALF;
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
                            if (px + 1 < SCREEN_W && py + 1 < SCREEN_H) {
                                rc->framebuffer[(py + 1) * SCREEN_W + px + 1] = COLOR_RGB(10, 15, 20);
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
                                    if (px + 2 < SCREEN_W && py + 2 < SCREEN_H) {
                                        rc->framebuffer[(py + 2) * SCREEN_W + px + 2] = COLOR_RGB(10, 15, 20);
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
    DRAWABLE_BEV_CART
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
/* MAIN RENDER FRAME                                                         */
/* ========================================================================= */

void render_frame(RenderContext *rc, const GolfState *gs) {
    rc->tick_count++;
    
    if (gs->mode == MODE_SPLASH) {
        render_splash_screen(rc, rc->tick_count);
        return;
    }
    
    /* 1. Clear Framebuffer to Sky Backdrop & Depth Buffer */
    uint32_t sky_col = COLOR_RGB(34, 46, 58);
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
            /* SW Edge: neighbor is in +Y direction (x, y + 1) */
            int32_t sw_elev = (y < GRID_H - 1) ? gs->grid[x][y + 1].elevation : 0;
            if (tile->elevation > sw_elev) {
                int drop_h = (tile->elevation - sw_elev) * HEIGHT_STEP_PX;
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
            int32_t se_elev = (x < GRID_W - 1) ? gs->grid[x + 1][y].elevation : 0;
            if (tile->elevation > se_elev) {
                int drop_h = (tile->elevation - se_elev) * HEIGHT_STEP_PX;
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
            if (tile->surface == SURF_FAIRWAY) tile_pixels = s_tile_fairway[tile->autotile_mask];
            else if (tile->surface == SURF_GREEN) tile_pixels = s_tile_green[tile->autotile_mask];
            else if (tile->surface == SURF_SAND) tile_pixels = s_tile_sand[tile->autotile_mask];
            else if (tile->surface == SURF_WATER) tile_pixels = s_tile_water[(rc->tick_count / 8) % 4];
            else if (tile->surface == SURF_CARTPATH) tile_pixels = s_tile_cartpath[tile->autotile_mask];
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
        item->foot_sx = FROM_FIXED(FMUL(gs->caddy.x - gs->caddy.y, TO_FIXED(TILE_HALF_W))) + rc->camera_x + TILE_HALF_W;
        item->foot_sy = FROM_FIXED(FMUL(gs->caddy.x + gs->caddy.y, TO_FIXED(TILE_HALF_H))) - FROM_FIXED(gs->caddy.current_z) + rc->camera_y + TILE_HALF_H;
    }
    
    /* 5. Golfer */
    if (gs->mode == MODE_PLAY && s_drawable_count < MAX_DRAWABLES) {
        DrawableItem *item = &s_drawables[s_drawable_count++];
        item->type = DRAWABLE_GOLFER;
        item->sort_key = FROM_FIXED(FMUL(gs->golfer.x + gs->golfer.y, TO_FIXED(32)));
        item->foot_sx = FROM_FIXED(FMUL(gs->golfer.x - gs->golfer.y, TO_FIXED(TILE_HALF_W))) + rc->camera_x + TILE_HALF_W;
        item->foot_sy = FROM_FIXED(FMUL(gs->golfer.x + gs->golfer.y, TO_FIXED(TILE_HALF_H))) - FROM_FIXED(gs->golfer.current_z) + rc->camera_y + TILE_HALF_H;
    }
    
    /* 6. Ball */
    if (gs->mode == MODE_PLAY && s_drawable_count < MAX_DRAWABLES) {
        DrawableItem *item = &s_drawables[s_drawable_count++];
        item->type = DRAWABLE_BALL;
        item->sort_key = FROM_FIXED(FMUL(gs->ball.x + gs->ball.y, TO_FIXED(32)));
        item->foot_sx = FROM_FIXED(FMUL(gs->ball.x - gs->ball.y, TO_FIXED(TILE_HALF_W))) + rc->camera_x + TILE_HALF_W;
        item->foot_sy = FROM_FIXED(FMUL(gs->ball.x + gs->ball.y, TO_FIXED(TILE_HALF_H))) - FROM_FIXED(gs->ball.ground_z) + rc->camera_y + TILE_HALF_H;
    }
    
    /* 7. Beverage Cart Girl */
    if (gs->bev_cart.active && s_drawable_count < MAX_DRAWABLES) {
        DrawableItem *item = &s_drawables[s_drawable_count++];
        item->type = DRAWABLE_BEV_CART;
        item->sort_key = FROM_FIXED(FMUL(gs->bev_cart.x + gs->bev_cart.y, TO_FIXED(32)));
        item->foot_sx = FROM_FIXED(FMUL(gs->bev_cart.x - gs->bev_cart.y, TO_FIXED(TILE_HALF_W))) + rc->camera_x + TILE_HALF_W;
        item->foot_sy = FROM_FIXED(FMUL(gs->bev_cart.x + gs->bev_cart.y, TO_FIXED(TILE_HALF_H))) - FROM_FIXED(gs->bev_cart.current_z) + rc->camera_y + TILE_HALF_H;
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
                render_clubhouse(rc, item->foot_sx, item->foot_sy);
                break;
            }
            case DRAWABLE_FEATURE: {
                int feat = item->data;
                int fx = item->foot_sx;
                int fy = item->foot_sy;
                
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
                const uint32_t *c_spr = s_sprite_caddy;
                if (gs->caddy.state == CADDY_STATE_FOLLOWING) {
                    c_spr = s_sprite_caddy_walk[gs->caddy.walk_anim_frame % 6];
                } else if (gs->caddy.state == CADDY_STATE_CELEBRATING) {
                    c_spr = s_sprite_caddy_celebrate;
                }
                
                draw_drop_shadow(rc, fx, fy, 9, 4, 125);
                blit_sprite_depth(rc, fx - SPRITE_CADDY_W / 2, fy - SPRITE_CADDY_H + 4,
                                  SPRITE_CADDY_W, SPRITE_CADDY_H, c_spr, item->sort_key);
                break;
            }
            case DRAWABLE_GOLFER: {
                int fx = item->foot_sx;
                int fy = item->foot_sy;
                const uint32_t *g_spr = (gs->golfer.state == GOLFER_STATE_WALKING) ?
                                         s_sprite_golfer_walk[gs->golfer.walk_anim_frame % 6] : s_sprite_golfer;
                
                draw_drop_shadow(rc, fx, fy, 9, 4, 130);
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
                    int bub_w = 110;
                    int bub_h = 20;
                    int bub_x = fx - bub_w / 2;
                    int bub_y = fy - SPRITE_BEV_CART_H - 24;
                    /* White speech bubble with dark border */
                    draw_rect(rc, bub_x, bub_y, bub_w, bub_h, COLOR_RGB(255, 255, 255));
                    draw_rect_border(rc, bub_x, bub_y, bub_w, bub_h, COLOR_RGB(35, 45, 55));
                    /* Speech pointer */
                    for (int dy = 0; dy < 5; ++dy) {
                        draw_rect(rc, fx - 2 + dy / 2, bub_y + bub_h + dy, 5 - dy, 1, COLOR_RGB(255, 255, 255));
                    }
                    draw_text(rc, bub_x + 6, bub_y + 6, "♪ COLD DRINKS! ♪", COLOR_RGB(220, 20, 70), false);
                }
                break;
            }
        }
    }
    
    /* 3. Aim Line & Reticle in Play Mode */
    if (gs->mode == MODE_PLAY && gs->ball.state == BALL_IDLE) {
        int bsx = FROM_FIXED(FMUL(gs->ball.x - gs->ball.y, TO_FIXED(TILE_HALF_W))) + rc->camera_x + TILE_HALF_W;
        int bsy = FROM_FIXED(FMUL(gs->ball.x + gs->ball.y, TO_FIXED(TILE_HALF_H))) - FROM_FIXED(gs->ball.ground_z) + rc->camera_y + TILE_HALF_H;
        
        fixed gz_aim = golf_get_ground_z_fixed(gs, gs->aim_target_x, gs->aim_target_y);
        int asx = FROM_FIXED(FMUL(gs->aim_target_x - gs->aim_target_y, TO_FIXED(TILE_HALF_W))) + rc->camera_x + TILE_HALF_W;
        int asy = FROM_FIXED(FMUL(gs->aim_target_x + gs->aim_target_y, TO_FIXED(TILE_HALF_H))) - FROM_FIXED(gz_aim) + rc->camera_y + TILE_HALF_H;
        
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
    snprintf(stat_buf, sizeof(stat_buf), "HOLE 1  |  PAR %d  |  %d YDS", gs->par, gs->hole_yards);
    draw_text(rc, 210, 10, stat_buf, COLOR_RGB(240, 245, 250), true);
    char stroke_buf[64];
    snprintf(stroke_buf, sizeof(stroke_buf), "STROKE %d", gs->strokes);
    draw_text(rc, 210, 26, stroke_buf, COLOR_RGB(200, 220, 240), true);
    
    /* SimGolf Fun Rating Stars */
    int stars = (gs->fun_rating * 5) / 100;
    char star_buf[64];
    snprintf(star_buf, sizeof(star_buf), "FUN: %d/5 STARS (%d%%)", stars, gs->fun_rating);
    draw_text(rc, 460, 10, star_buf, COLOR_RGB(255, 215, 65), true);
    
    /* Economy Funds */
    char fund_buf[64];
    snprintf(fund_buf, sizeof(fund_buf), "FUNDS: $%d", gs->club_funds);
    draw_text(rc, 460, 26, fund_buf, COLOR_RGB(115, 225, 115), true);
    
    /* Top Bar Buttons: Randomize Hole and Mode Selector */
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
            draw_text(rc, sg_x + 55, sg_y + 32, "SPACE OR CLICK BALL TO START SWING", COLOR_RGB(255, 235, 90), true);
        } else if (gs->swing.state == SWING_STATE_POWER) {
            draw_text(rc, sg_x + 85, sg_y + 32, "CLICK TO LOCK POWER (0-100%)", COLOR_RGB(255, 245, 120), true);
        } else if (gs->swing.state == SWING_STATE_ACCURACY) {
            draw_text(rc, sg_x + 75, sg_y + 32, "CLICK IN GOLD TO SNAP SWEET SPOT!", COLOR_RGB(255, 205, 45), true);
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
    
    /* 5. Rolling Emerald Fairway Turf & Hazards (y = 420..768) */
    for (int y = 420; y < SCREEN_H; ++y) {
        float t = (float)(y - 420) / (float)(SCREEN_H - 420);
        for (int x = 0; x < SCREEN_W; ++x) {
            /* Rolling contour hill curve */
            int hill_offset = (int)(sinf((float)x * 0.006f) * 28.0f);
            if (y < 420 + hill_offset) continue;
            
            /* Fairway stripes */
            int stripe = ((x + y) / 24) & 1;
            int r = stripe ? (int)(55.0f * (1.0f - t) + 42.0f * t) : (int)(45.0f * (1.0f - t) + 34.0f * t);
            int g = stripe ? (int)(155.0f * (1.0f - t) + 115.0f * t) : (int)(135.0f * (1.0f - t) + 98.0f * t);
            int b = stripe ? (int)(45.0f * (1.0f - t) + 32.0f * t) : (int)(38.0f * (1.0f - t) + 26.0f * t);
            rc->framebuffer[y * SCREEN_W + x] = COLOR_RGB(r, g, b);
        }
    }
    
    /* Water Hazard Lake on Left (x = 30..260, y = 460..540) */
    for (int dy = -35; dy <= 35; ++dy) {
        int py = 495 + dy;
        if (py < 0 || py >= SCREEN_H) continue;
        for (int dx = -110; dx <= 110; ++dx) {
            int px = 145 + dx;
            if (px < 0 || px >= SCREEN_W) continue;
            if (dx * dx * 2 + dy * dy * 18 <= 110 * 110 * 2) {
                int wave = (int)(sinf((float)px * 0.1f + (float)tick_count * 0.08f) * 1.5f);
                rc->framebuffer[(py + wave) * SCREEN_W + px] = COLOR_RGB(42, 95, 165);
            }
        }
    }
    
    /* White Sand Bunker on Right (x = 690..790, y = 460..510) */
    for (int dy = -22; dy <= 22; ++dy) {
        int py = 485 + dy;
        if (py < 0 || py >= SCREEN_H) continue;
        for (int dx = -50; dx <= 50; ++dx) {
            int px = 740 + dx;
            if (px < 0 || px >= SCREEN_W) continue;
            if (dx * dx + dy * dy * 5 <= 50 * 50) {
                int shade = (dy < -12) ? -28 : 12;
                rc->framebuffer[py * SCREEN_W + px] = COLOR_RGB(225 + shade, 210 + shade, 160 + shade);
            }
        }
    }
    
    /* 6. Grand Clubhouse on the Bluff (x = 85..235, y = 370..445) */
    render_clubhouse(rc, 160, 440);
    
    /* Flagstick on Putting Green (x = 840, y = 450) */
    draw_rect(rc, 840, 415, 2, 35, COLOR_RGB(230, 235, 240));
    draw_rect(rc, 842, 415, 14, 8, COLOR_RGB(225, 45, 45)); /* Red flag */
    
    /* Ponderosa Pines along ridge */
    blit_sprite_depth(rc, 40, 390, SPRITE_PINE_W, SPRITE_PINE_H, s_sprite_pine, 999999);
    blit_sprite_depth(rc, 240, 400, SPRITE_OAK_W, SPRITE_OAK_H, s_sprite_oak, 999999);
    blit_sprite_depth(rc, 910, 420, SPRITE_PALM_W, SPRITE_PALM_H, s_sprite_palm, 999999);
    blit_sprite_depth(rc, 870, 450, SPRITE_FLOWERING_W, SPRITE_FLOWERING_H, s_sprite_flowering, 999999);
    
    /* 7. Foreground Golfer on Left (x = 240, y = 520) */
    draw_drop_shadow(rc, 254, 555, 14, 6, 140);
    blit_sprite_depth(rc, 240, 520, SPRITE_GOLFER_W, SPRITE_GOLFER_H, s_sprite_golfer, 999999);
    draw_text(rc, 222, 563, "CHAMPION", COLOR_RGB(255, 235, 110), true);
    
    /* 8. Foreground Beverage Cart Girl on Right (x = 680, y = 520) */
    draw_drop_shadow(rc, 703, 556, 26, 10, 140);
    blit_sprite_depth(rc, 680, 520, SPRITE_BEV_CART_W, SPRITE_BEV_CART_H, s_sprite_bev_cart, 999999);
    
    /* Floating Melodic Whistle Speech Bubble above Beverage Cart */
    int bub_x = 645;
    int bub_y = 485;
    draw_rect(rc, bub_x, bub_y, 115, 22, COLOR_RGB(255, 255, 255));
    draw_rect_border(rc, bub_x, bub_y, 115, 22, COLOR_RGB(35, 45, 55));
    for (int dy = 0; dy < 6; ++dy) {
        draw_rect(rc, bub_x + 55 + dy / 2, bub_y + 22 + dy, 6 - dy, 1, COLOR_RGB(255, 255, 255));
    }
    draw_text(rc, bub_x + 8, bub_y + 7, "♪ COLD DRINKS! ♪", COLOR_RGB(220, 20, 80), false);
    
    /* 9. Hero Title Logo: "DUFFER PRO" with 3D Embossing */
    int title_y = 65;
    const char *title_text = "DUFFER PRO";
    int title_scale = 4;
    int title_w = (int)strlen(title_text) * 8 * title_scale;
    int title_x = SCREEN_W / 2 - title_w / 2;
    
    /* Title Backing Plaque (Translucent Smoked Glass with Gold Filigree) */
    int plq_w = 640;
    int plq_h = 100;
    int plq_x = SCREEN_W / 2 - plq_w / 2;
    int plq_y = title_y - 12;
    draw_rect_alpha(rc, plq_x, plq_y, plq_w, plq_h, COLOR_RGB(16, 24, 32), 225);
    draw_rect_border(rc, plq_x, plq_y, plq_w, plq_h, COLOR_RGB(255, 215, 75));
    draw_rect_border(rc, plq_x + 3, plq_y + 3, plq_w - 6, plq_h - 6, COLOR_RGB(165, 125, 35));
    
    /* 3D Drop Shadow */
    draw_text_scaled(rc, title_x + 4, title_y + 4, title_text, COLOR_RGB(35, 20, 10), title_scale, false);
    /* Specular Bevel Top Highlight */
    draw_text_scaled(rc, title_x - 1, title_y - 1, title_text, COLOR_RGB(255, 255, 235), title_scale, false);
    /* Metallic Gold Face */
    draw_text_scaled(rc, title_x, title_y, title_text, COLOR_RGB(255, 215, 65), title_scale, false);
    
    /* PGA Championship Ribbon Banner at y = 125 */
    int ban_w = 460;
    int ban_h = 24;
    int ban_x = SCREEN_W / 2 - ban_w / 2;
    int ban_y = title_y + 42;
    draw_rect(rc, ban_x, ban_y, ban_w, ban_h, COLOR_RGB(24, 75, 42));
    draw_rect_border(rc, ban_x, ban_y, ban_w, ban_h, COLOR_RGB(245, 215, 75));
    const char *ribbon_text = "★  CHAMPIONSHIP RESORT GOLF  ★";
    int r_tx = SCREEN_W / 2 - (int)strlen(ribbon_text) * 8 / 2;
    draw_text(rc, r_tx, ban_y + 8, ribbon_text, COLOR_RGB(255, 245, 140), true);
    
    /* Subtitle */
    const char *sub_text = "The Definitive Isometric Country Club Simulation";
    int s_tx = SCREEN_W / 2 - (int)strlen(sub_text) * 8 / 2;
    draw_text(rc, s_tx, plq_y + plq_h + 10, sub_text, COLOR_RGB(255, 235, 145), true);
    
    /* 10. Three Translucent Feature Cards (y = 575..630) */
    int card_w = 270;
    int card_h = 52;
    int card_y = 575;
    
    /* Card 1: Terrain Sculpting */
    int c1_x = 75;
    draw_rect_alpha(rc, c1_x, card_y, card_w, card_h, COLOR_RGB(18, 28, 38), 215);
    draw_rect_border(rc, c1_x, card_y, card_w, card_h, COLOR_RGB(75, 125, 165));
    draw_text(rc, c1_x + 12, card_y + 8, "[ 6-TIER 3D SCULPTING ]", COLOR_RGB(255, 215, 75), true);
    draw_text(rc, c1_x + 12, card_y + 24, "Raise, lower cliffs & autotile", COLOR_RGB(210, 225, 235), false);
    draw_text(rc, c1_x + 12, card_y + 36, "bunkers, greens & water hazards", COLOR_RGB(165, 185, 205), false);
    
    /* Card 2: Living Resort */
    int c2_x = 377;
    draw_rect_alpha(rc, c2_x, card_y, card_w, card_h, COLOR_RGB(18, 28, 38), 215);
    draw_rect_border(rc, c2_x, card_y, card_w, card_h, COLOR_RGB(75, 125, 165));
    draw_text(rc, c2_x + 12, card_y + 8, "[ LIVING RESORT LIFE ]", COLOR_RGB(255, 215, 75), true);
    draw_text(rc, c2_x + 12, card_y + 24, "Clubhouse, caddy, gallery crowd", COLOR_RGB(210, 225, 235), false);
    draw_text(rc, c2_x + 12, card_y + 36, "cheers & country club funds", COLOR_RGB(165, 185, 205), false);
    
    /* Card 3: Beverage Cart */
    int c3_x = 679;
    draw_rect_alpha(rc, c3_x, card_y, card_w, card_h, COLOR_RGB(18, 28, 38), 215);
    draw_rect_border(rc, c3_x, card_y, card_w, card_h, COLOR_RGB(75, 125, 165));
    draw_text(rc, c3_x + 12, card_y + 8, "[ BEVERAGE CART SERVICE ]", COLOR_RGB(255, 215, 75), true);
    draw_text(rc, c3_x + 12, card_y + 24, "Roving cart girl sells drinks &", COLOR_RGB(210, 225, 235), false);
    draw_text(rc, c3_x + 12, card_y + 36, "whistles cheerful golf melodies", COLOR_RGB(165, 185, 205), false);
    
    /* 11. Prominent Interactive START BUTTON & Guidance (y = 646..735) */
    int btn_w = 440;
    int btn_h = 44;
    int btn_x = SCREEN_W / 2 - btn_w / 2;
    int btn_y = 646;
    
    /* Pulsing button glow */
    bool pulse = ((tick_count / 24) % 2 == 0);
    uint32_t btn_bg = pulse ? COLOR_RGB(34, 105, 58) : COLOR_RGB(24, 82, 45);
    uint32_t btn_border = pulse ? COLOR_RGB(255, 235, 95) : COLOR_RGB(225, 185, 55);
    
    draw_rect(rc, btn_x, btn_y, btn_w, btn_h, btn_bg);
    draw_rect_border(rc, btn_x, btn_y, btn_w, btn_h, btn_border);
    draw_rect_border(rc, btn_x + 2, btn_y + 2, btn_w - 4, btn_h - 4, btn_border);
    
    const char *btn_label = ">>  PRESS ENTER TO TEE OFF  <<";
    int btx = SCREEN_W / 2 - (int)strlen(btn_label) * 8 / 2;
    draw_text(rc, btx, btn_y + 17, btn_label, COLOR_RGB(255, 250, 160), true);
    
    /* Explicit Clear Instruction */
    const char *note_text = "Press ENTER (or click button) to start  |  Spacebar is reserved for swings during play";
    int ntx = SCREEN_W / 2 - (int)strlen(note_text) * 8 / 2;
    draw_text(rc, ntx, btn_y + 54, note_text, COLOR_RGB(255, 225, 130), true);
    
    const char *keys_hint = "Controls: TAB Switch Modes | Mouse Aim & Drag | Arrow Keys Pan Camera | C Change Club";
    int ktx = SCREEN_W / 2 - (int)strlen(keys_hint) * 8 / 2;
    draw_text(rc, ktx, btn_y + 70, keys_hint, COLOR_RGB(155, 180, 200), false);
    
    /* Outer screen vignette border */
    draw_rect_border(rc, 0, 0, SCREEN_W, SCREEN_H, COLOR_RGB(35, 45, 55));
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
