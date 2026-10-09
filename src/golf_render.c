#include "golf_render.h"
#include "font8x8.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Pixel format: 0xAARRGGBB */
#define COLOR_ARGB(a, r, g, b) (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))
#define COLOR_RGB(r, g, b)     COLOR_ARGB(255, r, g, b)

/* Procedural Sprite Buffers */
static uint32_t s_tile_rough[4][TILE_W * TILE_H];
static uint32_t s_tile_fairway[16][TILE_W * TILE_H];
static uint32_t s_tile_green[16][TILE_W * TILE_H];
static uint32_t s_tile_sand[16][TILE_W * TILE_H];
static uint32_t s_tile_water[4][TILE_W * TILE_H];
static uint32_t s_tile_tee[TILE_W * TILE_H];

/* Cliff Strata Textures */
static uint32_t s_cliff_sw[64 * 48]; /* Max 4 levels * 12px = 48px */
static uint32_t s_cliff_se[64 * 48];

/* Dynamic Entity Sprites */
#define SPRITE_PINE_W 32
#define SPRITE_PINE_H 48
static uint32_t s_sprite_pine[SPRITE_PINE_W * SPRITE_PINE_H];

#define SPRITE_OAK_W  38
#define SPRITE_OAK_H  46
static uint32_t s_sprite_oak[SPRITE_OAK_W * SPRITE_OAK_H];

#define SPRITE_ROCK_W 24
#define SPRITE_ROCK_H 18
static uint32_t s_sprite_rock[SPRITE_ROCK_W * SPRITE_ROCK_H];

#define SPRITE_FLAG_W 16
#define SPRITE_FLAG_H 40
static uint32_t s_sprite_flag[SPRITE_FLAG_W * SPRITE_FLAG_H];

#define SPRITE_GOLFER_W 22
#define SPRITE_GOLFER_H 32
static uint32_t s_sprite_golfer[SPRITE_GOLFER_W * SPRITE_GOLFER_H];

/* Fast PRNG for procedural texture generation */
static uint32_t s_tex_rand = 987654321;
static uint32_t trand(void) {
    s_tex_rand = s_tex_rand * 1664525 + 1013904223;
    return (s_tex_rand >> 16) & 0x7FFF;
}

/* Helper to check if (px, py) is inside 64x32 diamond */
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

/* Generate procedural period-correct textures */
static void generate_procedural_graphics(void) {
    s_tex_rand = 54321;
    
    /* 1. Generate Rough Grass Variations (4 variants) */
    for (int v = 0; v < 4; ++v) {
        for (int py = 0; py < TILE_H; ++py) {
            for (int px = 0; px < TILE_W; ++px) {
                int idx = py * TILE_W + px;
                if (!is_in_diamond(px, py, NULL)) {
                    s_tile_rough[v][idx] = 0;
                    continue;
                }
                
                /* Organic grass texture: rich meadow greens with subtle stippling */
                int noise = (trand() % 16) - 8;
                int dither = ((px ^ py) & 1) ? 4 : -4;
                int r = 50 + noise + dither;
                int g = 96 + noise * 2 + dither * 2;
                int b = 32 + noise / 2;
                
                /* Subtle shade along top/bottom */
                if (py < 4) { r -= 6; g -= 8; b -= 4; }
                if (py > 27) { r -= 8; g -= 12; b -= 6; }
                
                if (r < 30) r = 30; if (r > 80) r = 80;
                if (g < 65) g = 65; if (g > 130) g = 130;
                if (b < 15) b = 15; if (b > 50) b = 50;
                
                s_tile_rough[v][idx] = COLOR_RGB(r, g, b);
            }
        }
    }
    
    /* 2. Generate Fairway (16 Autotile Masks) */
    for (int m = 0; m < 16; ++m) {
        for (int py = 0; py < TILE_H; ++py) {
            for (int px = 0; px < TILE_W; ++px) {
                int idx = py * TILE_W + px;
                int dist_edge = 0;
                if (!is_in_diamond(px, py, &dist_edge)) {
                    s_tile_fairway[m][idx] = 0;
                    continue;
                }
                
                /* Classic 2:1 mowed stripe pattern */
                int stripe = ((px + py * 2) / 8) & 1;
                int r = stripe ? 92 : 80;
                int g = stripe ? 168 : 152;
                int b = stripe ? 44 : 36;
                
                /* Fine velvety grain */
                int grain = (trand() % 6) - 3;
                r += grain; g += grain * 2; b += grain;
                
                /* Autotile transition: if bordering rough on this side, scallop with rough */
                bool nw_open = !(m & MASK_NW);
                bool ne_open = !(m & MASK_NE);
                bool se_open = !(m & MASK_SE);
                bool sw_open = !(m & MASK_SW);
                
                bool is_edge = false;
                if (nw_open && px < 32 && py < 16 && (31 - px) >= (py * 2 - 4)) is_edge = true;
                if (ne_open && px >= 32 && py < 16 && (px - 32) >= (py * 2 - 4)) is_edge = true;
                if (sw_open && px < 32 && py >= 16 && (31 - px) >= ((31 - py) * 2 - 4)) is_edge = true;
                if (se_open && px >= 32 && py >= 16 && (px - 32) >= ((31 - py) * 2 - 4)) is_edge = true;
                
                if (is_edge && dist_edge < 4) {
                    /* Blend into native rough tone */
                    int blend = (dist_edge * 64);
                    uint32_t rough_col = s_tile_rough[0][idx];
                    uint32_t fw_col = COLOR_RGB(r, g, b);
                    s_tile_fairway[m][idx] = blend_color(rough_col, fw_col, blend);
                } else {
                    s_tile_fairway[m][idx] = COLOR_RGB(r, g, b);
                }
            }
        }
    }
    
    /* 3. Generate Green (16 Autotile Masks) */
    for (int m = 0; m < 16; ++m) {
        for (int py = 0; py < TILE_H; ++py) {
            for (int px = 0; px < TILE_W; ++px) {
                int idx = py * TILE_W + px;
                int dist_edge = 0;
                if (!is_in_diamond(px, py, &dist_edge)) {
                    s_tile_green[m][idx] = 0;
                    continue;
                }
                
                /* Smooth putting turf */
                int swirl = (((px - 32) * (px - 32) + (py - 16) * (py - 16) * 4) / 40) & 1;
                int r = swirl ? 116 : 108;
                int g = swirl ? 198 : 188;
                int b = swirl ? 56 : 50;
                
                /* Autotile edge feathering */
                bool nw_open = !(m & MASK_NW);
                bool ne_open = !(m & MASK_NE);
                bool se_open = !(m & MASK_SE);
                bool sw_open = !(m & MASK_SW);
                
                bool is_edge = false;
                if (nw_open && px < 32 && py < 16 && (31 - px) >= (py * 2 - 3)) is_edge = true;
                if (ne_open && px >= 32 && py < 16 && (px - 32) >= (py * 2 - 3)) is_edge = true;
                if (sw_open && px < 32 && py >= 16 && (31 - px) >= ((31 - py) * 2 - 3)) is_edge = true;
                if (se_open && px >= 32 && py >= 16 && (px - 32) >= ((31 - py) * 2 - 3)) is_edge = true;
                
                if (is_edge && dist_edge < 4) {
                    uint32_t fw_col = s_tile_fairway[15][idx];
                    uint32_t gr_col = COLOR_RGB(r, g, b);
                    s_tile_green[m][idx] = blend_color(fw_col, gr_col, dist_edge * 64);
                } else {
                    s_tile_green[m][idx] = COLOR_RGB(r, g, b);
                }
            }
        }
    }
    
    /* 4. Generate Sand Bunkers (16 Autotile Masks) */
    for (int m = 0; m < 16; ++m) {
        for (int py = 0; py < TILE_H; ++py) {
            for (int px = 0; px < TILE_W; ++px) {
                int idx = py * TILE_W + px;
                int dist_edge = 0;
                if (!is_in_diamond(px, py, &dist_edge)) {
                    s_tile_sand[m][idx] = 0;
                    continue;
                }
                
                /* Golden sand grain */
                int grain = (trand() % 14) - 7;
                int r = 214 + grain;
                int g = 192 + grain;
                int b = 132 + grain / 2;
                
                /* Subtle concave lip shadow on rim */
                if (dist_edge < 3) {
                    r -= 35; g -= 35; b -= 25;
                }
                
                if (r > 240) r = 240; if (r < 140) r = 140;
                if (g > 220) g = 220; if (g < 125) g = 125;
                if (b > 165) b = 165; if (b < 80) b = 80;
                
                s_tile_sand[m][idx] = COLOR_RGB(r, g, b);
            }
        }
    }
    
    /* 5. Generate Water Waves (4 animation frames) */
    for (int f = 0; f < 4; ++f) {
        for (int py = 0; py < TILE_H; ++py) {
            for (int px = 0; px < TILE_W; ++px) {
                int idx = py * TILE_W + px;
                if (!is_in_diamond(px, py, NULL)) {
                    s_tile_water[f][idx] = 0;
                    continue;
                }
                
                int wave = ((px + py * 2 + f * 4) / 6) & 1;
                int r = wave ? 38 : 30;
                int g = wave ? 95 : 78;
                int b = wave ? 142 : 122;
                
                /* Specular sparkle */
                if (((px * 13 + py * 7 + f * 5) % 37) == 0) {
                    r += 40; g += 50; b += 60;
                }
                
                s_tile_water[f][idx] = COLOR_RGB(r, g, b);
            }
        }
    }
    
    /* 6. Generate Tee Box (Fairway with dual markers) */
    memcpy(s_tile_tee, s_tile_fairway[15], sizeof(s_tile_tee));
    /* Red marker at (24, 14), White marker at (40, 18) */
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
            if (dx * dx + dy * dy <= 4) {
                s_tile_tee[(14 + dy) * TILE_W + (24 + dx)] = COLOR_RGB(210, 40, 40); /* Red */
                s_tile_tee[(18 + dy) * TILE_W + (40 + dx)] = COLOR_RGB(240, 240, 240); /* White */
            }
        }
    }
    
    /* 7. Generate Cliff Textures (Stratified Rock with Highlights) */
    for (int y = 0; y < 48; ++y) {
        for (int x = 0; x < 64; ++x) {
            int idx = y * 64 + x;
            int strata = ((y / 4) & 1) ? 10 : -8;
            int noise = (trand() % 12) - 6;
            
            /* SW cliff face (lit) */
            int r_sw = 105 + strata + noise;
            int g_sw = 92 + strata + noise;
            int b_sw = 75 + strata + noise;
            if (y < 2) { r_sw = 65; g_sw = 115; b_sw = 40; } /* Moss lip */
            s_cliff_sw[idx] = COLOR_RGB(r_sw, g_sw, b_sw);
            
            /* SE cliff face (in shadow) */
            int r_se = 72 + strata + noise;
            int g_se = 62 + strata + noise;
            int b_se = 50 + strata + noise;
            if (y < 2) { r_se = 50; g_se = 90; b_se = 30; }
            s_cliff_se[idx] = COLOR_RGB(r_se, g_se, b_se);
        }
    }
    
    /* 8. Generate Pine Tree Sprite (32x48) - 4 Tiered Conifer */
    memset(s_sprite_pine, 0, sizeof(s_sprite_pine));
    /* Drop shadow at ground */
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -6; dx <= 6; ++dx) {
            if (dx * dx * 4 + dy * dy * 36 <= 144) {
                s_sprite_pine[(45 + dy) * SPRITE_PINE_W + (16 + dx)] = COLOR_ARGB(120, 20, 30, 15);
            }
        }
    }
    /* Textured trunk with root flare */
    for (int y = 30; y < 45; ++y) {
        int tw = (y > 41) ? 2 : 1;
        for (int dx = -tw; dx <= tw; ++dx) {
            int px = 16 + dx;
            int lit = (dx < 0) ? 15 : -10;
            s_sprite_pine[y * SPRITE_PINE_W + px] = COLOR_RGB(70 + lit, 45 + lit, 26 + lit);
        }
    }
    /* 4-tier needle-fringed evergreen tiers */
    static const struct { int top, bot, max_w; } pine_tiers[4] = {
        {  4, 15,  6 },
        { 11, 23, 10 },
        { 18, 31, 14 },
        { 25, 38, 17 }
    };
    for (int t = 0; t < 4; ++t) {
        int t_top = pine_tiers[t].top;
        int t_bot = pine_tiers[t].bot;
        int max_w = pine_tiers[t].max_w;
        for (int y = t_top; y < t_bot; ++y) {
            int span = (y - t_top) * max_w / (t_bot - t_top) + 1;
            /* Jagged scalloped needle fringe near tier bottom */
            int fringe = (y >= t_bot - 3 && ((y ^ t) & 1)) ? 1 : 0;
            span += fringe;
            for (int dx = -span; dx <= span; ++dx) {
                int px = 16 + dx;
                int py = y;
                if (px >= 0 && px < SPRITE_PINE_W && py < SPRITE_PINE_H) {
                    /* Isometric lighting: top-left sun, bottom-right shade */
                    int lit = ((-dx * 3) - ((y - t_top) * 2));
                    int needle_noise = (trand() % 8) - 4;
                    int r = 36 + lit + needle_noise;
                    int g = 78 + lit * 2 + needle_noise * 2;
                    int b = 38 + lit + needle_noise;
                    if (r < 18) r = 18; if (r > 65) r = 65;
                    if (g < 42) g = 42; if (g > 120) g = 120;
                    if (b < 18) b = 18; if (b > 60) b = 60;
                    s_sprite_pine[py * SPRITE_PINE_W + px] = COLOR_RGB(r, g, b);
                }
            }
        }
    }
    
    /* 9. Generate Oak Tree Sprite (38x46) - Multi-cluster Billowing Deciduous Tree */
    memset(s_sprite_oak, 0, sizeof(s_sprite_oak));
    /* Ground drop shadow */
    for (int dy = -3; dy <= 3; ++dy) {
        for (int dx = -10; dx <= 10; ++dx) {
            if (dx * dx * 3 + dy * dy * 30 <= 300) {
                s_sprite_oak[(42 + dy) * SPRITE_OAK_W + (19 + dx)] = COLOR_ARGB(120, 20, 30, 15);
            }
        }
    }
    /* Gnarled Oak Trunk with branches */
    for (int y = 24; y < 43; ++y) {
        int w = (y > 38) ? 3 : 2;
        for (int dx = -w; dx <= w; ++dx) {
            int px = 19 + dx;
            int lit = (dx < 0) ? 18 : -14;
            s_sprite_oak[y * SPRITE_OAK_W + px] = COLOR_RGB(85 + lit, 58 + lit, 38 + lit);
        }
    }
    /* Visible fork branches */
    for (int i = 0; i < 6; ++i) {
        s_sprite_oak[(25 - i) * SPRITE_OAK_W + (18 - i)] = COLOR_RGB(70, 48, 30);
        s_sprite_oak[(25 - i) * SPRITE_OAK_W + (20 + i)] = COLOR_RGB(60, 40, 24);
    }
    /* Overlapping foliage puffs (5 clusters: Top, Top-Left, Top-Right, Mid-Left, Mid-Right) */
    static const struct { int cx, cy, rx, ry; } oak_puffs[5] = {
        { 19, 12, 10,  9 },
        { 13, 18,  9,  8 },
        { 25, 17,  9,  8 },
        { 12, 24,  8,  7 },
        { 26, 23,  9,  7 }
    };
    for (int p = 0; p < 5; ++p) {
        int cx = oak_puffs[p].cx;
        int cy = oak_puffs[p].cy;
        int rx = oak_puffs[p].rx;
        int ry = oak_puffs[p].ry;
        for (int dy = -ry; dy <= ry; ++dy) {
            for (int dx = -rx; dx <= rx; ++dx) {
                if (dx * dx * ry * ry + dy * dy * rx * rx <= rx * rx * ry * ry) {
                    int px = cx + dx;
                    int py = cy + dy;
                    if (px >= 0 && px < SPRITE_OAK_W && py < SPRITE_OAK_H) {
                        /* Leafy volume shading with sunlight from top-left */
                        int lit = (-dx * 4 - dy * 4);
                        int stipple = ((px ^ py) & 1) ? 6 : -6;
                        int r = 68 + lit + stipple;
                        int g = 134 + lit * 2 + stipple * 2;
                        int b = 40 + lit + stipple;
                        if (r < 25) r = 25; if (r > 125) r = 125;
                        if (g < 55) g = 55; if (g > 195) g = 195;
                        if (b < 18) b = 18; if (b > 65) b = 65;
                        s_sprite_oak[py * SPRITE_OAK_W + px] = COLOR_RGB(r, g, b);
                    }
                }
            }
        }
    }
    
    /* 10. Generate Rock Sprite (24x18) */
    memset(s_sprite_rock, 0, sizeof(s_sprite_rock));
    /* Rock ground shadow */
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -7; dx <= 7; ++dx) {
            if (dx * dx * 4 + dy * dy * 36 <= 144) {
                s_sprite_rock[(15 + dy) * SPRITE_ROCK_W + (12 + dx)] = COLOR_ARGB(120, 20, 30, 15);
            }
        }
    }
    for (int y = 2; y < 15; ++y) {
        for (int x = 3; x < 21; ++x) {
            int dx = x - 12;
            int dy = y - 9;
            if (dx * dx * 3 + dy * dy * 5 <= 120) {
                int lit = (-dx * 4 - dy * 5);
                int grain = (trand() % 10) - 5;
                int r = 108 + lit + grain;
                int g = 104 + lit + grain;
                int b = 98 + lit + grain;
                /* Moss highlight on top left */
                if (dy < -2 && dx < 0) {
                    g += 20;
                }
                if (r < 45) r = 45; if (r > 165) r = 165;
                if (g < 42) g = 42; if (g > 175) g = 175;
                if (b < 38) b = 38; if (b > 155) b = 155;
                s_sprite_rock[y * SPRITE_ROCK_W + x] = COLOR_RGB(r, g, b);
            }
        }
    }
    
    /* 11. Generate Flag Sprite (16x40) */
    memset(s_sprite_flag, 0, sizeof(s_sprite_flag));
    /* Cup hole */
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -4; dx <= 4; ++dx) {
            if (dx * dx * 4 + dy * dy * 16 <= 64) {
                s_sprite_flag[(38 + dy) * SPRITE_FLAG_W + (8 + dx)] = COLOR_RGB(15, 18, 22);
            }
        }
    }
    /* White pole */
    for (int y = 4; y < 38; ++y) {
        s_sprite_flag[y * SPRITE_FLAG_W + 8] = COLOR_RGB(245, 245, 245);
        s_sprite_flag[y * SPRITE_FLAG_W + 7] = COLOR_RGB(160, 160, 160);
    }
    /* Red/Yellow pennant waving */
    for (int y = 5; y < 17; ++y) {
        int w = 7 - (y - 5) / 2;
        for (int x = 9; x <= 9 + w; ++x) {
            int col = ((x + y) & 2) ? COLOR_RGB(235, 45, 45) : COLOR_RGB(255, 215, 30);
            s_sprite_flag[y * SPRITE_FLAG_W + x] = col;
        }
    }
    
    /* 12. Generate Golfer Sprite (22x32) */
    memset(s_sprite_golfer, 0, sizeof(s_sprite_golfer));
    /* Ground shadow */
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -5; dx <= 5; ++dx) {
            if (dx * dx * 4 + dy * dy * 25 <= 100) {
                s_sprite_golfer[(30 + dy) * SPRITE_GOLFER_W + (11 + dx)] = COLOR_ARGB(120, 20, 30, 15);
            }
        }
    }
    /* White Visor/Cap */
    for (int y = 4; y <= 6; ++y) {
        for (int x = 9; x <= 14; ++x) {
            s_sprite_golfer[y * SPRITE_GOLFER_W + x] = (y == 4) ? COLOR_RGB(250, 250, 250) : COLOR_RGB(220, 220, 220);
        }
    }
    /* Face & Hair */
    for (int y = 7; y <= 9; ++y) {
        for (int x = 10; x <= 13; ++x) {
            s_sprite_golfer[y * SPRITE_GOLFER_W + x] = COLOR_RGB(235, 190, 145);
        }
        s_sprite_golfer[y * SPRITE_GOLFER_W + 9] = COLOR_RGB(90, 60, 35); /* Hair */
    }
    /* Red Polo Shirt with shading */
    for (int y = 10; y <= 18; ++y) {
        for (int x = 8; x <= 15; ++x) {
            int lit = (x < 11) ? 25 : -20;
            s_sprite_golfer[y * SPRITE_GOLFER_W + x] = COLOR_RGB(200 + lit, 35 + lit / 2, 35 + lit / 2);
        }
    }
    /* White golf glove on lead hand */
    s_sprite_golfer[17 * SPRITE_GOLFER_W + 7] = COLOR_RGB(240, 240, 240);
    s_sprite_golfer[18 * SPRITE_GOLFER_W + 7] = COLOR_RGB(240, 240, 240);
    /* Khaki Trousers */
    for (int y = 19; y <= 27; ++y) {
        for (int x = 8; x <= 11; ++x)
            s_sprite_golfer[y * SPRITE_GOLFER_W + x] = COLOR_RGB(190, 170, 130);
        for (int x = 12; x <= 15; ++x)
            s_sprite_golfer[y * SPRITE_GOLFER_W + x] = COLOR_RGB(165, 145, 110);
    }
    /* White/Black saddle golf shoes */
    for (int x = 7; x <= 11; ++x) s_sprite_golfer[28 * SPRITE_GOLFER_W + x] = COLOR_RGB(230, 230, 230);
    for (int x = 12; x <= 16; ++x) s_sprite_golfer[28 * SPRITE_GOLFER_W + x] = COLOR_RGB(230, 230, 230);
    s_sprite_golfer[28 * SPRITE_GOLFER_W + 9] = COLOR_RGB(40, 40, 40);
    s_sprite_golfer[28 * SPRITE_GOLFER_W + 14] = COLOR_RGB(40, 40, 40);
    /* Steel Golf Club (shaft + iron head) */
    for (int i = 0; i < 13; ++i) {
        int cx = 13 + i / 2;
        int cy = 16 + i;
        if (cx < SPRITE_GOLFER_W && cy < SPRITE_GOLFER_H)
            s_sprite_golfer[cy * SPRITE_GOLFER_W + cx] = COLOR_RGB(200, 205, 210);
    }
    /* Club head near ground */
    s_sprite_golfer[28 * SPRITE_GOLFER_W + 19] = COLOR_RGB(150, 155, 160);
    s_sprite_golfer[28 * SPRITE_GOLFER_W + 20] = COLOR_RGB(180, 185, 190);
}

void render_init(RenderContext *rc, uint32_t *buffer) {
    rc->framebuffer = buffer;
    rc->camera_x = SCREEN_W / 2 - 32;
    rc->camera_y = 60;
    rc->mouse_gx = -1;
    rc->mouse_gy = -1;
    rc->mouse_valid = false;
    rc->tick_count = 0;
    generate_procedural_graphics();
}

void render_free(RenderContext *rc) {
    (void)rc;
}

/* Projection conversions */
void grid_to_screen(int32_t gx, int32_t gy, int32_t elevation, int32_t cam_x, int32_t cam_y, int32_t *sx, int32_t *sy) {
    *sx = (gx - gy) * TILE_HALF_W + cam_x;
    *sy = (gx + gy) * TILE_HALF_H - (elevation * HEIGHT_STEP_PX) + cam_y;
}

bool screen_to_grid(const GolfState *gs, int32_t sx, int32_t sy, int32_t cam_x, int32_t cam_y, int32_t *out_gx, int32_t *out_gy) {
    /* Multi-tier inverse isometric raycast test (from highest elevation 3 down to 0) */
    for (int elev = 3; elev >= 0; --elev) {
        int32_t unshifted_y = sy - cam_y + (elev * HEIGHT_STEP_PX) - TILE_HALF_H;
        int32_t unshifted_x = sx - cam_x - TILE_HALF_W;
        
        /* 2:1 isometric inverse transformation */
        int32_t fx = (unshifted_x * 16 + unshifted_y * 32);
        int32_t fy = (unshifted_y * 32 - unshifted_x * 16);
        
        int32_t gx = fx / (TILE_W * TILE_H / 2);
        int32_t gy = fy / (TILE_W * TILE_H / 2);
        
        if (fx < 0) gx--;
        if (fy < 0) gy--;
        
        if (gx >= 0 && gx < GRID_W && gy >= 0 && gy < GRID_H) {
            if (gs->grid[gx][gy].elevation == (uint8_t)elev) {
                *out_gx = gx;
                *out_gy = gy;
                return true;
            }
        }
    }
    
    /* Fallback to lowland plane (elev 1) */
    int32_t unshifted_y = sy - cam_y + (1 * HEIGHT_STEP_PX) - TILE_HALF_H;
    int32_t unshifted_x = sx - cam_x - TILE_HALF_W;
    int32_t gx = (unshifted_x * 16 + unshifted_y * 32) / (TILE_W * TILE_H / 2);
    int32_t gy = (unshifted_y * 32 - unshifted_x * 16) / (TILE_W * TILE_H / 2);
    if (gx >= 0 && gx < GRID_W && gy >= 0 && gy < GRID_H) {
        *out_gx = gx;
        *out_gy = gy;
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

/* Fast Pixel Blitters */
static inline void set_pixel(RenderContext *rc, int x, int y, uint32_t color) {
    if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) {
        rc->framebuffer[y * SCREEN_W + x] = color;
    }
}

static inline void blend_pixel(RenderContext *rc, int x, int y, uint32_t color) {
    if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) {
        uint32_t bg = rc->framebuffer[y * SCREEN_W + x];
        int a = (color >> 24) & 0xFF;
        if (a == 255) {
            rc->framebuffer[y * SCREEN_W + x] = color;
        } else if (a > 0) {
            rc->framebuffer[y * SCREEN_W + x] = blend_color(bg, color, a);
        }
    }
}

void draw_rect(RenderContext *rc, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    int x1 = FMAX(0, x), x2 = FMIN(SCREEN_W, x + w);
    int y1 = FMAX(0, y), y2 = FMIN(SCREEN_H, y + h);
    for (int py = y1; py < y2; ++py) {
        uint32_t *row = &rc->framebuffer[py * SCREEN_W + x1];
        for (int px = x1; px < x2; ++px) {
            *row++ = color;
        }
    }
}

void draw_rect_border(RenderContext *rc, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    for (int px = x; px < x + w; ++px) {
        set_pixel(rc, px, y, color);
        set_pixel(rc, px, y + h - 1, color);
    }
    for (int py = y; py < y + h; ++py) {
        set_pixel(rc, x, py, color);
        set_pixel(rc, x + w - 1, py, color);
    }
}

void draw_text(RenderContext *rc, int32_t x, int32_t y, const char *str, uint32_t color, bool shadow) {
    if (!str) return;
    int cur_x = x;
    while (*str) {
        char c = *str++;
        if (c < 32 || c > 126) c = '?';
        const uint8_t *glyph = g_font8x8[c - 32];
        for (int gy = 0; gy < 8; ++gy) {
            uint8_t row = glyph[gy];
            for (int gx = 0; gx < 8; ++gx) {
                if (row & (1 << (7 - gx))) {
                    if (shadow) {
                        set_pixel(rc, cur_x + gx + 1, y + gy + 1, COLOR_RGB(10, 10, 10));
                    }
                    set_pixel(rc, cur_x + gx, y + gy, color);
                }
            }
        }
        cur_x += 8;
    }
}

/* Blits a 64x32 tile diamond sprite with transparency */
static void blit_tile_diamond(RenderContext *rc, int sx, int sy, const uint32_t *sprite) {
    for (int py = 0; py < TILE_H; ++py) {
        int screen_y = sy + py;
        if (screen_y < 0 || screen_y >= SCREEN_H) continue;
        
        const uint32_t *src = &sprite[py * TILE_W];
        uint32_t *dst = &rc->framebuffer[screen_y * SCREEN_W];
        
        for (int px = 0; px < TILE_W; ++px) {
            int screen_x = sx + px;
            if (screen_x >= 0 && screen_x < SCREEN_W) {
                uint32_t col = src[px];
                if (col != 0) {
                    dst[screen_x] = col;
                }
            }
        }
    }
}

/* Draws vertical cliff skirts to seal gaps */
static void blit_cliff_skirt(RenderContext *rc, int sx, int sy, int h_sw, int h_se) {
    /* SW Cliff Face (lit by ambient light) */
    if (h_sw > 0) {
        for (int step = 0; step < 32; ++step) {
            int base_x = sx + step;
            int base_y = sy + 16 + (step / 2);
            for (int dy = 0; dy < h_sw; ++dy) {
                int py = base_y + dy;
                int px = base_x;
                if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
                    int tex_idx = (dy % 48) * 64 + (step % 64);
                    rc->framebuffer[py * SCREEN_W + px] = s_cliff_sw[tex_idx];
                }
            }
        }
    }
    
    /* SE Cliff Face (in shadow) */
    if (h_se > 0) {
        for (int step = 0; step < 32; ++step) {
            int base_x = sx + 32 + step;
            int base_y = sy + 31 - (step / 2);
            for (int dy = 0; dy < h_se; ++dy) {
                int py = base_y + dy;
                int px = base_x;
                if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
                    int tex_idx = (dy % 48) * 64 + (step % 64);
                    rc->framebuffer[py * SCREEN_W + px] = s_cliff_se[tex_idx];
                }
            }
        }
    }
}

/* Blit Sprite with color key 0x00000000 */
static void blit_sprite(RenderContext *rc, int sx, int sy, const uint32_t *sprite, int w, int h) {
    for (int py = 0; py < h; ++py) {
        int screen_y = sy + py;
        if (screen_y < 0 || screen_y >= SCREEN_H) continue;
        
        const uint32_t *src = &sprite[py * w];
        uint32_t *dst = &rc->framebuffer[screen_y * SCREEN_W];
        
        for (int px = 0; px < w; ++px) {
            int screen_x = sx + px;
            if (screen_x >= 0 && screen_x < SCREEN_W) {
                uint32_t col = src[px];
                if (col != 0) {
                    dst[screen_x] = col;
                }
            }
        }
    }
}

/* Draw a glowing diamond outline around hovered tile */
static void draw_tile_highlight(RenderContext *rc, int sx, int sy, uint32_t color) {
    /* 4 diamond edges */
    for (int i = 0; i < 32; ++i) {
        set_pixel(rc, sx + 31 - i, sy + (i / 2), color);       /* NW */
        set_pixel(rc, sx + 32 + i, sy + (i / 2), color);       /* NE */
        set_pixel(rc, sx + 31 - i, sy + 31 - (i / 2), color);  /* SW */
        set_pixel(rc, sx + 32 + i, sy + 31 - (i / 2), color);  /* SE */
    }
}

/* Bresenham line for wind and aim indicators */
static void draw_line(RenderContext *rc, int x0, int y0, int x1, int y1, uint32_t color, bool dotted) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    int count = 0;
    
    while (1) {
        if (!dotted || (count & 2) == 0) {
            set_pixel(rc, x0, y0, color);
        }
        count++;
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* Render HUD and UI Controls */
static void render_hud(RenderContext *rc, const GolfState *gs) {
    char buf[128];
    
    /* 1. Top Beveled Status Bar */
    draw_rect(rc, 0, 0, SCREEN_W, 28, COLOR_RGB(24, 34, 46));
    draw_rect_border(rc, 0, 0, SCREEN_W, 28, COLOR_RGB(60, 80, 100));
    set_pixel(rc, 0, 27, COLOR_RGB(15, 20, 28));
    
    /* Hole Number & Par */
    snprintf(buf, sizeof(buf), "HOLE 1  PAR %d  %d YDS", gs->par, gs->hole_yards);
    draw_text(rc, 12, 10, buf, COLOR_RGB(245, 220, 110), true);
    
    /* Current Lie / Status */
    const char *surf_name = "ROUGH (80%)";
    if (gs->ball.current_surf == SURF_FAIRWAY) surf_name = "FAIRWAY (100%)";
    else if (gs->ball.current_surf == SURF_GREEN) surf_name = "GREEN (100%)";
    else if (gs->ball.current_surf == SURF_SAND) surf_name = "SAND BUNKER (55%)";
    else if (gs->ball.current_surf == SURF_TEE) surf_name = "TEE BOX (100%)";
    
    snprintf(buf, sizeof(buf), "LIE: %s", surf_name);
    draw_text(rc, 220, 10, buf, COLOR_RGB(200, 230, 200), true);
    
    /* Wind Indicator */
    int wind_mph = FROM_FIXED(gs->wind.speed);
    snprintf(buf, sizeof(buf), "WIND: %d MPH", wind_mph);
    draw_text(rc, 430, 10, buf, COLOR_RGB(180, 210, 255), true);
    
    /* Wind Arrow */
    int w_cx = 550, w_cy = 13;
    fixed wx = fixed_cos(gs->wind.angle);
    fixed wy = fixed_sin(gs->wind.angle);
    int w_tx = w_cx + FROM_FIXED(wx * 10);
    int w_ty = w_cy + FROM_FIXED(wy * 10);
    draw_line(rc, w_cx, w_cy, w_tx, w_ty, COLOR_RGB(255, 255, 120), false);
    
    /* Mode Badge */
    if (gs->mode == MODE_PLAY) {
        draw_rect(rc, 570, 4, 62, 20, COLOR_RGB(34, 120, 45));
        draw_rect_border(rc, 570, 4, 62, 20, COLOR_RGB(90, 210, 100));
        draw_text(rc, 578, 10, "PLAY", COLOR_RGB(255, 255, 255), false);
    } else {
        draw_rect(rc, 565, 4, 67, 20, COLOR_RGB(160, 95, 20));
        draw_rect_border(rc, 565, 4, 67, 20, COLOR_RGB(240, 180, 50));
        draw_text(rc, 572, 10, "BUILD", COLOR_RGB(255, 255, 255), false);
    }
    
    /* 2. Bottom Control Bar */
    draw_rect(rc, 0, SCREEN_H - 58, SCREEN_W, 58, COLOR_RGB(24, 34, 46));
    draw_rect_border(rc, 0, SCREEN_H - 58, SCREEN_W, 58, COLOR_RGB(60, 80, 100));
    
    if (gs->mode == MODE_BUILD) {
        /* Tool selection palette */
        static const char *tool_labels[TOOL_COUNT] = {
            "FAIRWAY", "GREEN", "SAND", "ROUGH", "TEE", "PIN", "PINE", "OAK", "ROCK", "DOZE"
        };
        int btn_x = 10;
        for (int i = 0; i < TOOL_COUNT; ++i) {
            int bw = (i == TOOL_FAIRWAY || i == TOOL_ROUGH) ? 58 : 46;
            bool active = (gs->current_tool == (BuildTool)i);
            
            uint32_t bg = active ? COLOR_RGB(70, 110, 150) : COLOR_RGB(38, 50, 68);
            uint32_t bd = active ? COLOR_RGB(255, 220, 80) : COLOR_RGB(80, 100, 130);
            
            draw_rect(rc, btn_x, SCREEN_H - 48, bw, 22, bg);
            draw_rect_border(rc, btn_x, SCREEN_H - 48, bw, 22, bd);
            draw_text(rc, btn_x + 4, SCREEN_H - 42, tool_labels[i], active ? COLOR_RGB(255, 255, 255) : COLOR_RGB(180, 190, 200), false);
            btn_x += bw + 4;
        }
        
        /* Brush toggle & Hints */
        snprintf(buf, sizeof(buf), "BRUSH: %dx%d [B] | TAB: PLAY HOLE | R-DRAG: CAMERA", gs->brush_size, gs->brush_size);
        draw_text(rc, 12, SCREEN_H - 18, buf, COLOR_RGB(220, 200, 140), true);
    } else {
        /* Play Mode HUD */
        const ClubData *club = golf_get_club_data(gs->current_club);
        
        /* Club Box */
        draw_rect(rc, 10, SCREEN_H - 50, 150, 42, COLOR_RGB(32, 45, 62));
        draw_rect_border(rc, 10, SCREEN_H - 50, 150, 42, COLOR_RGB(80, 110, 145));
        draw_text(rc, 16, SCREEN_H - 45, club->name, COLOR_RGB(255, 225, 100), true);
        snprintf(buf, sizeof(buf), "%d YARDS  [C]", club->nominal_yards);
        draw_text(rc, 16, SCREEN_H - 25, buf, COLOR_RGB(180, 210, 240), false);
        
        /* Shot & Aim Stats */
        int dist_pin = golf_distance_to_pin_yards(gs);
        snprintf(buf, sizeof(buf), "STROKE %d", gs->strokes + 1);
        draw_text(rc, 172, SCREEN_H - 45, buf, COLOR_RGB(255, 255, 255), true);
        snprintf(buf, sizeof(buf), "TO PIN: %d YDS", dist_pin);
        draw_text(rc, 172, SCREEN_H - 25, buf, COLOR_RGB(160, 230, 180), true);
        
        /* 3-Click Swing Gauge (Retro links-style meter) */
        int meter_x = 310;
        int meter_y = SCREEN_H - 45;
        int meter_w = 200;
        int meter_h = 24;
        
        draw_rect(rc, meter_x, meter_y, meter_w, meter_h, COLOR_RGB(15, 20, 26));
        draw_rect_border(rc, meter_x, meter_y, meter_w, meter_h, COLOR_RGB(100, 120, 140));
        
        /* Color gradient fill based on sweetspot and power */
        for (int px = 0; px < meter_w - 4; ++px) {
            int ratio = (px * 256) / (meter_w - 4);
            int r = (ratio * 240) / 256;
            int g = 220 - (ratio * 140) / 256;
            int b = 30;
            draw_rect(rc, meter_x + 2 + px, meter_y + 2, 1, meter_h - 4, COLOR_RGB(r, g, b));
        }
        
        /* Sweetspot notch at left (0% snap mark) */
        int snap_notch_x = meter_x + 16;
        for (int ny = meter_y + 1; ny < meter_y + meter_h - 1; ++ny) {
            set_pixel(rc, snap_notch_x, ny, COLOR_RGB(255, 255, 255));
            set_pixel(rc, snap_notch_x + 1, ny, COLOR_RGB(255, 255, 255));
        }
        
        /* Current Needle Position */
        fixed needle_ratio = gs->swing.cursor_pos;
        if (needle_ratio < 0) needle_ratio = 0;
        if (needle_ratio > FIXED_ONE) needle_ratio = FIXED_ONE;
        int needle_px = meter_x + 2 + FROM_FIXED(needle_ratio * (meter_w - 6));
        
        for (int ny = meter_y - 2; ny < meter_y + meter_h + 2; ++ny) {
            set_pixel(rc, needle_px - 1, ny, COLOR_RGB(20, 20, 20));
            set_pixel(rc, needle_px, ny, COLOR_RGB(255, 255, 255));
            set_pixel(rc, needle_px + 1, ny, COLOR_RGB(20, 20, 20));
        }
        
        /* Gauge action hint */
        const char *gauge_hint = "SPACE/CLICK: START SWING";
        if (gs->swing.state == SWING_STATE_POWER) gauge_hint = "CLICK: SET POWER";
        else if (gs->swing.state == SWING_STATE_ACCURACY) gauge_hint = "CLICK: SNAP SWEET SPOT!";
        else if (gs->swing.state == SWING_STATE_RESOLVED) gauge_hint = "BALL IN PLAY...";
        
        draw_text(rc, 310, SCREEN_H - 16, gauge_hint, COLOR_RGB(240, 220, 120), true);
        
        /* Aim rotate hints */
        draw_text(rc, 525, SCREEN_H - 45, "[<] [>] AIM", COLOR_RGB(200, 210, 230), false);
        draw_text(rc, 525, SCREEN_H - 25, "TAB: BUILD", COLOR_RGB(160, 180, 200), false);
    }
    
    /* 3. Center Notification Banner */
    if (gs->banner_timer > 0 && gs->banner_text[0] != '\0') {
        int text_len = (int)strlen(gs->banner_text);
        int bw = text_len * 8 + 32;
        int bh = 32;
        int bx = (SCREEN_W - bw) / 2;
        int by = 60;
        
        draw_rect(rc, bx, by, bw, bh, COLOR_RGB(20, 28, 40));
        draw_rect_border(rc, bx, by, bw, bh, COLOR_RGB(230, 190, 70));
        draw_text(rc, bx + 16, by + 12, gs->banner_text, COLOR_RGB(255, 235, 120), true);
    }
}

/* Main Frame Render Function */
void render_frame(RenderContext *rc, const GolfState *gs) {
    rc->tick_count++;
    
    /* 1. Clear background to atmospheric sky color (classic soft retro blue) */
    draw_rect(rc, 0, 0, SCREEN_W, SCREEN_H, COLOR_RGB(138, 172, 204));
    
    /* 2. Painter's Algorithm Back-to-Front Isometric Tile & Feature Rendering */
    for (int gy = 0; gy < GRID_H; ++gy) {
        for (int gx = 0; gx < GRID_W; ++gx) {
            const GolfTile *tile = &gs->grid[gx][gy];
            int elevation = tile->elevation;
            
            int sx, sy;
            grid_to_screen(gx, gy, elevation, rc->camera_x, rc->camera_y, &sx, &sy);
            
            /* Screen frustum culling */
            if (sx + TILE_W < 0 || sx >= SCREEN_W || sy + TILE_H + 48 < 0 || sy >= SCREEN_H) {
                continue;
            }
            
            /* Vertical Cliff Skirt Check */
            int h_sw = 0, h_se = 0;
            if (gx > 0 && gs->grid[gx - 1][gy].elevation < elevation) {
                h_sw = (elevation - gs->grid[gx - 1][gy].elevation) * HEIGHT_STEP_PX;
            }
            if (gy < GRID_H - 1 && gs->grid[gx][gy + 1].elevation < elevation) {
                h_se = (elevation - gs->grid[gx][gy + 1].elevation) * HEIGHT_STEP_PX;
            }
            if (h_sw > 0 || h_se > 0) {
                blit_cliff_skirt(rc, sx, sy, h_sw, h_se);
            }
            
            /* Blit Diamond Surface */
            const uint32_t *sprite = NULL;
            switch (tile->surface) {
                case SURF_WATER:
                    sprite = s_tile_water[(rc->tick_count / 16 + tile->variant) % 4];
                    break;
                case SURF_FAIRWAY:
                    sprite = s_tile_fairway[tile->autotile_mask & 15];
                    break;
                case SURF_GREEN:
                    sprite = s_tile_green[tile->autotile_mask & 15];
                    break;
                case SURF_SAND:
                    sprite = s_tile_sand[tile->autotile_mask & 15];
                    break;
                case SURF_TEE:
                    sprite = s_tile_tee;
                    break;
                case SURF_ROUGH:
                default:
                    sprite = s_tile_rough[tile->variant % 4];
                    break;
            }
            blit_tile_diamond(rc, sx, sy, sprite);
            
            /* Mouse hover highlight */
            if (rc->mouse_valid && rc->mouse_gx == gx && rc->mouse_gy == gy) {
                draw_tile_highlight(rc, sx, sy, (gs->mode == MODE_BUILD) ? COLOR_RGB(255, 230, 60) : COLOR_RGB(200, 240, 255));
            }
            
            /* Cup & Pin Flag */
            if (gs->has_pin && gs->pin_gx == gx && gs->pin_gy == gy) {
                blit_sprite(rc, sx + 24, sy - 12, s_sprite_flag, SPRITE_FLAG_W, SPRITE_FLAG_H);
            }
            
            /* Natural Obstacles */
            if (tile->feature == FEAT_PINE) {
                blit_sprite(rc, sx + 16, sy - 28, s_sprite_pine, SPRITE_PINE_W, SPRITE_PINE_H);
            } else if (tile->feature == FEAT_OAK) {
                blit_sprite(rc, sx + 13, sy - 26, s_sprite_oak, SPRITE_OAK_W, SPRITE_OAK_H);
            } else if (tile->feature == FEAT_ROCK) {
                blit_sprite(rc, sx + 20, sy + 4, s_sprite_rock, SPRITE_ROCK_W, SPRITE_ROCK_H);
            }
            
            /* Golfer Sprite */
            if (gs->mode == MODE_PLAY && gs->ball.state == BALL_IDLE) {
                int bgx = FROM_FIXED(gs->ball.x);
                int bgy = FROM_FIXED(gs->ball.y);
                if (bgx == gx && bgy == gy) {
                    blit_sprite(rc, sx + 6, sy - 12, s_sprite_golfer, SPRITE_GOLFER_W, SPRITE_GOLFER_H);
                }
            }
            
            /* Ball & Shadow */
            int bgx = FROM_FIXED(gs->ball.x);
            int bgy = FROM_FIXED(gs->ball.y);
            if (bgx == gx && bgy == gy) {
                /* Ball sub-tile screen coordinates */
                fixed fx = gs->ball.x;
                fixed fy = gs->ball.y;
                int32_t ball_ground_sx = FROM_FIXED(FMUL(fx - fy, TO_FIXED(TILE_HALF_W))) + rc->camera_x + TILE_HALF_W;
                int32_t ball_ground_sy = FROM_FIXED(FMUL(fx + fy, TO_FIXED(TILE_HALF_H))) - FROM_FIXED(gs->ball.ground_z) + rc->camera_y + TILE_HALF_H;
                int32_t ball_screen_y = ball_ground_sy - FROM_FIXED(gs->ball.z - gs->ball.ground_z);
                
                /* Draw Ground Drop Shadow */
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -2; dx <= 2; ++dx) {
                        blend_pixel(rc, ball_ground_sx + dx, ball_ground_sy + dy, COLOR_ARGB(140, 20, 20, 20));
                    }
                }
                
                /* Draw Ball (crisp 3x3 white sphere) */
                set_pixel(rc, ball_ground_sx - 1, ball_screen_y - 1, COLOR_RGB(255, 255, 255));
                set_pixel(rc, ball_ground_sx,     ball_screen_y - 1, COLOR_RGB(255, 255, 255));
                set_pixel(rc, ball_ground_sx + 1, ball_screen_y - 1, COLOR_RGB(220, 220, 220));
                set_pixel(rc, ball_ground_sx - 1, ball_screen_y,     COLOR_RGB(255, 255, 255));
                set_pixel(rc, ball_ground_sx,     ball_screen_y,     COLOR_RGB(255, 255, 255));
                set_pixel(rc, ball_ground_sx + 1, ball_screen_y,     COLOR_RGB(200, 200, 200));
                set_pixel(rc, ball_ground_sx - 1, ball_screen_y + 1, COLOR_RGB(200, 200, 200));
                set_pixel(rc, ball_ground_sx,     ball_screen_y + 1, COLOR_RGB(180, 180, 180));
                set_pixel(rc, ball_ground_sx + 1, ball_screen_y + 1, COLOR_RGB(150, 150, 150));
            }
        }
    }
    
    /* 3. Aim Line & Landing Reticle in Play Mode */
    if (gs->mode == MODE_PLAY && gs->ball.state == BALL_IDLE) {
        const ClubData *club = golf_get_club_data(gs->current_club);
        fixed aim_dx = fixed_cos(gs->aim_angle);
        fixed aim_dy = fixed_sin(gs->aim_angle);
        
        fixed start_fx = gs->ball.x;
        fixed start_fy = gs->ball.y;
        
        /* Distance in tiles */
        int tiles_dist = club->nominal_yards / YARDS_PER_TILE;
        fixed end_fx = start_fx + FMUL(TO_FIXED(tiles_dist), aim_dx);
        fixed end_fy = start_fy + FMUL(TO_FIXED(tiles_dist), aim_dy);
        
        fixed start_gz = golf_get_ground_z_fixed(gs, start_fx, start_fy);
        fixed end_gz = golf_get_ground_z_fixed(gs, end_fx, end_fy);
        
        int32_t sx0 = FROM_FIXED(FMUL(start_fx - start_fy, TO_FIXED(TILE_HALF_W))) + rc->camera_x + TILE_HALF_W;
        int32_t sy0 = FROM_FIXED(FMUL(start_fx + start_fy, TO_FIXED(TILE_HALF_H))) - FROM_FIXED(start_gz) + rc->camera_y + TILE_HALF_H;
        
        int32_t sx1 = FROM_FIXED(FMUL(end_fx - end_fy, TO_FIXED(TILE_HALF_W))) + rc->camera_x + TILE_HALF_W;
        int32_t sy1 = FROM_FIXED(FMUL(end_fx + end_fy, TO_FIXED(TILE_HALF_H))) - FROM_FIXED(end_gz) + rc->camera_y + TILE_HALF_H;
        
        draw_line(rc, sx0, sy0, sx1, sy1, COLOR_RGB(255, 235, 80), true);
        
        /* Target Reticle at predicted landing */
        for (int dy = -4; dy <= 4; ++dy) {
            for (int dx = -4; dx <= 4; ++dx) {
                int r2 = dx * dx + dy * dy;
                if (r2 >= 9 && r2 <= 16) {
                    set_pixel(rc, sx1 + dx, sy1 + dy, COLOR_RGB(255, 230, 40));
                }
            }
        }
    }
    
    /* 4. Render HUD & UI */
    render_hud(rc, gs);
}

bool render_save_bmp(const RenderContext *rc, const char *filename) {
    if (!rc || !rc->framebuffer || !filename) return false;
    
    FILE *f = fopen(filename, "wb");
    if (!f) return false;
    
    int w = SCREEN_W;
    int h = SCREEN_H;
    int row_bytes = w * 3; /* 640 * 3 = 1920 (multiple of 4) */
    int img_size = row_bytes * h;
    int file_size = 54 + img_size;
    
    uint8_t header[54];
    memset(header, 0, 54);
    
    /* BMP Magic */
    header[0] = 'B';
    header[1] = 'M';
    /* File size */
    header[2] = (uint8_t)(file_size);
    header[3] = (uint8_t)(file_size >> 8);
    header[4] = (uint8_t)(file_size >> 16);
    header[5] = (uint8_t)(file_size >> 24);
    /* Offset to pixels */
    header[10] = 54;
    /* DIB Header Size */
    header[14] = 40;
    /* Width */
    header[18] = (uint8_t)(w);
    header[19] = (uint8_t)(w >> 8);
    header[20] = (uint8_t)(w >> 16);
    header[21] = (uint8_t)(w >> 24);
    /* Height (positive = bottom-up) */
    header[22] = (uint8_t)(h);
    header[23] = (uint8_t)(h >> 8);
    header[24] = (uint8_t)(h >> 16);
    header[25] = (uint8_t)(h >> 24);
    /* Color planes */
    header[26] = 1;
    /* Bits per pixel */
    header[28] = 24;
    /* Image size */
    header[34] = (uint8_t)(img_size);
    header[35] = (uint8_t)(img_size >> 8);
    header[36] = (uint8_t)(img_size >> 16);
    header[37] = (uint8_t)(img_size >> 24);
    
    fwrite(header, 1, 54, f);
    
    /* Write rows bottom-up in BGR format */
    for (int y = h - 1; y >= 0; --y) {
        const uint32_t *src_row = &rc->framebuffer[y * w];
        for (int x = 0; x < w; ++x) {
            uint32_t pixel = src_row[x];
            uint8_t b = (uint8_t)(pixel & 0xFF);
            uint8_t g = (uint8_t)((pixel >> 8) & 0xFF);
            uint8_t r = (uint8_t)((pixel >> 16) & 0xFF);
            fputc(b, f);
            fputc(g, f);
            fputc(r, f);
        }
    }
    
    fclose(f);
    return true;
}

