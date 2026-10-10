#include "golf_render.h"
#include "font8x8.h"
#include "splash_art.h"
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

/* Dynamic Scenery Sprites */
#define SPRITE_PINE_W 34
#define SPRITE_PINE_H 54
static uint32_t s_sprite_pine[SPRITE_PINE_W * SPRITE_PINE_H];

#define SPRITE_OAK_W  44
#define SPRITE_OAK_H  50
static uint32_t s_sprite_oak[SPRITE_OAK_W * SPRITE_OAK_H];

#define SPRITE_PALM_W 40
#define SPRITE_PALM_H 56
static uint32_t s_sprite_palm[SPRITE_PALM_W * SPRITE_PALM_H];

#define SPRITE_FLOWERING_W 42
#define SPRITE_FLOWERING_H 48
static uint32_t s_sprite_flowering[SPRITE_FLOWERING_W * SPRITE_FLOWERING_H];

#define SPRITE_BUSH_W 26
#define SPRITE_BUSH_H 22
static uint32_t s_sprite_bush[SPRITE_BUSH_W * SPRITE_BUSH_H];

#define SPRITE_FLOWERS_W 28
#define SPRITE_FLOWERS_H 18
static uint32_t s_sprite_flowers[SPRITE_FLOWERS_W * SPRITE_FLOWERS_H];

#define SPRITE_ROCK_W 28
#define SPRITE_ROCK_H 22
static uint32_t s_sprite_rock[SPRITE_ROCK_W * SPRITE_ROCK_H];

#define SPRITE_BENCH_W 26
#define SPRITE_BENCH_H 18
static uint32_t s_sprite_bench[SPRITE_BENCH_W * SPRITE_BENCH_H];

#define SPRITE_CART_W 28
#define SPRITE_CART_H 26
static uint32_t s_sprite_cart[SPRITE_CART_W * SPRITE_CART_H];

#define SPRITE_FLAG_W 18
#define SPRITE_FLAG_H 44
static uint32_t s_sprite_flag[4][SPRITE_FLAG_W * SPRITE_FLAG_H];

/* Characters & People */
#define SPRITE_GOLFER_W 24
#define SPRITE_GOLFER_H 34
static uint32_t s_sprite_golfer[SPRITE_GOLFER_W * SPRITE_GOLFER_H];
static uint32_t s_sprite_golfer_walk[4][SPRITE_GOLFER_W * SPRITE_GOLFER_H];

#define SPRITE_CADDY_W 24
#define SPRITE_CADDY_H 34
static uint32_t s_sprite_caddy[SPRITE_CADDY_W * SPRITE_CADDY_H];
static uint32_t s_sprite_caddy_walk[4][SPRITE_CADDY_W * SPRITE_CADDY_H];

#define SPRITE_SPEC_W 18
#define SPRITE_SPEC_H 28
static uint32_t s_sprite_spec[5][4][SPRITE_SPEC_W * SPRITE_SPEC_H]; /* [shirt color][anim frame] */

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

/* Procedural Texture Generation for Duffer-Pro */
static void generate_procedural_graphics(void) {
    s_tex_rand = 54321;
    
    /* 1. Rough Grass Variants */
    for (int v = 0; v < 4; ++v) {
        for (int py = 0; py < TILE_H; ++py) {
            for (int px = 0; px < TILE_W; ++px) {
                int idx = py * TILE_W + px;
                if (!is_in_diamond(px, py, NULL)) {
                    s_tile_rough[v][idx] = 0;
                    continue;
                }
                int noise = (trand() % 16) - 8;
                int dither = ((px ^ py) & 1) ? 3 : -3;
                int r = 48 + noise + dither;
                int g = 98 + noise * 2 + dither * 2;
                int b = 32 + noise / 2;
                if (r < 25) r = 25; if (r > 75) r = 75;
                if (g < 65) g = 65; if (g > 135) g = 135;
                if (b < 15) b = 15; if (b > 50) b = 50;
                s_tile_rough[v][idx] = COLOR_RGB(r, g, b);
            }
        }
    }
    
    /* 2. Championship Fairway with Alternating Mower Stripes */
    for (int m = 0; m < 16; ++m) {
        for (int py = 0; py < TILE_H; ++py) {
            for (int px = 0; px < TILE_W; ++px) {
                int idx = py * TILE_W + px;
                int dist_edge = 0;
                if (!is_in_diamond(px, py, &dist_edge)) {
                    s_tile_fairway[m][idx] = 0;
                    continue;
                }
                int stripe = ((px + py * 2) / 8) & 1;
                int r = stripe ? 96 : 82;
                int g = stripe ? 172 : 154;
                int b = stripe ? 46 : 38;
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
    
    /* 3. Putting Green */
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
                uint32_t gr_col = COLOR_RGB(104 + grain, 204 + grain * 2, 60 + grain);
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
                uint32_t sand_col = COLOR_RGB(226 + wave + noise, 206 + wave + noise, 142 + wave / 2);
                if (dist_edge < 5) {
                    int t256 = (dist_edge * 256) / 5;
                    sand_col = blend_color(COLOR_RGB(120, 100, 60), sand_col, t256);
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
                if (!is_in_diamond(px, py, NULL)) {
                    s_tile_water[v][idx] = 0;
                    continue;
                }
                float wave = sinf((float)px * 0.22f + (float)v * 1.57f) * 6.0f +
                             cosf((float)py * 0.35f - (float)v * 1.57f) * 4.0f;
                int r = 38 + (int)wave;
                int g = 112 + (int)(wave * 1.5f);
                int b = 186 + (int)(wave * 2.0f);
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
                int noise = (trand() % 12) - 6;
                int r = 168 + noise, g = 166 + noise, b = 162 + noise;
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
                /* Planks running horizontally */
                int plank_idx = (py / 3) % 2;
                int r = plank_idx ? 138 : 124;
                int g = plank_idx ? 92 : 82;
                int b = plank_idx ? 54 : 46;
                if ((py % 3) == 0) { r -= 30; g -= 24; b -= 16; } /* plank gap */
                /* Side railings */
                if (px < 4 || px > TILE_W - 5) { r += 25; g += 18; b += 12; }
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
            int dither = ((px ^ py) & 1) ? 2 : -2;
            s_tile_tee[idx] = COLOR_RGB(90 + dither, 165 + dither, 42);
        }
    }
    
    /* 9. Vertical Cliff Skirts */
    for (int y = 0; y < 72; ++y) {
        for (int x = 0; x < 64; ++x) {
            int idx = y * 64 + x;
            int strata = (int)(sinf((float)y * 0.4f) * 12.0f);
            int noise = (trand() % 16) - 8;
            int r_sw = 105 + strata + noise;
            int g_sw = 95 + strata + noise;
            int b_sw = 80 + strata + noise;
            if (y < 4) { r_sw -= 20; g_sw += 15; b_sw -= 15; } /* moss top */
            s_cliff_sw[idx] = COLOR_RGB(r_sw, g_sw, b_sw);
            
            /* SE face is in shadow (Sun is NW) */
            int r_se = (r_sw * 68) / 100;
            int g_se = (g_sw * 68) / 100;
            int b_se = (b_sw * 75) / 100;
            s_cliff_se[idx] = COLOR_RGB(r_se, g_se, b_se);
        }
    }
    
    /* 10. Scots Pine Sprite */
    memset(s_sprite_pine, 0, sizeof(s_sprite_pine));
    for (int y = 0; y < SPRITE_PINE_H; ++y) {
        for (int x = 0; x < SPRITE_PINE_W; ++x) {
            int idx = y * SPRITE_PINE_W + x;
            int cx = SPRITE_PINE_W / 2;
            /* Trunk */
            if (y >= 38 && x >= cx - 2 && x <= cx + 2) {
                int bark = ((x ^ y) & 1) ? 8 : -8;
                s_sprite_pine[idx] = COLOR_RGB(92 + bark, 64 + bark, 42);
                continue;
            }
            /* 3 tiers of conical pine boughs */
            int bough_tier = (y < 16) ? 0 : ((y < 28) ? 1 : ((y < 40) ? 2 : -1));
            if (bough_tier >= 0) {
                int tier_top = (bough_tier == 0) ? 2 : ((bough_tier == 1) ? 14 : 26);
                int tier_h = (bough_tier == 0) ? 14 : ((bough_tier == 1) ? 14 : 14);
                int rel_y = y - tier_top;
                int max_w = (bough_tier == 0) ? 8 : ((bough_tier == 1) ? 12 : 16);
                int w_at_y = (rel_y * max_w) / tier_h;
                if (abs(x - cx) <= w_at_y) {
                    int light = (x < cx) ? 18 : -14;
                    int noise = (trand() % 12) - 6;
                    s_sprite_pine[idx] = COLOR_RGB(34 + light + noise, 78 + light * 2 + noise, 38 + light);
                }
            }
        }
    }
    
    /* 11. English Oak Sprite */
    memset(s_sprite_oak, 0, sizeof(s_sprite_oak));
    for (int y = 0; y < SPRITE_OAK_H; ++y) {
        for (int x = 0; x < SPRITE_OAK_W; ++x) {
            int idx = y * SPRITE_OAK_W + x;
            int cx = SPRITE_OAK_W / 2;
            if (y >= 34 && x >= cx - 3 && x <= cx + 3) {
                s_sprite_oak[idx] = COLOR_RGB(86, 58, 38);
                continue;
            }
            int cdx = x - cx;
            int cdy = y - 20;
            if (cdx * cdx * 10 + cdy * cdy * 16 <= 18 * 18 * 10) {
                int light = (x < cx && y < 22) ? 25 : ((x > cx) ? -18 : 0);
                int noise = (trand() % 14) - 7;
                s_sprite_oak[idx] = COLOR_RGB(56 + light + noise, 118 + light * 2 + noise, 42 + light);
            }
        }
    }
    
    /* 12. Royal Palm Sprite */
    memset(s_sprite_palm, 0, sizeof(s_sprite_palm));
    for (int y = 0; y < SPRITE_PALM_H; ++y) {
        for (int x = 0; x < SPRITE_PALM_W; ++x) {
            int idx = y * SPRITE_PALM_W + x;
            int cx = SPRITE_PALM_W / 2;
            /* Curved trunk */
            int tx = cx - (int)(sinf((float)y * 0.08f) * 4.0f);
            if (y >= 18 && abs(x - tx) <= 2) {
                int ring = (y % 4 == 0) ? -15 : 10;
                s_sprite_palm[idx] = COLOR_RGB(138 + ring, 110 + ring, 78);
                continue;
            }
            /* Palm fronds radiating from top (y=16) */
            if (y < 24) {
                int fx = x - tx;
                int fy = y - 16;
                float d = sqrtf((float)(fx * fx + fy * fy));
                if (d < 18.0f && fabsf(fy) < 14.0f) {
                    s_sprite_palm[idx] = COLOR_RGB(46, 126, 44);
                }
            }
        }
    }
    
    /* 13. Flowering Cherry Blossom */
    memset(s_sprite_flowering, 0, sizeof(s_sprite_flowering));
    for (int y = 0; y < SPRITE_FLOWERING_H; ++y) {
        for (int x = 0; x < SPRITE_FLOWERING_W; ++x) {
            int idx = y * SPRITE_FLOWERING_W + x;
            int cx = SPRITE_FLOWERING_W / 2;
            if (y >= 32 && x >= cx - 2 && x <= cx + 2) {
                s_sprite_flowering[idx] = COLOR_RGB(75, 48, 36);
                continue;
            }
            int cdx = x - cx;
            int cdy = y - 18;
            if (cdx * cdx * 12 + cdy * cdy * 16 <= 17 * 17 * 12) {
                int blossom = (trand() % 20);
                s_sprite_flowering[idx] = COLOR_RGB(235 + blossom, 155 + blossom, 175 + blossom);
            }
        }
    }
    
    /* 14. Azalea Bush */
    memset(s_sprite_bush, 0, sizeof(s_sprite_bush));
    for (int y = 0; y < SPRITE_BUSH_H; ++y) {
        for (int x = 0; x < SPRITE_BUSH_W; ++x) {
            int idx = y * SPRITE_BUSH_W + x;
            int dx = x - 13, dy = y - 11;
            if (dx * dx * 10 + dy * dy * 14 <= 10 * 10 * 10) {
                if ((trand() % 5) == 0) s_sprite_bush[idx] = COLOR_RGB(228, 55, 115);
                else s_sprite_bush[idx] = COLOR_RGB(48, 108, 42);
            }
        }
    }
    
    /* 15. Wildflower Bed */
    memset(s_sprite_flowers, 0, sizeof(s_sprite_flowers));
    for (int y = 0; y < SPRITE_FLOWERS_H; ++y) {
        for (int x = 0; x < SPRITE_FLOWERS_W; ++x) {
            int idx = y * SPRITE_FLOWERS_W + x;
            int dx = x - 14, dy = y - 9;
            if (dx * dx * 8 + dy * dy * 16 <= 12 * 12 * 8) {
                int fl = trand() % 6;
                if (fl == 0) s_sprite_flowers[idx] = COLOR_RGB(240, 210, 40); /* yellow */
                else if (fl == 1) s_sprite_flowers[idx] = COLOR_RGB(230, 60, 60); /* red */
                else if (fl == 2) s_sprite_flowers[idx] = COLOR_RGB(80, 140, 240); /* blue */
                else s_sprite_flowers[idx] = COLOR_RGB(50, 115, 45); /* stem */
            }
        }
    }
    
    /* 16. Granite Boulder */
    memset(s_sprite_rock, 0, sizeof(s_sprite_rock));
    for (int y = 0; y < SPRITE_ROCK_H; ++y) {
        for (int x = 0; x < SPRITE_ROCK_W; ++x) {
            int idx = y * SPRITE_ROCK_W + x;
            int dx = x - 14, dy = y - 11;
            if (dx * dx * 10 + dy * dy * 14 <= 12 * 12 * 10) {
                int shade = (x < 14) ? 20 : -20;
                int noise = (trand() % 14) - 7;
                s_sprite_rock[idx] = COLOR_RGB(125 + shade + noise, 122 + shade + noise, 118 + shade);
            }
        }
    }
    
    /* 17. Park Bench */
    memset(s_sprite_bench, 0, sizeof(s_sprite_bench));
    for (int y = 0; y < SPRITE_BENCH_H; ++y) {
        for (int x = 0; x < SPRITE_BENCH_W; ++x) {
            int idx = y * SPRITE_BENCH_W + x;
            /* Bench slats */
            if (y >= 4 && y <= 8 && x >= 4 && x <= 22) s_sprite_bench[idx] = COLOR_RGB(145, 75, 40);
            if (y >= 10 && y <= 13 && x >= 3 && x <= 23) s_sprite_bench[idx] = COLOR_RGB(165, 88, 48);
            /* Cast iron legs */
            if (y >= 8 && y <= 16 && (x == 5 || x == 21)) s_sprite_bench[idx] = COLOR_RGB(35, 55, 42);
        }
    }
    
    /* 18. Golf Cart */
    memset(s_sprite_cart, 0, sizeof(s_sprite_cart));
    for (int y = 0; y < SPRITE_CART_H; ++y) {
        for (int x = 0; x < SPRITE_CART_W; ++x) {
            int idx = y * SPRITE_CART_W + x;
            /* White roof */
            if (y >= 2 && y <= 5 && x >= 4 && x <= 24) s_sprite_cart[idx] = COLOR_RGB(240, 242, 245);
            /* Roof struts */
            if (y >= 5 && y <= 14 && (x == 6 || x == 22)) s_sprite_cart[idx] = COLOR_RGB(70, 75, 80);
            /* Windshield */
            if (y >= 8 && y <= 13 && x >= 7 && x <= 12) s_sprite_cart[idx] = COLOR_RGB(190, 220, 240);
            /* White body */
            if (y >= 14 && y <= 21 && x >= 4 && x <= 24) s_sprite_cart[idx] = COLOR_RGB(235, 238, 242);
            /* Golf bags in back */
            if (y >= 10 && y <= 17 && x >= 18 && x <= 23) s_sprite_cart[idx] = COLOR_RGB(160, 40, 40);
            /* Wheels */
            if (y >= 20 && y <= 25 && (x >= 6 && x <= 10)) s_sprite_cart[idx] = COLOR_RGB(30, 30, 30);
            if (y >= 20 && y <= 25 && (x >= 18 && x <= 22)) s_sprite_cart[idx] = COLOR_RGB(30, 30, 30);
        }
    }
    
    /* 19. Waving Pin Flag */
    for (int f = 0; f < 4; ++f) {
        memset(s_sprite_flag[f], 0, sizeof(s_sprite_flag[0]));
        for (int y = 0; y < SPRITE_FLAG_H; ++y) {
            for (int x = 0; x < SPRITE_FLAG_W; ++x) {
                int idx = y * SPRITE_FLAG_W + x;
                /* Metal pole */
                if (x == 3 && y >= 2) s_sprite_flag[f][idx] = COLOR_RGB(220, 225, 230);
                /* Pennant flag */
                if (x >= 4 && x <= 15 && y >= 2 && y <= 12) {
                    int wave = (int)(sinf((float)x * 0.6f + (float)f * 1.57f) * 1.5f);
                    if (y >= 3 + wave && y <= 11 + wave) {
                        /* Red flag with white numeral */
                        s_sprite_flag[f][idx] = COLOR_RGB(225, 45, 45);
                    }
                }
            }
        }
    }
    
    /* 20. Golfer Character (Ready stance and walk frames) */
    memset(s_sprite_golfer, 0, sizeof(s_sprite_golfer));
    for (int y = 0; y < SPRITE_GOLFER_H; ++y) {
        for (int x = 0; x < SPRITE_GOLFER_W; ++x) {
            int idx = y * SPRITE_GOLFER_W + x;
            int cx = 12;
            /* White Cap */
            if (y >= 2 && y <= 5 && abs(x - cx) <= 4) s_sprite_golfer[idx] = COLOR_RGB(245, 245, 245);
            /* Head / Face */
            if (y >= 6 && y <= 9 && abs(x - cx) <= 3) s_sprite_golfer[idx] = COLOR_RGB(238, 192, 155);
            /* Red Polo Shirt */
            if (y >= 10 && y <= 18 && abs(x - cx) <= 5) s_sprite_golfer[idx] = COLOR_RGB(215, 42, 42);
            /* Khaki Shorts */
            if (y >= 19 && y <= 24 && abs(x - cx) <= 4) s_sprite_golfer[idx] = COLOR_RGB(198, 172, 134);
            /* Legs */
            if (y >= 25 && y <= 29 && (abs(x - (cx - 2)) <= 1 || abs(x - (cx + 2)) <= 1)) s_sprite_golfer[idx] = COLOR_RGB(238, 192, 155);
            /* White Golf Shoes */
            if (y >= 30 && y <= 32 && (abs(x - (cx - 2)) <= 2 || abs(x - (cx + 2)) <= 2)) s_sprite_golfer[idx] = COLOR_RGB(240, 240, 240);
        }
    }
    
    /* 4 Walk Animation Frames */
    for (int f = 0; f < 4; ++f) {
        memcpy(s_sprite_golfer_walk[f], s_sprite_golfer, sizeof(s_sprite_golfer));
        /* Leg strides */
        int offset = (f == 1) ? 2 : ((f == 3) ? -2 : 0);
        for (int y = 25; y < SPRITE_GOLFER_H; ++y) {
            for (int x = 0; x < SPRITE_GOLFER_W; ++x) {
                int idx = y * SPRITE_GOLFER_W + x;
                s_sprite_golfer_walk[f][idx] = 0;
            }
        }
        for (int y = 25; y <= 29; ++y) {
            int x1 = 10 - offset, x2 = 14 + offset;
            if (x1 >= 0 && x1 < SPRITE_GOLFER_W) s_sprite_golfer_walk[f][y * SPRITE_GOLFER_W + x1] = COLOR_RGB(238, 192, 155);
            if (x2 >= 0 && x2 < SPRITE_GOLFER_W) s_sprite_golfer_walk[f][y * SPRITE_GOLFER_W + x2] = COLOR_RGB(238, 192, 155);
        }
        for (int y = 30; y <= 32; ++y) {
            int x1 = 10 - offset, x2 = 14 + offset;
            for (int dx = -1; dx <= 1; ++dx) {
                if (x1 + dx >= 0 && x1 + dx < SPRITE_GOLFER_W) s_sprite_golfer_walk[f][y * SPRITE_GOLFER_W + x1 + dx] = COLOR_RGB(240, 240, 240);
                if (x2 + dx >= 0 && x2 + dx < SPRITE_GOLFER_W) s_sprite_golfer_walk[f][y * SPRITE_GOLFER_W + x2 + dx] = COLOR_RGB(240, 240, 240);
            }
        }
    }
    
    /* 21. Caddy Character with Golf Tour Bag */
    memset(s_sprite_caddy, 0, sizeof(s_sprite_caddy));
    for (int y = 0; y < SPRITE_CADDY_H; ++y) {
        for (int x = 0; x < SPRITE_CADDY_W; ++x) {
            int idx = y * SPRITE_CADDY_W + x;
            int cx = 10;
            /* Green Visor */
            if (y >= 2 && y <= 5 && abs(x - cx) <= 3) s_sprite_caddy[idx] = COLOR_RGB(45, 125, 65);
            /* Face */
            if (y >= 6 && y <= 9 && abs(x - cx) <= 3) s_sprite_caddy[idx] = COLOR_RGB(235, 190, 150);
            /* White Polo */
            if (y >= 10 && y <= 18 && abs(x - cx) <= 4) s_sprite_caddy[idx] = COLOR_RGB(235, 238, 242);
            /* Navy Shorts */
            if (y >= 19 && y <= 24 && abs(x - cx) <= 4) s_sprite_caddy[idx] = COLOR_RGB(35, 55, 95);
            /* Legs */
            if (y >= 25 && y <= 29 && (x == cx - 2 || x == cx + 2)) s_sprite_caddy[idx] = COLOR_RGB(235, 190, 150);
            /* Shoes */
            if (y >= 30 && y <= 32 && (abs(x - (cx - 2)) <= 1 || abs(x - (cx + 2)) <= 1)) s_sprite_caddy[idx] = COLOR_RGB(60, 60, 60);
            /* Big Golf Bag over shoulder (x = 16..22, y = 8..26) */
            if (y >= 8 && y <= 24 && x >= 16 && x <= 22) s_sprite_caddy[idx] = COLOR_RGB(25, 45, 85); /* Navy Bag */
            if (y >= 6 && y <= 8 && (x == 17 || x == 19 || x == 21)) s_sprite_caddy[idx] = COLOR_RGB(190, 195, 200); /* Club Irons */
        }
    }
    for (int f = 0; f < 4; ++f) memcpy(s_sprite_caddy_walk[f], s_sprite_caddy, sizeof(s_sprite_caddy));
    
    /* 22. Spectators (5 shirt colors) */
    uint32_t spec_colors[5] = {
        COLOR_RGB(225, 65, 65),   /* Red */
        COLOR_RGB(55, 125, 225),  /* Royal Blue */
        COLOR_RGB(235, 195, 45),  /* Gold */
        COLOR_RGB(55, 165, 85),   /* Emerald */
        COLOR_RGB(240, 240, 245)  /* Crisp White */
    };
    for (int c = 0; c < 5; ++c) {
        for (int f = 0; f < 4; ++f) {
            memset(s_sprite_spec[c][f], 0, sizeof(s_sprite_spec[0][0]));
            for (int y = 0; y < SPRITE_SPEC_H; ++y) {
                for (int x = 0; x < SPRITE_SPEC_W; ++x) {
                    int idx = y * SPRITE_SPEC_W + x;
                    int cx = 9;
                    /* Hair */
                    if (y >= 2 && y <= 4 && abs(x - cx) <= 3) s_sprite_spec[c][f][idx] = COLOR_RGB(95, 65, 45);
                    /* Face */
                    if (y >= 5 && y <= 8 && abs(x - cx) <= 2) s_sprite_spec[c][f][idx] = COLOR_RGB(235, 190, 150);
                    /* Colorful Shirt */
                    if (y >= 9 && y <= 16 && abs(x - cx) <= 4) s_sprite_spec[c][f][idx] = spec_colors[c];
                    /* Arms / Cheering / Clapping */
                    if (f == 1 && y >= 6 && y <= 12 && (abs(x - (cx - 5)) <= 1 || abs(x - (cx + 5)) <= 1)) {
                        s_sprite_spec[c][f][idx] = COLOR_RGB(235, 190, 150); /* raised arms cheering */
                    }
                    /* Trousers */
                    if (y >= 17 && y <= 23 && abs(x - cx) <= 3) s_sprite_spec[c][f][idx] = COLOR_RGB(50, 60, 75);
                    /* Shoes */
                    if (y >= 24 && y <= 26 && (x == cx - 2 || x == cx + 2)) s_sprite_spec[c][f][idx] = COLOR_RGB(40, 40, 40);
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

/* Render Stately 4x3 Country Club Clubhouse */
static void render_clubhouse(RenderContext *rc, int32_t sx, int32_t sy) {
    /* Large isometric Country Club mansion with slate gables, brick chimneys, and veranda */
    int b_w = 110;
    int b_h = 75;
    int base_x = sx - 24;
    int base_y = sy - 45;
    
    /* Soft ground drop shadow */
    for (int dy = 35; dy < 60; ++dy) {
        for (int dx = 20; dx < 105; ++dx) {
            int px = base_x + dx;
            int py = base_y + dy;
            if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
                rc->framebuffer[py * SCREEN_W + px] = alpha_blend_pixel(rc->framebuffer[py * SCREEN_W + px], COLOR_RGB(15, 25, 20), 120);
            }
        }
    }
    
    /* Main Building Blocks (Colonial clapboard & brick) */
    for (int y = 0; y < b_h; ++y) {
        for (int x = 0; x < b_w; ++x) {
            int px = base_x + x;
            int py = base_y + y;
            if (px < 0 || px >= SCREEN_W || py < 0 || py >= SCREEN_H) continue;
            
            uint32_t col = 0;
            
            /* Slate Roof Gables (Pitched gables) */
            if (y < 28) {
                int peak_x = 55;
                if (abs(x - peak_x) <= (y + 1) * 2) {
                    int shingle = (y % 3 == 0) ? -20 : 10;
                    col = COLOR_RGB(55 + shingle, 65 + shingle, 80 + shingle);
                }
            }
            /* Brick Chimney on left */
            if (x >= 20 && x <= 28 && y >= 6 && y <= 35) {
                int brick = ((x ^ y) & 2) ? 15 : -15;
                col = COLOR_RGB(165 + brick, 62 + brick, 45);
            }
            /* White Weatherboard Walls */
            if (y >= 28 && y <= 56 && x >= 15 && x <= 95) {
                int board = (y % 4 == 0) ? -18 : 8;
                col = COLOR_RGB(232 + board, 236 + board, 240 + board);
                
                /* Dormer & Sash Windows with warm interior light */
                if (y >= 34 && y <= 46 && ((x >= 28 && x <= 38) || (x >= 72 && x <= 82))) {
                    col = COLOR_RGB(245, 215, 120); /* warm glow */
                    if (x == 33 || x == 77 || y == 40) col = COLOR_RGB(45, 50, 55); /* mullion */
                }
                /* Grand Front Door */
                if (y >= 42 && y <= 56 && x >= 50 && x <= 60) {
                    col = COLOR_RGB(95, 45, 25);
                }
            }
            /* Veranda Porch & Steps */
            if (y > 56 && y <= 66 && x >= 10 && x <= 100) {
                col = COLOR_RGB(185, 175, 160);
                if (y == 58 || y == 62 || y == 66) col = COLOR_RGB(120, 110, 100);
            }
            
            if (col != 0) {
                rc->framebuffer[py * SCREEN_W + px] = col;
            }
        }
    }
    
    /* Club Pennant Flag on Roof Peak */
    int flag_x = base_x + 55;
    int flag_y = base_y + 1;
    draw_rect(rc, flag_x, flag_y - 12, 1, 14, COLOR_RGB(220, 225, 230));
    draw_rect(rc, flag_x + 1, flag_y - 12, 10, 6, COLOR_RGB(215, 45, 45));
}

/* Modern Minimap Radar */
static void render_minimap(RenderContext *rc, const GolfState *gs) {
    int mm_x = SCREEN_W - 170;
    int mm_y = 52;
    int mm_w = 156;
    int mm_h = 130;
    
    /* Dark glass backing */
    draw_rect_alpha(rc, mm_x, mm_y, mm_w, mm_h, COLOR_RGB(18, 26, 34), 215);
    draw_rect_border(rc, mm_x, mm_y, mm_w, mm_h, COLOR_RGB(70, 95, 125));
    draw_text(rc, mm_x + 8, mm_y + 6, "COURSE RADAR", COLOR_RGB(215, 185, 95), true);
    
    /* Map scaling: 32 columns x 64 rows into 140x95 px */
    int map_x = mm_x + 8;
    int map_y = mm_y + 24;
    int mw = 140;
    int mh = 98;
    
    for (int gy = 0; gy < GRID_H; ++gy) {
        int py = map_y + (gy * mh) / GRID_H;
        for (int gx = 0; gx < GRID_W; ++gx) {
            int px = map_x + (gx * mw) / GRID_W;
            uint8_t surf = gs->grid[gx][gy].surface;
            uint32_t col = COLOR_RGB(45, 85, 35); /* Rough */
            if (surf == SURF_WATER) col = COLOR_RGB(45, 115, 195);
            else if (surf == SURF_FAIRWAY) col = COLOR_RGB(85, 175, 45);
            else if (surf == SURF_GREEN) col = COLOR_RGB(125, 225, 65);
            else if (surf == SURF_SAND) col = COLOR_RGB(215, 195, 125);
            else if (surf == SURF_TEE) col = COLOR_RGB(220, 220, 220);
            else if (surf == SURF_BRIDGE) col = COLOR_RGB(145, 95, 55);
            
            rc->framebuffer[py * SCREEN_W + px] = col;
        }
    }
    
    /* Pin blip (Red pulsing) */
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

/* Main Draw Entrypoint */
void render_frame(RenderContext *rc, const GolfState *gs) {
    rc->tick_count++;
    
    if (gs->mode == MODE_SPLASH) {
        render_splash_screen(rc, rc->tick_count);
        return;
    }
    
    /* 1. Clear Framebuffer to Sky Backdrop */
    uint32_t sky_col = COLOR_RGB(34, 46, 58);
    for (int i = 0; i < SCREEN_W * SCREEN_H; ++i) {
        rc->framebuffer[i] = sky_col;
    }
    
    /* 2. Painter's Algorithm: Back to Front (row 0 to 63) */
    for (int y = 0; y < GRID_H; ++y) {
        for (int x = 0; x < GRID_W; ++x) {
            const GolfTile *tile = &gs->grid[x][y];
            int32_t sx, sy;
            grid_to_screen(x, y, tile->elevation, rc->camera_x, rc->camera_y, &sx, &sy);
            
            /* View frustum clipping */
            if (sx < -140 || sx > SCREEN_W + 140 || sy < -140 || sy > SCREEN_H + 140) {
                continue;
            }
            
            /* Cliff Skirt Walls */
            /* SW Edge: check neighbor (x-1, y) */
            int32_t sw_elev = (x > 0) ? gs->grid[x - 1][y].elevation : 0;
            if (tile->elevation > sw_elev) {
                int drop_h = (tile->elevation - sw_elev) * HEIGHT_STEP_PX;
                for (int dy = 0; dy < drop_h; ++dy) {
                    for (int dx = 0; dx < TILE_HALF_W; ++dx) {
                        int r_py = 16 + (dx / 2) + dy;
                        int px = sx + dx;
                        int py = sy + r_py;
                        if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
                            rc->framebuffer[py * SCREEN_W + px] = s_cliff_sw[(dy % 72) * 64 + dx];
                        }
                    }
                }
            }
            
            /* SE Edge: check neighbor (x, y+1) */
            int32_t se_elev = (y < GRID_H - 1) ? gs->grid[x][y + 1].elevation : 0;
            if (tile->elevation > se_elev) {
                int drop_h = (tile->elevation - se_elev) * HEIGHT_STEP_PX;
                for (int dy = 0; dy < drop_h; ++dy) {
                    for (int dx = 0; dx < TILE_HALF_W; ++dx) {
                        int r_py = 31 - (dx / 2) + dy;
                        int px = sx + TILE_HALF_W + dx;
                        int py = sy + r_py;
                        if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
                            rc->framebuffer[py * SCREEN_W + px] = s_cliff_se[(dy % 72) * 64 + dx];
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
                        rc->framebuffer[f_py * SCREEN_W + f_px] = col;
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
                                rc->framebuffer[f_py * SCREEN_W + f_px] = alpha_blend_pixel(rc->framebuffer[f_py * SCREEN_W + f_px], COLOR_RGB(255, 235, 90), 85);
                            }
                        }
                    }
                }
            }
            
            /* Scenery Features */
            if (tile->feature == FEAT_CLUBHOUSE) {
                if (x == gs->clubhouse_gx && y == gs->clubhouse_gy) {
                    render_clubhouse(rc, sx, sy);
                }
            } else if (tile->feature == FEAT_PINE) {
                int sp_x = sx + TILE_HALF_W - SPRITE_PINE_W / 2;
                int sp_y = sy + TILE_HALF_H - SPRITE_PINE_H + 4;
                for (int py = 0; py < SPRITE_PINE_H; ++py) {
                    int f_py = sp_y + py;
                    if (f_py < 0 || f_py >= SCREEN_H) continue;
                    for (int px = 0; px < SPRITE_PINE_W; ++px) {
                        int f_px = sp_x + px;
                        if (f_px < 0 || f_px >= SCREEN_W) continue;
                        uint32_t col = s_sprite_pine[py * SPRITE_PINE_W + px];
                        if (col != 0) rc->framebuffer[f_py * SCREEN_W + f_px] = col;
                    }
                }
            } else if (tile->feature == FEAT_OAK) {
                int sp_x = sx + TILE_HALF_W - SPRITE_OAK_W / 2;
                int sp_y = sy + TILE_HALF_H - SPRITE_OAK_H + 6;
                for (int py = 0; py < SPRITE_OAK_H; ++py) {
                    int f_py = sp_y + py;
                    if (f_py < 0 || f_py >= SCREEN_H) continue;
                    for (int px = 0; px < SPRITE_OAK_W; ++px) {
                        int f_px = sp_x + px;
                        if (f_px < 0 || f_px >= SCREEN_W) continue;
                        uint32_t col = s_sprite_oak[py * SPRITE_OAK_W + px];
                        if (col != 0) rc->framebuffer[f_py * SCREEN_W + f_px] = col;
                    }
                }
            } else if (tile->feature == FEAT_PALM) {
                int sp_x = sx + TILE_HALF_W - SPRITE_PALM_W / 2;
                int sp_y = sy + TILE_HALF_H - SPRITE_PALM_H + 4;
                for (int py = 0; py < SPRITE_PALM_H; ++py) {
                    int f_py = sp_y + py;
                    if (f_py < 0 || f_py >= SCREEN_H) continue;
                    for (int px = 0; px < SPRITE_PALM_W; ++px) {
                        int f_px = sp_x + px;
                        if (f_px < 0 || f_px >= SCREEN_W) continue;
                        uint32_t col = s_sprite_palm[py * SPRITE_PALM_W + px];
                        if (col != 0) rc->framebuffer[f_py * SCREEN_W + f_px] = col;
                    }
                }
            } else if (tile->feature == FEAT_FLOWERING) {
                int sp_x = sx + TILE_HALF_W - SPRITE_FLOWERING_W / 2;
                int sp_y = sy + TILE_HALF_H - SPRITE_FLOWERING_H + 4;
                for (int py = 0; py < SPRITE_FLOWERING_H; ++py) {
                    int f_py = sp_y + py;
                    if (f_py < 0 || f_py >= SCREEN_H) continue;
                    for (int px = 0; px < SPRITE_FLOWERING_W; ++px) {
                        int f_px = sp_x + px;
                        if (f_px < 0 || f_px >= SCREEN_W) continue;
                        uint32_t col = s_sprite_flowering[py * SPRITE_FLOWERING_W + px];
                        if (col != 0) rc->framebuffer[f_py * SCREEN_W + f_px] = col;
                    }
                }
            } else if (tile->feature == FEAT_BUSH) {
                int sp_x = sx + TILE_HALF_W - SPRITE_BUSH_W / 2;
                int sp_y = sy + TILE_HALF_H - SPRITE_BUSH_H + 2;
                for (int py = 0; py < SPRITE_BUSH_H; ++py) {
                    int f_py = sp_y + py;
                    if (f_py < 0 || f_py >= SCREEN_H) continue;
                    for (int px = 0; px < SPRITE_BUSH_W; ++px) {
                        int f_px = sp_x + px;
                        if (f_px < 0 || f_px >= SCREEN_W) continue;
                        uint32_t col = s_sprite_bush[py * SPRITE_BUSH_W + px];
                        if (col != 0) rc->framebuffer[f_py * SCREEN_W + f_px] = col;
                    }
                }
            } else if (tile->feature == FEAT_FLOWERS) {
                int sp_x = sx + TILE_HALF_W - SPRITE_FLOWERS_W / 2;
                int sp_y = sy + TILE_HALF_H - SPRITE_FLOWERS_H;
                for (int py = 0; py < SPRITE_FLOWERS_H; ++py) {
                    int f_py = sp_y + py;
                    if (f_py < 0 || f_py >= SCREEN_H) continue;
                    for (int px = 0; px < SPRITE_FLOWERS_W; ++px) {
                        int f_px = sp_x + px;
                        if (f_px < 0 || f_px >= SCREEN_W) continue;
                        uint32_t col = s_sprite_flowers[py * SPRITE_FLOWERS_W + px];
                        if (col != 0) rc->framebuffer[f_py * SCREEN_W + f_px] = col;
                    }
                }
            } else if (tile->feature == FEAT_ROCK) {
                int sp_x = sx + TILE_HALF_W - SPRITE_ROCK_W / 2;
                int sp_y = sy + TILE_HALF_H - SPRITE_ROCK_H + 2;
                for (int py = 0; py < SPRITE_ROCK_H; ++py) {
                    int f_py = sp_y + py;
                    if (f_py < 0 || f_py >= SCREEN_H) continue;
                    for (int px = 0; px < SPRITE_ROCK_W; ++px) {
                        int f_px = sp_x + px;
                        if (f_px < 0 || f_px >= SCREEN_W) continue;
                        uint32_t col = s_sprite_rock[py * SPRITE_ROCK_W + px];
                        if (col != 0) rc->framebuffer[f_py * SCREEN_W + f_px] = col;
                    }
                }
            } else if (tile->feature == FEAT_BENCH) {
                int sp_x = sx + TILE_HALF_W - SPRITE_BENCH_W / 2;
                int sp_y = sy + TILE_HALF_H - SPRITE_BENCH_H;
                for (int py = 0; py < SPRITE_BENCH_H; ++py) {
                    int f_py = sp_y + py;
                    if (f_py < 0 || f_py >= SCREEN_H) continue;
                    for (int px = 0; px < SPRITE_BENCH_W; ++px) {
                        int f_px = sp_x + px;
                        if (f_px < 0 || f_px >= SCREEN_W) continue;
                        uint32_t col = s_sprite_bench[py * SPRITE_BENCH_W + px];
                        if (col != 0) rc->framebuffer[f_py * SCREEN_W + f_px] = col;
                    }
                }
            } else if (tile->feature == FEAT_CART) {
                int sp_x = sx + TILE_HALF_W - SPRITE_CART_W / 2;
                int sp_y = sy + TILE_HALF_H - SPRITE_CART_H + 2;
                for (int py = 0; py < SPRITE_CART_H; ++py) {
                    int f_py = sp_y + py;
                    if (f_py < 0 || f_py >= SCREEN_H) continue;
                    for (int px = 0; px < SPRITE_CART_W; ++px) {
                        int f_px = sp_x + px;
                        if (f_px < 0 || f_px >= SCREEN_W) continue;
                        uint32_t col = s_sprite_cart[py * SPRITE_CART_W + px];
                        if (col != 0) rc->framebuffer[f_py * SCREEN_W + f_px] = col;
                    }
                }
            }
            
            /* Pin Flag */
            if (gs->has_pin && x == gs->pin_gx && y == gs->pin_gy) {
                int sp_x = sx + TILE_HALF_W - 4;
                int sp_y = sy + TILE_HALF_H - SPRITE_FLAG_H + 2;
                int anim_f = (rc->tick_count / 10) % 4;
                for (int py = 0; py < SPRITE_FLAG_H; ++py) {
                    int f_py = sp_y + py;
                    if (f_py < 0 || f_py >= SCREEN_H) continue;
                    for (int px = 0; px < SPRITE_FLAG_W; ++px) {
                        int f_px = sp_x + px;
                        if (f_px < 0 || f_px >= SCREEN_W) continue;
                        uint32_t col = s_sprite_flag[anim_f][py * SPRITE_FLAG_W + px];
                        if (col != 0) rc->framebuffer[f_py * SCREEN_W + f_px] = col;
                    }
                }
            }
            
            /* Spectators standing at this tile */
            for (int i = 0; i < gs->spectator_count; ++i) {
                const SpectatorEntity *sp = &gs->spectators[i];
                if (sp->active && sp->gx == x && sp->gy == y) {
                    int sp_x = sx + TILE_HALF_W - SPRITE_SPEC_W / 2;
                    int sp_y = sy + TILE_HALF_H - SPRITE_SPEC_H + 2;
                    int anim_f = (sp->state == SPEC_STATE_CHEERING) ? sp->anim_frame : 0;
                    for (int py = 0; py < SPRITE_SPEC_H; ++py) {
                        int f_py = sp_y + py;
                        if (f_py < 0 || f_py >= SCREEN_H) continue;
                        for (int px = 0; px < SPRITE_SPEC_W; ++px) {
                            int f_px = sp_x + px;
                            if (f_px < 0 || f_px >= SCREEN_W) continue;
                            uint32_t col = s_sprite_spec[sp->shirt_color_idx % 5][anim_f % 4][py * SPRITE_SPEC_W + px];
                            if (col != 0) rc->framebuffer[f_py * SCREEN_W + f_px] = col;
                        }
                    }
                }
            }
            
            /* Caddy */
            if (gs->mode == MODE_PLAY && FROM_FIXED(gs->caddy.x) == x && FROM_FIXED(gs->caddy.y) == y) {
                int c_sx = FROM_FIXED(FMUL(gs->caddy.x - gs->caddy.y, TO_FIXED(TILE_HALF_W))) + rc->camera_x + TILE_HALF_W - SPRITE_CADDY_W / 2;
                int c_sy = FROM_FIXED(FMUL(gs->caddy.x + gs->caddy.y, TO_FIXED(TILE_HALF_H))) - FROM_FIXED(gs->caddy.current_z) + rc->camera_y + TILE_HALF_H - SPRITE_CADDY_H;
                const uint32_t *c_spr = (gs->caddy.state == CADDY_STATE_FOLLOWING) ? s_sprite_caddy_walk[gs->caddy.walk_anim_frame % 4] : s_sprite_caddy;
                for (int py = 0; py < SPRITE_CADDY_H; ++py) {
                    int f_py = c_sy + py;
                    if (f_py < 0 || f_py >= SCREEN_H) continue;
                    for (int px = 0; px < SPRITE_CADDY_W; ++px) {
                        int f_px = c_sx + px;
                        if (f_px < 0 || f_px >= SCREEN_W) continue;
                        uint32_t col = c_spr[py * SPRITE_CADDY_W + px];
                        if (col != 0) rc->framebuffer[f_py * SCREEN_W + f_px] = col;
                    }
                }
            }
            
            /* Golfer Character */
            if (gs->mode == MODE_PLAY && FROM_FIXED(gs->golfer.x) == x && FROM_FIXED(gs->golfer.y) == y) {
                int g_sx = FROM_FIXED(FMUL(gs->golfer.x - gs->golfer.y, TO_FIXED(TILE_HALF_W))) + rc->camera_x + TILE_HALF_W - SPRITE_GOLFER_W / 2;
                int g_sy = FROM_FIXED(FMUL(gs->golfer.x + gs->golfer.y, TO_FIXED(TILE_HALF_H))) - FROM_FIXED(gs->golfer.current_z) + rc->camera_y + TILE_HALF_H - SPRITE_GOLFER_H;
                
                const uint32_t *g_spr = (gs->golfer.state == GOLFER_STATE_WALKING) ? s_sprite_golfer_walk[gs->golfer.walk_anim_frame % 4] : s_sprite_golfer;
                for (int py = 0; py < SPRITE_GOLFER_H; ++py) {
                    int f_py = g_sy + py;
                    if (f_py < 0 || f_py >= SCREEN_H) continue;
                    for (int px = 0; px < SPRITE_GOLFER_W; ++px) {
                        int f_px = g_sx + px;
                        if (f_px < 0 || f_px >= SCREEN_W) continue;
                        uint32_t col = g_spr[py * SPRITE_GOLFER_W + px];
                        if (col != 0) rc->framebuffer[f_py * SCREEN_W + f_px] = col;
                    }
                }
            }
            
            /* Ball Shadow & Ball */
            if (FROM_FIXED(gs->ball.x) == x && FROM_FIXED(gs->ball.y) == y) {
                int bsx = FROM_FIXED(FMUL(gs->ball.x - gs->ball.y, TO_FIXED(TILE_HALF_W))) + rc->camera_x + TILE_HALF_W;
                int bsy = FROM_FIXED(FMUL(gs->ball.x + gs->ball.y, TO_FIXED(TILE_HALF_H))) - FROM_FIXED(gs->ball.ground_z) + rc->camera_y + TILE_HALF_H;
                
                /* Ground Drop Shadow */
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -2; dx <= 2; ++dx) {
                        int px = bsx + dx;
                        int py = bsy + dy;
                        if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
                            rc->framebuffer[py * SCREEN_W + px] = alpha_blend_pixel(rc->framebuffer[py * SCREEN_W + px], COLOR_RGB(10, 15, 20), 120);
                        }
                    }
                }
                
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

/* Splash Screen Renderer scaled to 1024x768 */
void render_splash_screen(RenderContext *rc, uint32_t tick_count) {
    /* Background Gradient */
    for (int y = 0; y < SCREEN_H; ++y) {
        float t = (float)y / (float)SCREEN_H;
        int r = (int)(25.0f * (1.0f - t) + 15.0f * t);
        int g = (int)(55.0f * (1.0f - t) + 35.0f * t);
        int b = (int)(95.0f * (1.0f - t) + 50.0f * t);
        uint32_t col = COLOR_RGB(r, g, b);
        for (int x = 0; x < SCREEN_W; ++x) {
            rc->framebuffer[y * SCREEN_W + x] = col;
        }
    }
    
    /* Bilinear / Nearest Scale of Retro Splash Art into Center */
    static uint32_t s_art_buf[640 * 480];
    static bool s_art_init = false;
    if (!s_art_init) {
        generate_splash_background(s_art_buf);
        s_art_init = true;
    }
    
    int dst_x0 = 80;
    int dst_y0 = 60;
    int dst_w = 864;
    int dst_h = 600;
    
    for (int dy = 0; dy < dst_h; ++dy) {
        int py = dst_y0 + dy;
        if (py < 0 || py >= SCREEN_H) continue;
        int src_y = (dy * 480) / dst_h;
        for (int dx = 0; dx < dst_w; ++dx) {
            int px = dst_x0 + dx;
            if (px < 0 || px >= SCREEN_W) continue;
            int src_x = (dx * 640) / dst_w;
            rc->framebuffer[py * SCREEN_W + px] = s_art_buf[src_y * 640 + src_x];
        }
    }
    
    /* Frame border */
    draw_rect_border(rc, dst_x0, dst_y0, dst_w, dst_h, COLOR_RGB(235, 195, 65));
    
    /* Pulsing Prompt */
    if ((tick_count / 30) % 2 == 0) {
        const char *prompt = "--- CLICK ANYWHERE OR PRESS SPACE TO ENTER DUFFER PRO ---";
        int tx = SCREEN_W / 2 - (int)strlen(prompt) * 8 / 2;
        draw_rect_alpha(rc, tx - 16, SCREEN_H - 72, (int)strlen(prompt) * 8 + 32, 28, COLOR_RGB(15, 20, 25), 220);
        draw_rect_border(rc, tx - 16, SCREEN_H - 72, (int)strlen(prompt) * 8 + 32, 28, COLOR_RGB(255, 215, 65));
        draw_text(rc, tx, SCREEN_H - 63, prompt, COLOR_RGB(255, 245, 120), true);
    }
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
