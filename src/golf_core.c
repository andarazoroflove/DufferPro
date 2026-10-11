#include "golf_core.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* 256-entry Sine Lookup Table (16.16 Fixed Point) */
const int32_t g_sin_lut[256] = {
         0,   1608,   3216,   4821,   6424,   8022,   9616,  11204,
     12785,  14359,  15924,  17479,  19024,  20557,  22078,  23586,
     25080,  26558,  28020,  29466,  30893,  32303,  33692,  35062,
     36410,  37736,  39040,  40320,  41576,  42806,  44011,  45190,
     46341,  47464,  48559,  49624,  50660,  51665,  52639,  53581,
     54491,  55368,  56212,  57022,  57798,  58538,  59244,  59914,
     60547,  61145,  61705,  62228,  62714,  63162,  63572,  63944,
     64277,  64571,  64827,  65043,  65220,  65358,  65457,  65516,
     65536,  65516,  65457,  65358,  65220,  65043,  64827,  64571,
     64277,  63944,  63572,  63162,  62714,  62228,  61705,  61145,
     60547,  59914,  59244,  58538,  57798,  57022,  56212,  55368,
     54491,  53581,  52639,  51665,  50660,  49624,  48559,  47464,
     46341,  45190,  44011,  42806,  41576,  40320,  39040,  37736,
     36410,  35062,  33692,  32303,  30893,  29466,  28020,  26558,
     25080,  23586,  22078,  20557,  19024,  17479,  15924,  14359,
     12785,  11204,   9616,   8022,   6424,   4821,   3216,   1608,
         0,  -1608,  -3216,  -4821,  -6424,  -8022,  -9616, -11204,
    -12785, -14359, -15924, -17479, -19024, -20557, -22078, -23586,
    -25080, -26558, -28020, -29466, -30893, -32303, -33692, -35062,
    -36410, -37736, -39040, -40320, -41576, -42806, -44011, -45190,
    -46341, -47464, -48559, -49624, -50660, -51665, -52639, -53581,
    -54491, -55368, -56212, -57022, -57798, -58538, -59244, -59914,
    -60547, -61145, -61705, -62228, -62714, -63162, -63572, -63944,
    -64277, -64571, -64827, -65043, -65220, -65358, -65457, -65516,
    -65536, -65516, -65457, -65358, -65220, -65043, -64827, -64571,
    -64277, -63944, -63572, -63162, -62714, -62228, -61705, -61145,
    -60547, -59914, -59244, -58538, -57798, -57022, -56212, -55368,
    -54491, -53581, -52639, -51665, -50660, -49624, -48559, -47464,
    -46341, -45190, -44011, -42806, -41576, -40320, -39040, -37736,
    -36410, -35062, -33692, -32303, -30893, -29466, -28020, -26558,
    -25080, -23586, -22078, -20557, -19024, -17479, -15924, -14359,
    -12785, -11204,  -9616,  -8022,  -6424,  -4821,  -3216,  -1608
};

/* Exact 33-entry Octant Atan2 LUT (0..32 binary radians) */
static const uint8_t s_octant_atan2[33] = {
    0, 1, 3, 4, 5, 6, 8, 9, 10, 11, 12, 13, 15, 16, 17, 18,
    19, 20, 21, 22, 23, 24, 25, 25, 26, 27, 28, 29, 29, 30, 31, 31, 32
};

/* Fast, precise binary-radian atan2 */
uint8_t fixed_atan2(fixed dy, fixed dx) {
    if (dx == 0 && dy == 0) return 0;
    
    fixed ax = FABS(dx);
    fixed ay = FABS(dy);
    int32_t angle = 0;
    
    if (ax >= ay) {
        if (ax == 0) return 0;
        int32_t idx = (int32_t)(((int64_t)ay * 32) / ax);
        if (idx > 32) idx = 32;
        angle = s_octant_atan2[idx];
    } else {
        int32_t idx = (int32_t)(((int64_t)ax * 32) / ay);
        if (idx > 32) idx = 32;
        angle = 64 - s_octant_atan2[idx];
    }
    
    if (dx >= 0 && dy >= 0) {
        return (uint8_t)angle;
    } else if (dx < 0 && dy >= 0) {
        return (uint8_t)(128 - angle);
    } else if (dx < 0 && dy < 0) {
        return (uint8_t)(128 + angle);
    } else {
        return (uint8_t)(256 - angle);
    }
}

/* Club Definitions (Descending order of power/distance) */
static const ClubData s_clubs[CLUB_COUNT] = {
    { "DRIVER DR",   "DR", 260, 39500, 180000, 28000, 60500, false }, /* Driver: ~260 yds */
    { "1-WOOD 1W",   "1W", 240, 36000, 185000, 28000, 60293, false }, /* 1 Wood: ~240 yds */
    { "2-WOOD 2W",   "2W", 225, 33000, 192000, 27000, 60000, false }, /* 2 Wood: ~225 yds */
    { "3-WOOD 3W",   "3W", 210, 30000, 200000, 26000, 59637, false }, /* 3 Wood: ~210 yds */
    { "4-IRON 4I",   "4I", 195, 26500, 208000, 24000, 59000, false }, /* 4 Iron: ~195 yds */
    { "5-IRON 5I",   "5I", 180, 23500, 215000, 22000, 58327, false }, /* 5 Iron: ~180 yds */
    { "6-IRON 6I",   "6I", 165, 21000, 220000, 20000, 57600, false }, /* 6 Iron: ~165 yds */
    { "7-IRON 7I",   "7I", 150, 18500, 224000, 18000, 57000, false }, /* 7 Iron: ~150 yds */
    { "8-IRON 8I",   "8I", 135, 16400, 228000, 16000, 56360, false }, /* 8 Iron: ~135 yds */
    { "9-IRON 9I",   "9I", 120, 14300, 232000, 14000, 55400, false }, /* 9 Iron: ~120 yds */
    { "P-WEDGE PW",  "PW", 105, 12200, 236000, 12000, 54400, false }, /* Pitching Wedge: ~105 yds */
    { "S-WEDGE SW",  "SW",  85,  9800, 240000,  8000, 52000, false }, /* Sand Wedge: ~85 yds */
    { "L-WEDGE LW",  "LW",  65,  7500, 244000,  5000, 50000, false }, /* Lob Wedge: ~65 yds */
    { "PUTTER PT",   "PT",  25, 18000,      0,     0, 62259, true  }  /* Putter: up to 25 yds */
};

const ClubData* golf_get_club_data(ClubType club) {
    if (club >= CLUB_COUNT) club = CLUB_DRV;
    return &s_clubs[club];
}

static ClubType recommend_club_for_distance(int32_t yds, SurfaceType surf) {
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

/* Fast PRNG */
static uint32_t s_rng_state = 123456789;
static uint32_t core_rand(void) {
    s_rng_state = s_rng_state * 1103515245 + 12345;
    return (s_rng_state >> 16) & 0x7FFF;
}
static void core_srand(uint32_t seed) {
    s_rng_state = seed ? seed : 123456789;
}

/* Physics parameters in 16.16 fixed point */
#define GRAVITY_FIXED        5200   /* ~0.079 pixels/tick^2 downward */
#define AIR_DRAG_FIXED      65405   /* ~0.998 air drag on horizontal velocity */
#define WIND_FACTOR_FIXED        2   /* Wind drift multiplier */
#define SLOPE_ACCEL_FIXED     300   /* Elevation slope acceleration */
#define STOP_SPEED_THRESHOLD  800   /* Threshold to stop rolling */
#define CUP_RADIUS_FIXED    28000   /* Cup capture radius (~0.42 tiles) */
#define CUP_MAX_SPEED       26000   /* Maximum entry speed to sink */

/* Elevation height step query */
fixed golf_get_ground_z_fixed(const GolfState *gs, fixed fx, fixed fy) {
    int32_t gx = FROM_FIXED(fx);
    int32_t gy = FROM_FIXED(fy);
    if (gx < 0) gx = 0;
    if (gx >= GRID_W) gx = GRID_W - 1;
    if (gy < 0) gy = 0;
    if (gy >= GRID_H) gy = GRID_H - 1;
    return TO_FIXED(gs->grid[gx][gy].elevation * HEIGHT_STEP_PX);
}

SurfaceType golf_get_surface_at(const GolfState *gs, fixed fx, fixed fy) {
    int32_t gx = FROM_FIXED(fx);
    int32_t gy = FROM_FIXED(fy);
    if (gx < 0 || gx >= GRID_W || gy < 0 || gy >= GRID_H) {
        return SURF_ROUGH;
    }
    return (SurfaceType)gs->grid[gx][gy].surface;
}

int32_t golf_distance_to_pin_yards(const GolfState *gs) {
    if (!gs->has_pin) return 0;
    fixed target_x = TO_FIXED(gs->pin_gx) + FIXED_HALF;
    fixed target_y = TO_FIXED(gs->pin_gy) + FIXED_HALF;
    fixed dx = target_x - gs->ball.x;
    fixed dy = target_y - gs->ball.y;
    fixed dist_tiles = fixed_sqrt(FMUL(dx, dx) + FMUL(dy, dy));
    return FROM_FIXED(FMUL(dist_tiles, TO_FIXED(YARDS_PER_TILE)));
}

void golf_update_autotile_masks(GolfState *gs) {
    for (int y = 0; y < GRID_H; ++y) {
        for (int x = 0; x < GRID_W; ++x) {
            uint8_t surf = gs->grid[x][y].surface;
            uint8_t mask = 0;
            
            uint8_t nw_surf = (y > 0) ? gs->grid[x][y - 1].surface : surf;
            uint8_t ne_surf = (x < GRID_W - 1) ? gs->grid[x + 1][y].surface : surf;
            uint8_t se_surf = (y < GRID_H - 1) ? gs->grid[x][y + 1].surface : surf;
            uint8_t sw_surf = (x > 0) ? gs->grid[x - 1][y].surface : surf;
            
            if (surf == SURF_FAIRWAY || surf == SURF_TEE) {
                if (nw_surf == SURF_FAIRWAY || nw_surf == SURF_GREEN || nw_surf == SURF_TEE || nw_surf == SURF_BRIDGE) mask |= MASK_NW;
                if (ne_surf == SURF_FAIRWAY || ne_surf == SURF_GREEN || ne_surf == SURF_TEE || ne_surf == SURF_BRIDGE) mask |= MASK_NE;
                if (se_surf == SURF_FAIRWAY || se_surf == SURF_GREEN || se_surf == SURF_TEE || se_surf == SURF_BRIDGE) mask |= MASK_SE;
                if (sw_surf == SURF_FAIRWAY || sw_surf == SURF_GREEN || sw_surf == SURF_TEE || sw_surf == SURF_BRIDGE) mask |= MASK_SW;
            } else if (surf == SURF_GREEN) {
                if (nw_surf == SURF_GREEN) mask |= MASK_NW;
                if (ne_surf == SURF_GREEN) mask |= MASK_NE;
                if (se_surf == SURF_GREEN) mask |= MASK_SE;
                if (sw_surf == SURF_GREEN) mask |= MASK_SW;
            } else if (surf == SURF_SAND) {
                if (nw_surf == SURF_SAND) mask |= MASK_NW;
                if (ne_surf == SURF_SAND) mask |= MASK_NE;
                if (se_surf == SURF_SAND) mask |= MASK_SE;
                if (sw_surf == SURF_SAND) mask |= MASK_SW;
            } else if (surf == SURF_CARTPATH) {
                if (nw_surf == SURF_CARTPATH || nw_surf == SURF_BRIDGE) mask |= MASK_NW;
                if (ne_surf == SURF_CARTPATH || ne_surf == SURF_BRIDGE) mask |= MASK_NE;
                if (se_surf == SURF_CARTPATH || se_surf == SURF_BRIDGE) mask |= MASK_SE;
                if (sw_surf == SURF_CARTPATH || sw_surf == SURF_BRIDGE) mask |= MASK_SW;
            }
            
            gs->grid[x][y].autotile_mask = mask;
        }
    }
}

void golf_recalc_hole_stats(GolfState *gs) {
    if (!gs->has_tee || !gs->has_pin) {
        gs->hole_yards = 0;
        gs->par = 4;
        return;
    }
    
    fixed dx = TO_FIXED(gs->pin_gx - gs->tee_gx);
    fixed dy = TO_FIXED(gs->pin_gy - gs->tee_gy);
    fixed dist_tiles = fixed_sqrt(FMUL(dx, dx) + FMUL(dy, dy));
    gs->hole_yards = FROM_FIXED(FMUL(dist_tiles, TO_FIXED(YARDS_PER_TILE)));
    
    if (gs->hole_yards < 220) {
        gs->par = 3;
    } else if (gs->hole_yards > 430) {
        gs->par = 5;
    } else {
        gs->par = 4;
    }
    
    golf_recalc_course_rating(gs);
}

void golf_recalc_course_rating(GolfState *gs) {
    int32_t scenery_score = 0;
    int32_t water_tiles = 0;
    int32_t sand_tiles = 0;
    int32_t fairway_tiles = 0;
    int32_t elevation_diversity = 0;
    uint8_t elev_seen[MAX_ELEVATION_TIER + 1] = {0};
    
    for (int y = 0; y < GRID_H; ++y) {
        for (int x = 0; x < GRID_W; ++x) {
            uint8_t s = gs->grid[x][y].surface;
            uint8_t f = gs->grid[x][y].feature;
            uint8_t e = gs->grid[x][y].elevation;
            
            if (e <= MAX_ELEVATION_TIER) elev_seen[e] = 1;
            if (s == SURF_WATER) water_tiles++;
            if (s == SURF_SAND) sand_tiles++;
            if (s == SURF_FAIRWAY) fairway_tiles++;
            
            if (f == FEAT_PINE || f == FEAT_OAK || f == FEAT_PALM || f == FEAT_FLOWERING) scenery_score += 2;
            else if (f == FEAT_FLOWERS || f == FEAT_BUSH) scenery_score += 3;
            else if (f == FEAT_BENCH || f == FEAT_CART) scenery_score += 4;
            else if (f == FEAT_ROCK) scenery_score += 1;
        }
    }
    
    for (int i = 0; i <= MAX_ELEVATION_TIER; ++i) {
        if (elev_seen[i]) elevation_diversity += 8;
    }
    
    int32_t rating = 25; /* Baseline */
    if (gs->has_clubhouse) rating += 25;
    if (gs->has_tee && gs->has_pin) rating += 10;
    
    rating += (scenery_score > 30) ? 30 : scenery_score;
    rating += (elevation_diversity > 30) ? 30 : elevation_diversity;
    if (water_tiles > 20) rating += 10;
    if (sand_tiles >= 8) rating += 10;
    if (fairway_tiles > 60) rating += 15;
    
    /* Normalize to 0..100 (maps to 0..5 stars) */
    if (rating > 100) rating = 100;
    if (rating < 10) rating = 10;
    gs->fun_rating = rating;
    
    /* Difficulty Rating (1..10) */
    int32_t diff = 1;
    if (gs->hole_yards > 380) diff += 3;
    else if (gs->hole_yards > 280) diff += 2;
    else diff += 1;
    
    if (water_tiles > 35) diff += 3;
    else if (water_tiles > 10) diff += 2;
    
    if (sand_tiles > 15) diff += 2;
    if (diff > 10) diff = 10;
    gs->difficulty_rating = diff;
}

void golf_set_banner(GolfState *gs, const char *msg, int32_t ticks) {
    if (!msg) return;
    strncpy(gs->banner_text, msg, sizeof(gs->banner_text) - 1);
    gs->banner_text[sizeof(gs->banner_text) - 1] = '\0';
    gs->banner_timer = ticks;
}
#define set_banner golf_set_banner

void golf_generate_terrain_scaled(GolfState *gs, uint32_t seed, int trees, int bushes, int rocks, int water) {
    core_srand(seed);
    gs->current_seed = seed;
    
    if (trees < 1) trees = 1; if (trees > 5) trees = 5;
    if (bushes < 1) bushes = 1; if (bushes > 5) bushes = 5;
    if (rocks < 1) rocks = 1; if (rocks > 5) rocks = 5;
    if (water < 1) water = 1; if (water > 5) water = 5;
    
    gs->tree_scale = trees;
    gs->bush_scale = bushes;
    gs->rock_scale = rocks;
    gs->water_scale = water;
    
    /* 1. Value noise height generation (0..5 tiers) - static to preserve GBA IWRAM stack */
    static int16_t s_noise[GRID_W][GRID_H];
    static int16_t s_temp[GRID_W][GRID_H];
    for (int y = 0; y < GRID_H; ++y) {
        for (int x = 0; x < GRID_W; ++x) {
            s_noise[x][y] = (int16_t)(core_rand() % 100);
        }
    }
    
    /* 3 passes of 3x3 box smoothing */
    for (int pass = 0; pass < 3; ++pass) {
        for (int y = 0; y < GRID_H; ++y) {
            for (int x = 0; x < GRID_W; ++x) {
                int32_t sum = 0;
                int32_t count = 0;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx >= 0 && nx < GRID_W && ny >= 0 && ny < GRID_H) {
                            sum += s_noise[nx][ny];
                            count++;
                        }
                    }
                }
                s_temp[x][y] = (int16_t)(sum / count);
            }
        }
        memcpy(s_noise, s_temp, sizeof(s_noise));
    }
    
    /* Water threshold based on 1..5 scale (1: minimal water, 5: large water hazards) */
    int32_t water_thresh = 14 + water * 6; /* 20, 26, 32, 38, 44 */
    int32_t tier_step = (100 - water_thresh) / 4;
    if (tier_step < 10) tier_step = 10;
    
    int32_t obstacle_count = 0;
    const int32_t MAX_OBSTACLES = 1433; /* Hard clamp: max 70% of 2048 map tiles */
    
    /* Map noise to discrete elevation tiers */
    for (int y = 0; y < GRID_H; ++y) {
        for (int x = 0; x < GRID_W; ++x) {
            int32_t v = s_noise[x][y];
            uint8_t elev;
            uint8_t surf;
            
            if (v < water_thresh) {
                elev = ELEV_WATER;
                surf = SURF_WATER;
                obstacle_count++;
            } else if (v < water_thresh + tier_step) {
                elev = ELEV_LOWLAND;
                surf = SURF_ROUGH;
            } else if (v < water_thresh + tier_step * 2) {
                elev = ELEV_VALLEY;
                surf = SURF_ROUGH;
            } else if (v < water_thresh + tier_step * 3) {
                elev = ELEV_RIDGE;
                surf = SURF_ROUGH;
            } else {
                elev = ELEV_PLATEAU;
                surf = SURF_ROUGH;
            }
            
            gs->grid[x][y].elevation = elev;
            gs->grid[x][y].surface = surf;
            gs->grid[x][y].feature = FEAT_NONE;
            gs->grid[x][y].autotile_mask = 0;
            gs->grid[x][y].variant = (uint8_t)(core_rand() % 4);
        }
    }
    
    /* Scale feature densities (1..5):
     * Trees: 3% to 15% (trees * 3)
     * Bushes: 3% to 15% (bushes * 3)
     * Rocks: 2% to 10% (rocks * 2)
     * Clamped strictly to MAX_OBSTACLES (70% coverage limit)
     */
    int tree_pct = trees * 3;
    int bush_pct = bushes * 3;
    int rock_pct = rocks * 2;
    
    for (int y = 2; y < GRID_H - 2; ++y) {
        for (int x = 2; x < GRID_W - 2; ++x) {
            if (obstacle_count >= MAX_OBSTACLES) break;
            
            if (gs->grid[x][y].surface == SURF_ROUGH) {
                uint32_t r = core_rand() % 100;
                if ((int)r < tree_pct) {
                    uint32_t tr = core_rand() % 4;
                    if (tr == 0) gs->grid[x][y].feature = FEAT_PINE;
                    else if (tr == 1) gs->grid[x][y].feature = FEAT_OAK;
                    else if (tr == 2) gs->grid[x][y].feature = FEAT_PALM;
                    else gs->grid[x][y].feature = FEAT_FLOWERING;
                    obstacle_count++;
                } else if ((int)r < tree_pct + bush_pct) {
                    gs->grid[x][y].feature = FEAT_BUSH;
                    obstacle_count++;
                } else if ((int)r < tree_pct + bush_pct + rock_pct) {
                    gs->grid[x][y].feature = FEAT_ROCK;
                    obstacle_count++;
                }
            }
        }
    }
    
    golf_update_autotile_masks(gs);
}

void golf_generate_terrain(GolfState *gs, uint32_t seed) {
    int t = gs->tree_scale ? gs->tree_scale : 3;
    int b = gs->bush_scale ? gs->bush_scale : 3;
    int r = gs->rock_scale ? gs->rock_scale : 3;
    int w = gs->water_scale ? gs->water_scale : 3;
    golf_generate_terrain_scaled(gs, seed, t, b, r, w);
}

void golf_place_default_routing(GolfState *gs) {
    /* 1. Tee Box Placement */
    gs->tee_gx = 10;
    gs->tee_gy = 55;
    gs->has_tee = true;
    
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int tx = gs->tee_gx + dx;
            int ty = gs->tee_gy + dy;
            if (tx >= 0 && tx < GRID_W && ty >= 0 && ty < GRID_H) {
                if (gs->grid[tx][ty].elevation == 0) gs->grid[tx][ty].elevation = ELEV_LOWLAND;
                gs->grid[tx][ty].surface = SURF_TEE;
                gs->grid[tx][ty].feature = FEAT_NONE;
            }
        }
    }
    
    /* 2. Green & Cup Placement */
    gs->pin_gx = 16;
    gs->pin_gy = 8;
    gs->has_pin = true;
    
    for (int dy = -3; dy <= 3; ++dy) {
        for (int dx = -3; dx <= 3; ++dx) {
            if (dx * dx + dy * dy <= 10) {
                int px = gs->pin_gx + dx;
                int py = gs->pin_gy + dy;
                if (px >= 0 && px < GRID_W && py >= 0 && py < GRID_H) {
                    if (gs->grid[px][py].elevation == 0) gs->grid[px][py].elevation = ELEV_VALLEY;
                    gs->grid[px][py].surface = SURF_GREEN;
                    gs->grid[px][py].feature = FEAT_NONE;
                }
            }
        }
    }
    
    /* 3. Fairway Routing corridor */
    int dy_total = (gs->tee_gy - 1) - (gs->pin_gy + 4);
    for (int y = gs->pin_gy + 4; y < gs->tee_gy - 1; ++y) {
        int center_x = (dy_total > 0) ? (gs->pin_gx + ((y - (gs->pin_gy + 4)) * (gs->tee_gx - gs->pin_gx)) / dy_total) : gs->pin_gx;
        int half_w = 2 + (int)((core_rand() % 3));
        
        for (int x = center_x - half_w; x <= center_x + half_w; ++x) {
            if (x >= 1 && x < GRID_W - 1) {
                if (gs->grid[x][y].surface != SURF_GREEN && gs->grid[x][y].surface != SURF_TEE) {
                    if (gs->grid[x][y].elevation == 0) {
                        /* Bridge across water hazard! */
                        gs->grid[x][y].surface = SURF_BRIDGE;
                    } else {
                        gs->grid[x][y].surface = SURF_FAIRWAY;
                    }
                    gs->grid[x][y].feature = FEAT_NONE;
                }
            }
        }
    }
    
    /* 4. Strategic Green-side Bunker */
    for (int dy = -1; dy <= 2; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int bx = gs->pin_gx + 4 + dx;
            int by = gs->pin_gy + 1 + dy;
            if (bx >= 0 && bx < GRID_W && by >= 0 && by < GRID_H) {
                if (gs->grid[bx][by].elevation == 0) gs->grid[bx][by].elevation = ELEV_VALLEY;
                gs->grid[bx][by].surface = SURF_SAND;
                gs->grid[bx][by].feature = FEAT_NONE;
            }
        }
    }
    
    /* 5. Cart Path from Clubhouse to Tee */
    for (int y = gs->tee_gy - 4; y <= gs->tee_gy + 2; ++y) {
        int x = gs->tee_gx - 3;
        if (x >= 0 && x < GRID_W && y >= 0 && y < GRID_H) {
            if (gs->grid[x][y].elevation == 0) gs->grid[x][y].elevation = ELEV_LOWLAND;
            gs->grid[x][y].surface = SURF_CARTPATH;
            gs->grid[x][y].feature = FEAT_NONE;
        }
    }
    
    /* 6. Place Stately Golf Clubhouse */
    golf_place_clubhouse(gs, gs->tee_gx - 6, gs->tee_gy - 2);
    
    /* 7. Bench & Golf Cart decor */
    if (gs->tee_gx + 2 < GRID_W) {
        gs->grid[gs->tee_gx + 2][gs->tee_gy].feature = FEAT_BENCH;
    }
    if (gs->tee_gx - 3 >= 0 && gs->tee_gy + 1 < GRID_H) {
        gs->grid[gs->tee_gx - 3][gs->tee_gy + 1].feature = FEAT_CART;
    }
    
    /* 8. Setup Spectators */
    gs->spectator_count = 0;
    
    /* Clubhouse patio spectators */
    if (gs->has_clubhouse) {
        int sx = gs->clubhouse_gx + 2;
        int sy = gs->clubhouse_gy + 3;
        for (int i = 0; i < 4; ++i) {
            if (gs->spectator_count < MAX_SPECTATORS) {
                SpectatorEntity *sp = &gs->spectators[gs->spectator_count++];
                sp->gx = sx + (i % 2);
                sp->gy = sy + (i / 2);
                sp->current_z = golf_get_ground_z_fixed(gs, TO_FIXED(sp->gx) + FIXED_HALF, TO_FIXED(sp->gy) + FIXED_HALF);
                sp->state = SPEC_STATE_IDLE;
                sp->shirt_color_idx = (uint8_t)(i % 5);
                sp->facing_angle = 192; /* facing North towards course */
                sp->anim_frame = i % 2;
                sp->timer = i * 15;
                sp->active = true;
            }
        }
    }
    
    /* Tee box spectators */
    for (int i = 0; i < 3; ++i) {
        if (gs->spectator_count < MAX_SPECTATORS) {
            SpectatorEntity *sp = &gs->spectators[gs->spectator_count++];
            sp->gx = gs->tee_gx + 3;
            sp->gy = gs->tee_gy - 1 + i;
            sp->current_z = golf_get_ground_z_fixed(gs, TO_FIXED(sp->gx) + FIXED_HALF, TO_FIXED(sp->gy) + FIXED_HALF);
            sp->state = SPEC_STATE_IDLE;
            sp->shirt_color_idx = (uint8_t)((i + 2) % 5);
            sp->facing_angle = 192;
            sp->anim_frame = 0;
            sp->timer = i * 20;
            sp->active = true;
        }
    }
    
    /* Green-side gallery spectators */
    for (int i = 0; i < 4; ++i) {
        if (gs->spectator_count < MAX_SPECTATORS) {
            SpectatorEntity *sp = &gs->spectators[gs->spectator_count++];
            sp->gx = gs->pin_gx - 4;
            sp->gy = gs->pin_gy - 2 + i * 2;
            if (sp->gx >= 0 && sp->gy >= 0 && sp->gy < GRID_H) {
                sp->current_z = golf_get_ground_z_fixed(gs, TO_FIXED(sp->gx) + FIXED_HALF, TO_FIXED(sp->gy) + FIXED_HALF);
                sp->state = SPEC_STATE_IDLE;
                sp->shirt_color_idx = (uint8_t)((i + 1) % 5);
                sp->facing_angle = 0; /* facing East towards green */
                sp->anim_frame = 0;
                sp->timer = i * 10;
                sp->active = true;
            }
        }
    }
    
    /* 9. Setup Beverage Cart Girl */
    gs->bev_cart.active = true;
    gs->bev_cart.x = TO_FIXED(gs->tee_gx - 3) + FIXED_HALF;
    gs->bev_cart.y = TO_FIXED(gs->tee_gy + 2) + FIXED_HALF;
    gs->bev_cart.current_z = golf_get_ground_z_fixed(gs, gs->bev_cart.x, gs->bev_cart.y);
    gs->bev_cart.target_x = TO_FIXED(gs->tee_gx) + FIXED_HALF;
    gs->bev_cart.target_y = TO_FIXED(gs->tee_gy - 10) + FIXED_HALF;
    gs->bev_cart.state = BEV_STATE_DRIVING;
    gs->bev_cart.facing_angle = 192;
    gs->bev_cart.speed = 2200; /* gentle cart cruising speed */
    gs->bev_cart.whistle_timer = 200 + (core_rand() % 250);
    gs->bev_cart.speech_timer = 0;
    gs->bev_cart.wait_timer = 0;
    
    /* 10. Setup Gopher Entity & Initial Holes */
    gs->gopher.state = GOPHER_STATE_HIDDEN;
    gs->gopher.timer = 0;
    gs->gopher.anim_frame = 0;
    gs->gopher.pop_cooldown = 2400 + (core_rand() % 1200); /* Rare random pop-up (40-60s) */
    gs->gopher.visible = false;
    gs->gopher.hole_count = 0;
    
    /* Pre-seed 2 gopher hole hazards along the corridor */
    int32_t mid_gx = (gs->tee_gx + gs->pin_gx) / 2;
    int32_t mid_gy = (gs->tee_gy + gs->pin_gy) / 2;
    if (mid_gx >= 2 && mid_gx < GRID_W - 2 && mid_gy >= 2 && mid_gy < GRID_H - 2) {
        GopherHole *h1 = &gs->gopher.holes[gs->gopher.hole_count++];
        h1->gx = mid_gx + 1;
        h1->gy = mid_gy;
        h1->current_z = golf_get_ground_z_fixed(gs, TO_FIXED(h1->gx) + FIXED_HALF, TO_FIXED(h1->gy) + FIXED_HALF);
        h1->active = true;
    }
    int32_t near_green_gx = gs->pin_gx - 2;
    int32_t near_green_gy = gs->pin_gy + 3;
    if (near_green_gx >= 2 && near_green_gx < GRID_W - 2 && near_green_gy >= 2 && near_green_gy < GRID_H - 2) {
        GopherHole *h2 = &gs->gopher.holes[gs->gopher.hole_count++];
        h2->gx = near_green_gx;
        h2->gy = near_green_gy;
        h2->current_z = golf_get_ground_z_fixed(gs, TO_FIXED(h2->gx) + FIXED_HALF, TO_FIXED(h2->gy) + FIXED_HALF);
        h2->active = true;
    }
    
    /* 11. Setup Carl Spackler Greenskeeper Nemesis */
    memset(&gs->carl, 0, sizeof(gs->carl));
    gs->carl.state = CARL_STATE_INACTIVE;
    gs->carl.active = false;
    gs->gopher_encounter_count = 0;
    gs->carl_cooldown = 480; /* ~8 seconds before first possible Carl appearance */
    
    golf_update_autotile_masks(gs);
    golf_recalc_hole_stats(gs);
    golf_reset_ball_to_tee(gs);

    /* Snapshot initial layout for Replay Hole */
    memcpy(gs->initial_grid, gs->grid, sizeof(gs->grid));
    gs->initial_tee_gx = gs->tee_gx;
    gs->initial_tee_gy = gs->tee_gy;
    gs->initial_pin_gx = gs->pin_gx;
    gs->initial_pin_gy = gs->pin_gy;
    gs->initial_ch_gx = gs->clubhouse_gx;
    gs->initial_ch_gy = gs->clubhouse_gy;
    gs->initial_has_ch = gs->has_clubhouse;
}

bool golf_place_clubhouse(GolfState *gs, int32_t gx, int32_t gy) {
    if (gx < 0 || gx + 3 >= GRID_W || gy < 0 || gy + 2 >= GRID_H) return false;
    
    /* Level the ground for the clubhouse */
    uint8_t elev = gs->grid[gx][gy].elevation;
    if (elev == 0) elev = ELEV_LOWLAND;
    
    for (int dy = 0; dy <= 2; ++dy) {
        for (int dx = 0; dx <= 3; ++dx) {
            int tx = gx + dx;
            int ty = gy + dy;
            gs->grid[tx][ty].elevation = elev;
            gs->grid[tx][ty].surface = SURF_ROUGH;
            gs->grid[tx][ty].feature = FEAT_CLUBHOUSE;
        }
    }
    
    gs->clubhouse_gx = gx;
    gs->clubhouse_gy = gy;
    gs->has_clubhouse = true;
    golf_recalc_course_rating(gs);
    return true;
}

void golf_remove_clubhouse(GolfState *gs) {
    if (!gs->has_clubhouse) return;
    for (int dy = 0; dy <= 2; ++dy) {
        for (int dx = 0; dx <= 3; ++dx) {
            int tx = gs->clubhouse_gx + dx;
            int ty = gs->clubhouse_gy + dy;
            if (tx >= 0 && tx < GRID_W && ty >= 0 && ty < GRID_H) {
                if (gs->grid[tx][ty].feature == FEAT_CLUBHOUSE) {
                    gs->grid[tx][ty].feature = FEAT_NONE;
                }
            }
        }
    }
    gs->has_clubhouse = false;
    golf_recalc_course_rating(gs);
}

void golf_randomize_hole(GolfState *gs, uint32_t seed) {
    if (gs->is_sneak_on) {
        gs->tree_scale = 1 + (core_rand() % 5);
        gs->bush_scale = 1 + (core_rand() % 5);
        gs->rock_scale = 1 + (core_rand() % 5);
        gs->water_scale = 1 + (core_rand() % 5);
        gs->theme = (CourseTheme)(core_rand() % THEME_COUNT);
    }
    
    golf_generate_terrain_scaled(gs, seed, gs->tree_scale ? gs->tree_scale : 3,
                                           gs->bush_scale ? gs->bush_scale : 3,
                                           gs->rock_scale ? gs->rock_scale : 3,
                                           gs->water_scale ? gs->water_scale : 3);
    
    /* Find valid dry land for Pin (upper half rows 6..14) */
    int pin_candidates[32][2];
    int pin_count = 0;
    for (int y = 6; y <= 14; ++y) {
        for (int x = 6; x < GRID_W - 6; ++x) {
            if (gs->grid[x][y].surface != SURF_WATER && gs->grid[x][y].elevation > 0) {
                pin_candidates[pin_count][0] = x;
                pin_candidates[pin_count][1] = y;
                pin_count++;
                if (pin_count >= 32) break;
            }
        }
        if (pin_count >= 32) break;
    }
    
    if (pin_count > 0) {
        int idx = (int)(core_rand() % pin_count);
        gs->pin_gx = pin_candidates[idx][0];
        gs->pin_gy = pin_candidates[idx][1];
    } else {
        gs->pin_gx = 16;
        gs->pin_gy = 8;
    }
    
    /* Find valid dry land for Tee (lower half rows 50..58) */
    int tee_candidates[32][2];
    int tee_count = 0;
    for (int y = 50; y <= 58; ++y) {
        for (int x = 4; x < GRID_W - 4; ++x) {
            if (gs->grid[x][y].surface != SURF_WATER && gs->grid[x][y].elevation > 0) {
                tee_candidates[tee_count][0] = x;
                tee_candidates[tee_count][1] = y;
                tee_count++;
                if (tee_count >= 32) break;
            }
        }
        if (tee_count >= 32) break;
    }
    
    if (tee_count > 0) {
        int idx = (int)(core_rand() % tee_count);
        gs->tee_gx = tee_candidates[idx][0];
        gs->tee_gy = tee_candidates[idx][1];
    } else {
        gs->tee_gx = 10;
        gs->tee_gy = 55;
    }
    
    gs->has_tee = true;
    gs->has_pin = true;
    
    /* Stamp Tee Box over generated terrain */
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int tx = gs->tee_gx + dx;
            int ty = gs->tee_gy + dy;
            if (tx >= 0 && tx < GRID_W && ty >= 0 && ty < GRID_H) {
                if (gs->grid[tx][ty].elevation == 0) gs->grid[tx][ty].elevation = ELEV_LOWLAND;
                gs->grid[tx][ty].surface = SURF_TEE;
                gs->grid[tx][ty].feature = FEAT_NONE;
            }
        }
    }
    
    /* Stamp Putting Green over generated terrain */
    for (int dy = -3; dy <= 3; ++dy) {
        for (int dx = -3; dx <= 3; ++dx) {
            if (dx * dx + dy * dy <= 10) {
                int px = gs->pin_gx + dx;
                int py = gs->pin_gy + dy;
                if (px >= 0 && px < GRID_W && py >= 0 && py < GRID_H) {
                    if (gs->grid[px][py].elevation == 0) gs->grid[px][py].elevation = ELEV_VALLEY;
                    gs->grid[px][py].surface = SURF_GREEN;
                    gs->grid[px][py].feature = FEAT_NONE;
                }
            }
        }
    }
    
    /* Stamp Fairway Corridor over generated terrain: guarantees playability! */
    int start_y = gs->pin_gy + 4;
    int end_y = gs->tee_gy - 1;
    int dy_span = end_y - start_y;
    for (int y = start_y; y <= end_y; ++y) {
        int center_x = (dy_span > 0) ? (gs->pin_gx + ((y - start_y) * (gs->tee_gx - gs->pin_gx)) / dy_span) : gs->pin_gx;
        int half_w = 2 + (int)(core_rand() % 3);
        
        for (int x = center_x - half_w; x <= center_x + half_w; ++x) {
            if (x >= 1 && x < GRID_W - 1) {
                if (gs->grid[x][y].surface != SURF_GREEN && gs->grid[x][y].surface != SURF_TEE) {
                    if (gs->grid[x][y].elevation == 0) {
                        gs->grid[x][y].surface = SURF_BRIDGE;
                    } else {
                        gs->grid[x][y].surface = SURF_FAIRWAY;
                    }
                    gs->grid[x][y].feature = FEAT_NONE;
                }
            }
        }
    }
    
    /* Sand Bunkers */
    int bx = gs->pin_gx + 3;
    int by = gs->pin_gy + 2;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int tx = bx + dx;
            int ty = by + dy;
            if (tx >= 0 && tx < GRID_W && ty >= 0 && ty < GRID_H) {
                if (gs->grid[tx][ty].elevation == 0) gs->grid[tx][ty].elevation = ELEV_VALLEY;
                gs->grid[tx][ty].surface = SURF_SAND;
                gs->grid[tx][ty].feature = FEAT_NONE;
            }
        }
    }
    
    /* Clubhouse Placement */
    int ch_x = (gs->tee_gx >= 8) ? (gs->tee_gx - 6) : (gs->tee_gx + 3);
    int ch_y = gs->tee_gy - 2;
    golf_place_clubhouse(gs, ch_x, ch_y);
    
    /* Setup Theme Entities */
    if (gs->theme == THEME_LA_PLAYA) {
        for (int i = 0; i < MAX_TUMBLEWEEDS; ++i) {
            gs->tumbleweeds[i].active = (i < 4);
            gs->tumbleweeds[i].x = TO_FIXED(core_rand() % GRID_W) + FIXED_HALF;
            gs->tumbleweeds[i].y = TO_FIXED(core_rand() % (GRID_H / 2) + 10) + FIXED_HALF;
            gs->tumbleweeds[i].vx = (TO_FIXED(4) / 100) + (core_rand() % 4) * (TO_FIXED(1) / 100);
            gs->tumbleweeds[i].vy = (TO_FIXED(2) / 100) + (core_rand() % 4) * (TO_FIXED(1) / 100);
            gs->tumbleweeds[i].current_z = golf_get_ground_z_fixed(gs, gs->tumbleweeds[i].x, gs->tumbleweeds[i].y);
            gs->tumbleweeds[i].rotation = core_rand() % 360;
            gs->tumbleweeds[i].stuck = false;
        }
    } else {
        for (int i = 0; i < MAX_TUMBLEWEEDS; ++i) gs->tumbleweeds[i].active = false;
    }
    
    gs->sea_monster.active = false;
    gs->sea_monster.state = 0;
    gs->sea_monster.timer = 2400 + (core_rand() % 1800);
    
    if (gs->is_sneak_on) {
        gs->ranger.active = true;
        gs->ranger.state = RANGER_STATE_PATROLLING;
        gs->ranger.x = TO_FIXED(4) + FIXED_HALF;
        gs->ranger.y = TO_FIXED(GRID_H / 2) + FIXED_HALF;
        gs->ranger.current_z = golf_get_ground_z_fixed(gs, gs->ranger.x, gs->ranger.y);
        gs->ranger.target_x = TO_FIXED(GRID_W - 6) + FIXED_HALF;
        gs->ranger.target_y = TO_FIXED(4) + FIXED_HALF;
        gs->ranger.facing_angle = 192;
        gs->ranger.speed = 1350;
        gs->ranger.timer = 0;
        gs->ranger.speech_timer = 0;
        gs->ranger.cooldown = 480; /* 8-second grace period on start */
        gs->ranger.speech_msg[0] = '\0';
    } else {
        gs->ranger.active = false;
    }
    
    /* Snapshot initial layout for Replay Hole */
    memcpy(gs->initial_grid, gs->grid, sizeof(gs->grid));
    gs->initial_tee_gx = gs->tee_gx;
    gs->initial_tee_gy = gs->tee_gy;
    gs->initial_pin_gx = gs->pin_gx;
    gs->initial_pin_gy = gs->pin_gy;
    gs->initial_ch_gx = gs->clubhouse_gx;
    gs->initial_ch_gy = gs->clubhouse_gy;
    gs->initial_has_ch = gs->has_clubhouse;
    
    golf_update_autotile_masks(gs);
    golf_recalc_hole_stats(gs);
    golf_reset_ball_to_tee(gs);
    set_banner(gs, "NEW HOLE GENERATED!", 120);
}

void golf_replay_hole(GolfState *gs) {
    memcpy(gs->grid, gs->initial_grid, sizeof(gs->grid));
    gs->tee_gx = gs->initial_tee_gx;
    gs->tee_gy = gs->initial_tee_gy;
    gs->pin_gx = gs->initial_pin_gx;
    gs->pin_gy = gs->initial_pin_gy;
    gs->clubhouse_gx = gs->initial_ch_gx;
    gs->clubhouse_gy = gs->initial_ch_gy;
    gs->has_clubhouse = gs->initial_has_ch;
    
    gs->strokes = 0;
    gs->score_rel_par = 0;
    gs->hole_completed = false;
    gs->caddy_eaten = false;
    gs->caddy_eaten_timer = 0;
    gs->raver_passed_out_timer = 0;
    
    gs->gopher.state = GOPHER_STATE_HIDDEN;
    gs->gopher.timer = 0;
    gs->gopher.visible = false;
    gs->gopher.pop_cooldown = 2400 + (core_rand() % 1200);
    gs->gopher_encounter_count = 0;
    gs->carl.active = false;
    gs->carl.state = CARL_STATE_INACTIVE;
    gs->carl_cooldown = 3600;
    gs->sea_monster.active = false;
    gs->sea_monster.state = 0;
    gs->sea_monster.timer = 2400 + (core_rand() % 1800);
    gs->ranger.cooldown = 480;
    
    golf_reset_ball_to_tee(gs);
    golf_golfer_arrive_at_ball(gs);
    set_banner(gs, "REPLAYING HOLE", 120);
}

void golf_start_sneak_on(GolfState *gs) {
    gs->is_sneak_on = true;
    gs->tree_scale = 1 + (core_rand() % 5);
    gs->bush_scale = 1 + (core_rand() % 5);
    gs->rock_scale = 1 + (core_rand() % 5);
    gs->water_scale = 1 + (core_rand() % 5);
    gs->theme = (CourseTheme)(core_rand() % THEME_COUNT);
    uint32_t seed = core_rand() | ((uint32_t)core_rand() << 16);
    golf_randomize_hole(gs, seed);
    
    gs->mode = MODE_PLAY;
    set_banner(gs, "SNEAK ON ACTIVE! AVOID THE RANGER!", 180);
}

void golf_set_theme(GolfState *gs, CourseTheme theme) {
    if (theme >= THEME_COUNT) theme = THEME_BUSHWOOD;
    gs->theme = theme;
}

void golf_init(GolfState *gs) {
    memset(gs, 0, sizeof(GolfState));
    
    gs->mode = MODE_SPLASH;
    gs->theme = THEME_BUSHWOOD;
    gs->tree_scale = 3;
    gs->bush_scale = 3;
    gs->rock_scale = 3;
    gs->water_scale = 3;
    gs->is_sneak_on = false;
    gs->caddy_eaten = false;
    gs->caddy_eaten_timer = 0;
    gs->raver_passed_out_timer = 0;
    gs->build_category = BUILD_CAT_TERRAIN;
    gs->current_tool = TOOL_FAIRWAY;
    gs->brush_size = 1;
    gs->club_funds = 150000; /* $150,000 SimGolf initial budget */
    
    golf_generate_terrain(gs, 0x51A601F);
    golf_place_default_routing(gs);
    
    /* Swing Gauge defaults */
    gs->swing.state = SWING_STATE_READY;
    gs->swing.cursor_pos = 0;
    gs->swing.cursor_speed = 850; /* Smooth 3-press swing timing */
    gs->swing.locked_power = FIXED_ONE;
    gs->swing.locked_accuracy = 0;
    gs->swing.moving_up = true;
    gs->swing.sweetspot_center = FIXED_ONE / 8;
    gs->swing.sweetspot_width  = 3200;
    
    /* Wind system */
    gs->wind.angle = 64; /* East-South-East */
    gs->wind.speed = 12; /* 12 mph gentle breeze */
    
    gs->current_club = CLUB_DRV;
    gs->strokes = 0;
    gs->score_rel_par = 0;
    gs->hole_completed = false;
    
    /* Caddy initialization */
    gs->caddy.x = gs->golfer.x - TO_FIXED(2);
    gs->caddy.y = gs->golfer.y;
    gs->caddy.current_z = gs->golfer.current_z;
    gs->caddy.state = CADDY_STATE_READY;
    gs->caddy.facing_angle = gs->golfer.facing_angle;
    
    golf_recalc_course_rating(gs);
    set_banner(gs, "WELCOME TO DUFFER PRO!", 180);
}

void golf_reset_ball_to_tee(GolfState *gs) {
    if (!gs->has_tee) return;
    
    gs->ball.x = TO_FIXED(gs->tee_gx) + FIXED_HALF;
    gs->ball.y = TO_FIXED(gs->tee_gy) + FIXED_HALF;
    gs->ball.ground_z = golf_get_ground_z_fixed(gs, gs->ball.x, gs->ball.y);
    gs->ball.z = gs->ball.ground_z;
    gs->ball.vx = 0;
    gs->ball.vy = 0;
    gs->ball.vz = 0;
    gs->ball.state = BALL_IDLE;
    gs->ball.current_surf = SURF_TEE;
    gs->ball.air_ticks = 0;
    gs->ball.bounce_count = 0;
    
    gs->prev_ball_gx = gs->tee_gx;
    gs->prev_ball_gy = gs->tee_gy;
    gs->shot_origin_gx = -1;
    gs->shot_origin_gy = -1;
    
    /* Setup Golfer */
    gs->golfer.x = gs->ball.x;
    gs->golfer.y = gs->ball.y;
    gs->golfer.current_z = gs->ball.ground_z;
    gs->golfer.target_x = gs->ball.x;
    gs->golfer.target_y = gs->ball.y;
    gs->golfer.state = GOLFER_STATE_READY;
    gs->golfer.walk_ticks = 0;
    gs->golfer.walk_anim_frame = 0;
    gs->golfer.step_timer = 0;
    
    /* Setup Caddy */
    gs->caddy.x = gs->golfer.x - TO_FIXED(2);
    gs->caddy.y = gs->golfer.y;
    gs->caddy.current_z = gs->golfer.current_z;
    gs->caddy.state = CADDY_STATE_READY;
    
    /* Aim toward cup */
    if (gs->has_pin) {
        gs->aim_target_x = TO_FIXED(gs->pin_gx) + FIXED_HALF;
        gs->aim_target_y = TO_FIXED(gs->pin_gy) + FIXED_HALF;
        fixed dx = gs->aim_target_x - gs->ball.x;
        fixed dy = gs->aim_target_y - gs->ball.y;
        gs->aim_angle = fixed_atan2(dy, dx);
        fixed dist_tiles = fixed_sqrt(FMUL(dx, dx) + FMUL(dy, dy));
        gs->aim_distance_yds = FROM_FIXED(FMUL(dist_tiles, TO_FIXED(YARDS_PER_TILE)));
        gs->has_custom_aim = false;
        gs->golfer.facing_angle = gs->aim_angle;
        gs->caddy.facing_angle = gs->aim_angle;
    }
    
    /* Club recommendation */
    int32_t yds = golf_distance_to_pin_yards(gs);
    gs->current_club = recommend_club_for_distance(yds, SURF_TEE);
    
    gs->swing.state = SWING_STATE_READY;
    gs->swing.cursor_pos = gs->swing.sweetspot_center;
    gs->strokes = 0;
    gs->score_rel_par = 0;
    gs->hole_completed = false;
    
    if (!gs->bev_cart.active) {
        gs->bev_cart.active = true;
        gs->bev_cart.x = TO_FIXED(gs->tee_gx - 3) + FIXED_HALF;
        gs->bev_cart.y = TO_FIXED(gs->tee_gy + 2) + FIXED_HALF;
        gs->bev_cart.current_z = golf_get_ground_z_fixed(gs, gs->bev_cart.x, gs->bev_cart.y);
        gs->bev_cart.target_x = TO_FIXED(gs->tee_gx) + FIXED_HALF;
        gs->bev_cart.target_y = TO_FIXED(gs->tee_gy - 8) + FIXED_HALF;
        gs->bev_cart.state = BEV_STATE_DRIVING;
        gs->bev_cart.facing_angle = 192;
        gs->bev_cart.speed = 2200;
        gs->bev_cart.whistle_timer = 200 + (core_rand() % 300);
        gs->bev_cart.speech_timer = 0;
        gs->bev_cart.wait_timer = 0;
    }
}

void golf_set_mode(GolfState *gs, GameMode mode) {
    gs->mode = mode;
    if (mode == MODE_PLAY) {
        set_banner(gs, "PLAY MODE - CLICK BALL TO SWING", 120);
    } else if (mode == MODE_BUILD) {
        set_banner(gs, "DESIGN MODE - SCULPT & BEAUTIFY COURSE", 120);
    }
}

void golf_set_tool(GolfState *gs, BuildTool tool) {
    if (tool < TOOL_COUNT) {
        gs->current_tool = tool;
    }
}

void golf_set_build_category(GolfState *gs, BuildCategory cat) {
    if (cat < BUILD_CAT_COUNT) {
        gs->build_category = cat;
        if (cat == BUILD_CAT_TERRAIN) gs->current_tool = TOOL_FAIRWAY;
        else if (cat == BUILD_CAT_COURSE) gs->current_tool = TOOL_TEE;
        else if (cat == BUILD_CAT_SCENERY) gs->current_tool = TOOL_PINE;
    }
}

bool golf_raise_terrain(GolfState *gs, int32_t gx, int32_t gy, int32_t brush_size) {
    int rad = (brush_size == 2) ? 1 : 0;
    bool changed = false;
    
    for (int dy = -rad; dy <= rad; ++dy) {
        for (int dx = -rad; dx <= rad; ++dx) {
            int tx = gx + dx;
            int ty = gy + dy;
            if (tx >= 0 && tx < GRID_W && ty >= 0 && ty < GRID_H) {
                if (gs->grid[tx][ty].elevation < MAX_ELEVATION_TIER) {
                    gs->grid[tx][ty].elevation++;
                    if (gs->grid[tx][ty].surface == SURF_WATER) {
                        gs->grid[tx][ty].surface = SURF_ROUGH;
                    }
                    changed = true;
                }
            }
        }
    }
    
    if (changed) {
        gs->audio_event = AUDIO_EVT_TERRAIN_SCULPT;
        gs->club_funds -= 50;
        golf_update_autotile_masks(gs);
        golf_recalc_course_rating(gs);
        set_banner(gs, "TERRAIN ELEVATION RAISED", 60);
    }
    return changed;
}

bool golf_lower_terrain(GolfState *gs, int32_t gx, int32_t gy, int32_t brush_size) {
    int rad = (brush_size == 2) ? 1 : 0;
    bool changed = false;
    
    for (int dy = -rad; dy <= rad; ++dy) {
        for (int dx = -rad; dx <= rad; ++dx) {
            int tx = gx + dx;
            int ty = gy + dy;
            if (tx >= 0 && tx < GRID_W && ty >= 0 && ty < GRID_H) {
                if (gs->grid[tx][ty].elevation > 0) {
                    gs->grid[tx][ty].elevation--;
                    if (gs->grid[tx][ty].elevation == 0) {
                        gs->grid[tx][ty].surface = SURF_WATER;
                        gs->grid[tx][ty].feature = FEAT_NONE;
                    }
                    changed = true;
                }
            }
        }
    }
    
    if (changed) {
        gs->audio_event = AUDIO_EVT_TERRAIN_SCULPT;
        gs->club_funds -= 50;
        golf_update_autotile_masks(gs);
        golf_recalc_course_rating(gs);
        set_banner(gs, "TERRAIN ELEVATION LOWERED", 60);
    }
    return changed;
}

bool golf_build_apply_tool(GolfState *gs, int32_t gx, int32_t gy) {
    if (gx < 0 || gx >= GRID_W || gy < 0 || gy >= GRID_H) return false;
    
    int rad = (gs->brush_size == 2) ? 1 : 0;
    bool modified = false;
    
    switch (gs->current_tool) {
        case TOOL_RAISE:
            return golf_raise_terrain(gs, gx, gy, gs->brush_size);
            
        case TOOL_LOWER:
            return golf_lower_terrain(gs, gx, gy, gs->brush_size);
            
        case TOOL_FAIRWAY:
        case TOOL_GREEN:
        case TOOL_SAND:
        case TOOL_ROUGH:
        case TOOL_WATER:
        case TOOL_CARTPATH:
        case TOOL_BRIDGE: {
            SurfaceType st = SURF_ROUGH;
            if (gs->current_tool == TOOL_FAIRWAY) st = SURF_FAIRWAY;
            else if (gs->current_tool == TOOL_GREEN) st = SURF_GREEN;
            else if (gs->current_tool == TOOL_SAND) st = SURF_SAND;
            else if (gs->current_tool == TOOL_WATER) st = SURF_WATER;
            else if (gs->current_tool == TOOL_CARTPATH) st = SURF_CARTPATH;
            else if (gs->current_tool == TOOL_BRIDGE) st = SURF_BRIDGE;
            
            for (int dy = -rad; dy <= rad; ++dy) {
                for (int dx = -rad; dx <= rad; ++dx) {
                    int tx = gx + dx;
                    int ty = gy + dy;
                    if (tx >= 0 && tx < GRID_W && ty >= 0 && ty < GRID_H) {
                        gs->grid[tx][ty].surface = st;
                        if (st == SURF_WATER) {
                            gs->grid[tx][ty].elevation = 0;
                            gs->grid[tx][ty].feature = FEAT_NONE;
                        } else if (gs->grid[tx][ty].elevation == 0) {
                            gs->grid[tx][ty].elevation = ELEV_LOWLAND;
                        }
                        modified = true;
                    }
                }
            }
            gs->audio_event = AUDIO_EVT_BUILD_PLACE;
            break;
        }
        
        case TOOL_BULLDOZE: {
            for (int dy = -rad; dy <= rad; ++dy) {
                for (int dx = -rad; dx <= rad; ++dx) {
                    int tx = gx + dx;
                    int ty = gy + dy;
                    if (tx >= 0 && tx < GRID_W && ty >= 0 && ty < GRID_H) {
                        if (gs->grid[tx][ty].feature == FEAT_CLUBHOUSE) {
                            golf_remove_clubhouse(gs);
                        }
                        gs->grid[tx][ty].feature = FEAT_NONE;
                        if (gs->grid[tx][ty].surface != SURF_WATER) {
                            gs->grid[tx][ty].surface = SURF_ROUGH;
                        }
                        modified = true;
                    }
                }
            }
            gs->audio_event = AUDIO_EVT_BUILD_PLACE;
            break;
        }
        
        case TOOL_TEE: {
            gs->tee_gx = gx;
            gs->tee_gy = gy;
            gs->has_tee = true;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int tx = gx + dx;
                    int ty = gy + dy;
                    if (tx >= 0 && tx < GRID_W && ty >= 0 && ty < GRID_H) {
                        if (gs->grid[tx][ty].elevation == 0) gs->grid[tx][ty].elevation = ELEV_LOWLAND;
                        gs->grid[tx][ty].surface = SURF_TEE;
                        gs->grid[tx][ty].feature = FEAT_NONE;
                    }
                }
            }
            golf_recalc_hole_stats(gs);
            golf_reset_ball_to_tee(gs);
            gs->audio_event = AUDIO_EVT_BUILD_PLACE;
            modified = true;
            break;
        }
        
        case TOOL_PIN: {
            gs->pin_gx = gx;
            gs->pin_gy = gy;
            gs->has_pin = true;
            for (int dy = -2; dy <= 2; ++dy) {
                for (int dx = -2; dx <= 2; ++dx) {
                    int tx = gx + dx;
                    int ty = gy + dy;
                    if (tx >= 0 && tx < GRID_W && ty >= 0 && ty < GRID_H) {
                        if (gs->grid[tx][ty].elevation == 0) gs->grid[tx][ty].elevation = ELEV_VALLEY;
                        gs->grid[tx][ty].surface = SURF_GREEN;
                        gs->grid[tx][ty].feature = FEAT_NONE;
                    }
                }
            }
            golf_recalc_hole_stats(gs);
            gs->audio_event = AUDIO_EVT_BUILD_PLACE;
            modified = true;
            break;
        }
        
        case TOOL_PINE:
        case TOOL_OAK:
        case TOOL_PALM:
        case TOOL_FLOWERING:
        case TOOL_BUSH:
        case TOOL_FLOWERS:
        case TOOL_ROCK:
        case TOOL_BENCH:
        case TOOL_CART: {
            FeatureType ft = FEAT_PINE;
            if (gs->current_tool == TOOL_OAK) ft = FEAT_OAK;
            else if (gs->current_tool == TOOL_PALM) ft = FEAT_PALM;
            else if (gs->current_tool == TOOL_FLOWERING) ft = FEAT_FLOWERING;
            else if (gs->current_tool == TOOL_BUSH) ft = FEAT_BUSH;
            else if (gs->current_tool == TOOL_FLOWERS) ft = FEAT_FLOWERS;
            else if (gs->current_tool == TOOL_ROCK) ft = FEAT_ROCK;
            else if (gs->current_tool == TOOL_BENCH) ft = FEAT_BENCH;
            else if (gs->current_tool == TOOL_CART) ft = FEAT_CART;
            
            if (gs->grid[gx][gy].surface != SURF_WATER && gs->grid[gx][gy].surface != SURF_TEE && gs->grid[gx][gy].surface != SURF_GREEN) {
                gs->grid[gx][gy].feature = ft;
                gs->audio_event = AUDIO_EVT_BUILD_PLACE;
                gs->club_funds -= 25;
                modified = true;
            }
            break;
        }
        
        case TOOL_CLUBHOUSE: {
            if (golf_place_clubhouse(gs, gx, gy)) {
                gs->audio_event = AUDIO_EVT_BUILD_PLACE;
                gs->club_funds -= 1500;
                modified = true;
            }
            break;
        }
        
        default:
            break;
    }
    
    if (modified) {
        golf_update_autotile_masks(gs);
        golf_recalc_course_rating(gs);
    }
    return modified;
}

void golf_select_club(GolfState *gs, ClubType club) {
    if (club < CLUB_COUNT) {
        gs->current_club = club;
        if (!gs->has_custom_aim) {
            const ClubData *cd = golf_get_club_data(gs->current_club);
            gs->aim_distance_yds = cd->nominal_yards;
            fixed dist_tiles = FDIV(TO_FIXED(gs->aim_distance_yds), TO_FIXED(YARDS_PER_TILE));
            gs->aim_target_x = gs->ball.x + FMUL(dist_tiles, fixed_cos(gs->aim_angle));
            gs->aim_target_y = gs->ball.y + FMUL(dist_tiles, fixed_sin(gs->aim_angle));
        }
    }
}

void golf_change_club(GolfState *gs, int delta) {
    int next = (int)gs->current_club + delta;
    if (next < 0) next = CLUB_COUNT - 1;
    if (next >= CLUB_COUNT) next = 0;
    golf_select_club(gs, (ClubType)next);
}

void golf_adjust_aim(GolfState *gs, int delta_angle) {
    gs->aim_angle = (uint8_t)(gs->aim_angle + delta_angle);
    gs->golfer.facing_angle = gs->aim_angle;
    gs->has_custom_aim = false;
    
    const ClubData *club = golf_get_club_data(gs->current_club);
    gs->aim_distance_yds = club->nominal_yards;
    fixed dist_tiles = FDIV(TO_FIXED(gs->aim_distance_yds), TO_FIXED(YARDS_PER_TILE));
    gs->aim_target_x = gs->ball.x + FMUL(dist_tiles, fixed_cos(gs->aim_angle));
    gs->aim_target_y = gs->ball.y + FMUL(dist_tiles, fixed_sin(gs->aim_angle));
}

void golf_set_aim_target(GolfState *gs, fixed target_fx, fixed target_fy) {
    gs->aim_target_x = target_fx;
    gs->aim_target_y = target_fy;
    gs->has_custom_aim = true;
    
    fixed dx = target_fx - gs->ball.x;
    fixed dy = target_fy - gs->ball.y;
    gs->aim_angle = fixed_atan2(dy, dx);
    gs->golfer.facing_angle = gs->aim_angle;
    
    fixed dist_tiles = fixed_sqrt(FMUL(dx, dx) + FMUL(dy, dy));
    gs->aim_distance_yds = FROM_FIXED(FMUL(dist_tiles, TO_FIXED(YARDS_PER_TILE)));
    if (gs->aim_distance_yds < 1) gs->aim_distance_yds = 1;
    
    /* Auto-select optimal club */
    gs->current_club = recommend_club_for_distance(gs->aim_distance_yds, gs->ball.current_surf);
}

void golf_execute_shot(GolfState *gs) {
    if (gs->mode != MODE_PLAY) return;
    
    if (gs->golfer.state == GOLFER_STATE_WALKING) {
        golf_golfer_arrive_at_ball(gs);
        return;
    }
    
    if (gs->ball.state != BALL_IDLE) return;
    
    const ClubData *club = golf_get_club_data(gs->current_club);
    gs->strokes++;
    gs->prev_ball_gx = FROM_FIXED(gs->ball.x);
    gs->prev_ball_gy = FROM_FIXED(gs->ball.y);
    gs->shot_origin_gx = gs->prev_ball_gx;
    gs->shot_origin_gy = gs->prev_ball_gy;
    
    /* Club strike audio */
    if (club->is_putter) {
        gs->audio_event = AUDIO_EVT_SWING_PUTTER;
    } else if (gs->current_club == CLUB_DRV || gs->current_club == CLUB_1W) {
        gs->audio_event = AUDIO_EVT_SWING_DRIVER;
    } else if (gs->current_club >= CLUB_PW && gs->current_club <= CLUB_LW) {
        gs->audio_event = AUDIO_EVT_SWING_WEDGE;
    } else {
        gs->audio_event = AUDIO_EVT_SWING_IRON;
    }
    
    gs->golfer.state = GOLFER_STATE_WATCHING;
    gs->swing.state = SWING_STATE_RESOLVED;
    
    fixed pwr = FIXED_ONE;
    if (gs->ball.current_surf == SURF_ROUGH) {
        pwr = FMUL(pwr, 52428); /* 80% from rough */
    } else if (gs->ball.current_surf == SURF_SAND) {
        pwr = FMUL(pwr, 36044); /* 55% from sand */
    }
    
    fixed dist_scale = FIXED_ONE;
    fixed root_scale = FIXED_ONE;
    if (club->nominal_yards > 0 && gs->aim_distance_yds > 0) {
        if (gs->aim_distance_yds < club->nominal_yards) {
            dist_scale = FDIV(TO_FIXED(gs->aim_distance_yds), TO_FIXED(club->nominal_yards));
            if (dist_scale < TO_FIXED(1) / 5) dist_scale = TO_FIXED(1) / 5;
            root_scale = fixed_sqrt(dist_scale);
        }
    }
    
    uint8_t final_angle = gs->aim_angle;
    
    if (club->is_putter) {
        fixed target_yds = (gs->aim_distance_yds > 0) ? gs->aim_distance_yds : 10;
        if (target_yds > 25) target_yds = 25;
        fixed roll_speed = FMUL(target_yds * 385, pwr);
        if (roll_speed < 1200) roll_speed = 1200;
        gs->ball.vx = FMUL(roll_speed, fixed_cos(final_angle));
        gs->ball.vy = FMUL(roll_speed, fixed_sin(final_angle));
        gs->ball.vz = 0;
        gs->ball.state = BALL_ROLLING;
    } else {
        fixed horiz_spd = FMUL(FMUL(club->launch_speed_h, pwr), root_scale);
        fixed vert_spd  = FMUL(FMUL(club->launch_speed_v, pwr), root_scale);
        
        gs->ball.vx = FMUL(horiz_spd, fixed_cos(final_angle));
        gs->ball.vy = FMUL(horiz_spd, fixed_sin(final_angle));
        gs->ball.vz = vert_spd;
        gs->ball.state = BALL_IN_FLIGHT;
        gs->ball.air_ticks = 0;
        gs->ball.bounce_count = 0;
    }
    
    set_banner(gs, "BALL STRUCK! NICE SHOT!", 90);
}

void golf_swing_click(GolfState *gs) {
    if (gs->mode != MODE_PLAY) return;
    
    /* If golfer is currently walking to the ball, click/space immediately arrives */
    if (gs->golfer.state == GOLFER_STATE_WALKING) {
        golf_golfer_arrive_at_ball(gs);
        return;
    }
    
    if (gs->ball.state != BALL_IDLE) return;
    
    switch (gs->swing.state) {
        case SWING_STATE_READY:
            /* Press 1: Start power gauge oscillation toward 100% */
            gs->swing.state = SWING_STATE_POWER;
            gs->swing.cursor_pos = 0;
            gs->swing.moving_up = true;
            gs->golfer.state = GOLFER_STATE_SWINGING;
            break;
            
        case SWING_STATE_POWER:
            /* Press 2: Lock shot power and reverse toward sweetspot */
            gs->swing.locked_power = gs->swing.cursor_pos;
            if (gs->swing.locked_power < TO_FIXED(1) / 10) {
                gs->swing.locked_power = TO_FIXED(1) / 10;
            }
            gs->swing.state = SWING_STATE_ACCURACY;
            gs->swing.moving_up = false;
            break;
            
        case SWING_STATE_ACCURACY:
            /* Press 2 (if auto-bounced at 100%) or Press 3: Lock accuracy & strike ball */
            gs->swing.locked_accuracy = gs->swing.cursor_pos - gs->swing.sweetspot_center;
            gs->swing.state = SWING_STATE_RESOLVED;
            gs->golfer.state = GOLFER_STATE_WATCHING;
            
            {
                const ClubData *club = golf_get_club_data(gs->current_club);
                gs->strokes++;
                gs->prev_ball_gx = FROM_FIXED(gs->ball.x);
                gs->prev_ball_gy = FROM_FIXED(gs->ball.y);
                gs->shot_origin_gx = gs->prev_ball_gx;
                gs->shot_origin_gy = gs->prev_ball_gy;
                
                /* Trigger club audio */
                if (club->is_putter) {
                    gs->audio_event = AUDIO_EVT_SWING_PUTTER;
                } else if (gs->current_club == CLUB_1W) {
                    gs->audio_event = AUDIO_EVT_SWING_DRIVER;
                } else if (gs->current_club == CLUB_PW) {
                    gs->audio_event = AUDIO_EVT_SWING_WEDGE;
                } else {
                    gs->audio_event = AUDIO_EVT_SWING_IRON;
                }
                
                fixed pwr = gs->swing.locked_power;
                fixed acc = gs->swing.locked_accuracy;
                
                if (gs->ball.current_surf == SURF_ROUGH) {
                    pwr = FMUL(pwr, 52428); /* 80% from rough */
                } else if (gs->ball.current_surf == SURF_SAND) {
                    pwr = FMUL(pwr, 36044); /* 55% from sand */
                }
                
                fixed dist_scale = FIXED_ONE;
                fixed root_scale = FIXED_ONE;
                if (club->nominal_yards > 0 && gs->aim_distance_yds > 0) {
                    if (gs->aim_distance_yds < club->nominal_yards) {
                        dist_scale = FDIV(TO_FIXED(gs->aim_distance_yds), TO_FIXED(club->nominal_yards));
                        if (dist_scale < TO_FIXED(1) / 5) dist_scale = TO_FIXED(1) / 5;
                        root_scale = fixed_sqrt(dist_scale);
                    }
                }
                
                int32_t angle_dev = 0;
                if (FABS(acc) <= gs->swing.sweetspot_width) {
                    angle_dev = 0;
                } else {
                    fixed dev = (acc > 0) ? (acc - gs->swing.sweetspot_width) : (acc + gs->swing.sweetspot_width);
                    angle_dev = FROM_FIXED(FMUL(dev, TO_FIXED(10)));
                }
                uint8_t final_angle = (uint8_t)(gs->aim_angle + angle_dev);
                
                if (club->is_putter) {
                    /* Calibrated putter roll: 100% power rolls right to aim target */
                    fixed target_yds = (gs->aim_distance_yds > 0) ? gs->aim_distance_yds : 10;
                    if (target_yds > 25) target_yds = 25;
                    fixed roll_speed = FMUL(target_yds * 385, pwr);
                    if (roll_speed < 1200) roll_speed = 1200;
                    gs->ball.vx = FMUL(roll_speed, fixed_cos(final_angle));
                    gs->ball.vy = FMUL(roll_speed, fixed_sin(final_angle));
                    gs->ball.vz = 0;
                    gs->ball.state = BALL_ROLLING;
                } else {
                    fixed horiz_spd = FMUL(FMUL(club->launch_speed_h, pwr), root_scale);
                    fixed vert_spd  = FMUL(FMUL(club->launch_speed_v, pwr), root_scale);
                    
                    gs->ball.vx = FMUL(horiz_spd, fixed_cos(final_angle));
                    gs->ball.vy = FMUL(horiz_spd, fixed_sin(final_angle));
                    gs->ball.vz = vert_spd;
                    gs->ball.state = BALL_IN_FLIGHT;
                    gs->ball.air_ticks = 0;
                    gs->ball.bounce_count = 0;
                }
                
                if (FABS(acc) <= gs->swing.sweetspot_width) {
                    set_banner(gs, "CRACK! SWEET SPOT STRIKE!", 90);
                } else if (acc < 0) {
                    set_banner(gs, "HOOK / DRAW!", 90);
                } else {
                    set_banner(gs, "SLICE / FADE!", 90);
                }
            }
            break;
            
        case SWING_STATE_RESOLVED:
            break;
    }
}

/* Check ball collision with active gopher or gopher holes */
static bool check_gopher_collision(GolfState *gs) {
    if (gs->mode != MODE_PLAY) return false;
    
    int hit_gx = -1;
    int hit_gy = -1;
    bool direct_gopher_hit = false;
    
    /* 1. Direct hit on visible Gopher */
    if (gs->gopher.visible) {
        fixed dx = gs->ball.x - gs->gopher.x;
        fixed dy = gs->ball.y - gs->gopher.y;
        fixed dist = fixed_sqrt(FMUL(dx, dx) + FMUL(dy, dy));
        if (dist < TO_FIXED(1) * 75 / 100) {
            hit_gx = gs->gopher.gx;
            hit_gy = gs->gopher.gy;
            direct_gopher_hit = true;
        }
    }
    
    /* 2. Ball rolling or landing into any active Gopher Hole */
    if (hit_gx == -1) {
        for (int i = 0; i < gs->gopher.hole_count; ++i) {
            if (!gs->gopher.holes[i].active) continue;
            fixed hx = TO_FIXED(gs->gopher.holes[i].gx) + FIXED_HALF;
            fixed hy = TO_FIXED(gs->gopher.holes[i].gy) + FIXED_HALF;
            fixed dx = gs->ball.x - hx;
            fixed dy = gs->ball.y - hy;
            fixed dist = fixed_sqrt(FMUL(dx, dx) + FMUL(dy, dy));
            if (dist < TO_FIXED(1) * 60 / 100) {
                hit_gx = gs->gopher.holes[i].gx;
                hit_gy = gs->gopher.holes[i].gy;
                break;
            }
        }
    }
    
    if (hit_gx != -1) {
        /* Ball vanishes into subterranean gopher burrow! */
        gs->ball.vx = 0;
        gs->ball.vy = 0;
        gs->ball.vz = 0;
        gs->audio_event = direct_gopher_hit ? AUDIO_EVT_GOPHER_CHUCKLE : AUDIO_EVT_GOPHER_GULP;
        
        /* Gopher pops up right at this burrow to dance and taunt! */
        gs->gopher.gx = hit_gx;
        gs->gopher.gy = hit_gy;
        gs->gopher.x = TO_FIXED(hit_gx) + FIXED_HALF;
        gs->gopher.y = TO_FIXED(hit_gy) + FIXED_HALF;
        gs->gopher.current_z = golf_get_ground_z_fixed(gs, gs->gopher.x, gs->gopher.y);
        gs->gopher.state = GOPHER_STATE_TAUNTING;
        gs->gopher.visible = true;
        gs->gopher.timer = 0;
        gs->gopher_encounter_count++;
        
        /* Drop ball at lip of burrow on safe ground */
        gs->ball.x = TO_FIXED(hit_gx) + FIXED_HALF + 24000;
        gs->ball.y = TO_FIXED(hit_gy) + FIXED_HALF + 24000;
        gs->ball.ground_z = golf_get_ground_z_fixed(gs, gs->ball.x, gs->ball.y);
        gs->ball.z = gs->ball.ground_z;
        gs->ball.state = BALL_IDLE;
        gs->ball.current_surf = SURF_ROUGH;
        
        if (direct_gopher_hit) {
            set_banner(gs, "DIRECT HIT ON GOPHER! Ball vanished down burrow lol", 220);
        } else {
            set_banner(gs, "GOPHER HOLE! Ball swallowed! Free relief at burrow lip lol", 220);
        }
        
        /* Golfer walks to drop location */
        fixed gdx = gs->ball.x - gs->golfer.x;
        fixed gdy = gs->ball.y - gs->golfer.y;
        fixed gdist = fixed_sqrt(FMUL(gdx, gdx) + FMUL(gdy, gdy));
        if (gdist > TO_FIXED(1) / 2) {
            gs->golfer.state = GOLFER_STATE_WALKING;
            gs->golfer.target_x = gs->ball.x;
            gs->golfer.target_y = gs->ball.y;
            gs->golfer.facing_angle = fixed_atan2(gdy, gdx);
            gs->golfer.walk_ticks = 0;
            gs->golfer.step_timer = 0;
            gs->caddy.state = CADDY_STATE_FOLLOWING;
        } else {
            golf_golfer_arrive_at_ball(gs);
        }
        
        gs->swing.state = SWING_STATE_READY;
        gs->swing.cursor_pos = gs->swing.sweetspot_center;
        
        if (gs->has_pin) {
            gs->aim_target_x = TO_FIXED(gs->pin_gx) + FIXED_HALF;
            gs->aim_target_y = TO_FIXED(gs->pin_gy) + FIXED_HALF;
            fixed adx = gs->aim_target_x - gs->ball.x;
            fixed ady = gs->aim_target_y - gs->ball.y;
            gs->aim_angle = fixed_atan2(ady, adx);
            fixed dist_tiles = fixed_sqrt(FMUL(adx, adx) + FMUL(ady, ady));
            gs->aim_distance_yds = FROM_FIXED(FMUL(dist_tiles, TO_FIXED(YARDS_PER_TILE)));
            gs->has_custom_aim = false;
        }
        return true;
    }
    return false;
}

/* Physics and Simulation Tick (60 Hz) */
void golf_tick(GolfState *gs) {
    if (gs->banner_timer > 0) {
        gs->banner_timer--;
    }
    
    /* 1. Update Swing Gauge */
    if (gs->mode == MODE_PLAY && gs->ball.state == BALL_IDLE && gs->golfer.state != GOLFER_STATE_WALKING) {
        if (gs->swing.state == SWING_STATE_POWER) {
            gs->swing.cursor_pos += gs->swing.cursor_speed;
            if (gs->swing.cursor_pos >= FIXED_ONE) {
                /* Far side auto-bounce: locks 100% power and reverses back down */
                gs->swing.cursor_pos = FIXED_ONE;
                gs->swing.locked_power = FIXED_ONE;
                gs->swing.state = SWING_STATE_ACCURACY;
                gs->swing.moving_up = false;
            }
        } else if (gs->swing.state == SWING_STATE_ACCURACY) {
            gs->swing.cursor_pos -= (gs->swing.cursor_speed * 11) / 10;
            if (gs->swing.cursor_pos <= 0) {
                /* Needle reached bottom without player press: NEVER auto-fire!
                   Reset swing meter so player can try again */
                gs->swing.cursor_pos = 0;
                gs->swing.state = SWING_STATE_READY;
                gs->golfer.state = GOLFER_STATE_READY;
                set_banner(gs, "SWING TIMED OUT - PRESS SPACE TO SWING", 120);
            }
        }
    }
    
    /* 2. Ball Simulation */
    if (gs->ball.state == BALL_IN_FLIGHT) {
        gs->ball.air_ticks++;
        gs->ball.vz -= GRAVITY_FIXED;
        
        fixed wx = FMUL(gs->wind.speed, fixed_cos(gs->wind.angle));
        fixed wy = FMUL(gs->wind.speed, fixed_sin(gs->wind.angle));
        gs->ball.vx += FMUL(wx, WIND_FACTOR_FIXED);
        gs->ball.vy += FMUL(wy, WIND_FACTOR_FIXED);
        
        gs->ball.vx = FMUL(gs->ball.vx, AIR_DRAG_FIXED);
        gs->ball.vy = FMUL(gs->ball.vy, AIR_DRAG_FIXED);
        
        gs->ball.x += gs->ball.vx;
        gs->ball.y += gs->ball.vy;
        gs->ball.z += gs->ball.vz;
        
        /* Boundary check */
        if (gs->ball.x < 0 || gs->ball.x >= TO_FIXED(GRID_W) ||
            gs->ball.y < 0 || gs->ball.y >= TO_FIXED(GRID_H)) {
            gs->ball.state = BALL_WATER;
            gs->strokes++;
            set_banner(gs, "OUT OF BOUNDS! (+1 PENALTY)", 120);
            return;
        }
        
        /* Tree / Object Collision */
        int32_t bgx = FROM_FIXED(gs->ball.x);
        int32_t bgy = FROM_FIXED(gs->ball.y);
        
        /* Only collide if ball strikes obstacle from outside the square where it was struck */
        bool is_launch_square = (bgx == gs->shot_origin_gx && bgy == gs->shot_origin_gy);
        
        if (!is_launch_square && bgx >= 0 && bgx < GRID_W && bgy >= 0 && bgy < GRID_H) {
            uint8_t feat = gs->grid[bgx][bgy].feature;
            fixed ground_z = TO_FIXED(gs->grid[bgx][bgy].elevation * HEIGHT_STEP_PX);
            
            if (feat == FEAT_PINE || feat == FEAT_OAK || feat == FEAT_PALM || feat == FEAT_FLOWERING || feat == FEAT_BUSH) {
                if (gs->ball.z >= ground_z + TO_FIXED(4) && gs->ball.z <= ground_z + TO_FIXED(36)) {
                    gs->ball.vz = -FABS(gs->ball.vz) / 2 - TO_FIXED(1);
                    gs->ball.vx = gs->ball.vx / 3;
                    gs->ball.vy = gs->ball.vy / 3;
                    gs->audio_event = AUDIO_EVT_TREE_HIT;
                    set_banner(gs, "THWACK! STRUCK TREE BRANCHES!", 90);
                }
            } else if (feat == FEAT_ROCK) {
                if (gs->ball.z <= ground_z + TO_FIXED(10)) {
                    gs->ball.vz = FABS(gs->ball.vz) + TO_FIXED(2);
                    gs->ball.vx = -gs->ball.vx / 2;
                    gs->ball.vy = -gs->ball.vy / 2;
                    gs->audio_event = AUDIO_EVT_ROCK_HIT;
                    set_banner(gs, "CLANG! STRUCK BOULDER!", 90);
                }
            }
        }
        
        fixed gz = golf_get_ground_z_fixed(gs, gs->ball.x, gs->ball.y);
        gs->ball.ground_z = gz;
        
        /* Ascending launch clearance: allow ascending ball to clear terrace lips without getting stuck behind a tile */
        if (gs->ball.air_ticks < 30 && gs->ball.vz > 0 && gs->ball.z < gz) {
            if (gz - gs->ball.z <= TO_FIXED(HEIGHT_STEP_PX * 3)) {
                gs->ball.z = gz + TO_FIXED(1);
            }
        }
        
        if (gs->ball.z <= gz) {
            gs->ball.z = gz;
            
            SurfaceType surf = golf_get_surface_at(gs, gs->ball.x, gs->ball.y);
            if (surf == SURF_WATER) {
                gs->ball.state = BALL_WATER;
                gs->strokes++;
                gs->audio_event = AUDIO_EVT_WATER_SPLASH;
                gs->caddy.state = CADDY_STATE_DISAPPOINTED;
                set_banner(gs, "SPLASH! WATER HAZARD (+1 PENALTY)", 120);
                return;
            }
            
            /* Direct dunk / cup capture check */
            if (gs->has_pin) {
                fixed pin_x = TO_FIXED(gs->pin_gx) + FIXED_HALF;
                fixed pin_y = TO_FIXED(gs->pin_gy) + FIXED_HALF;
                fixed dx = pin_x - gs->ball.x;
                fixed dy = pin_y - gs->ball.y;
                fixed dist = fixed_sqrt(FMUL(dx, dx) + FMUL(dy, dy));
                if (dist < CUP_RADIUS_FIXED) {
                    fixed speed = fixed_sqrt(FMUL(gs->ball.vx, gs->ball.vx) + FMUL(gs->ball.vy, gs->ball.vy));
                    if (speed < CUP_MAX_SPEED) {
                        gs->ball.x = pin_x;
                        gs->ball.y = pin_y;
                        gs->ball.vx = 0;
                        gs->ball.vy = 0;
                        gs->ball.vz = 0;
                        gs->ball.state = BALL_HOLED;
                        gs->hole_completed = true;
                        gs->audio_event = AUDIO_EVT_CUP_SINK;
                        gs->caddy.state = CADDY_STATE_CELEBRATING;
                        
                        int32_t rel = gs->strokes - gs->par;
                        if (gs->strokes == 1) {
                            set_banner(gs, "*** HOLE IN ONE! LEGENDARY! ***", 300);
                            gs->club_funds += 25000;
                        } else if (rel <= -2) {
                            set_banner(gs, "*** EAGLE! MAGNIFICENT! ***", 300);
                            gs->club_funds += 10000;
                        } else if (rel == -1) {
                            set_banner(gs, "** BIRDIE! EXCELLENT SHOT! **", 300);
                            gs->club_funds += 5000;
                        } else if (rel == 0) {
                            set_banner(gs, "* PAR! SOLID GOLF! *", 240);
                            gs->club_funds += 1000;
                        } else {
                            set_banner(gs, "HOLED OUT!", 200);
                        }
                        return;
                    }
                }
            }
            
            /* Check if ball landed on gopher or into gopher hole */
            if (check_gopher_collision(gs)) return;
            
            /* Ground Restitution & Friction */
            fixed rest = 26214;   /* ~0.40 Fairway */
            fixed fric = 60293;   /* ~0.92 Fairway */
            
            if (surf == SURF_GREEN) {
                rest = 32768; fric = 62914;
            } else if (surf == SURF_ROUGH) {
                rest = 9830;  fric = 52428;
            } else if (surf == SURF_SAND) {
                rest = 3276;  fric = 36044;
            } else if (surf == SURF_BRIDGE || surf == SURF_CARTPATH) {
                rest = 28000; fric = 62000;
            }
            
            gs->ball.vz = -FMUL(gs->ball.vz, rest);
            gs->ball.vx = FMUL(gs->ball.vx, fric);
            gs->ball.vy = FMUL(gs->ball.vy, fric);
            gs->ball.bounce_count++;
            if (gs->ball.bounce_count <= 2) {
                gs->audio_event = AUDIO_EVT_BALL_BOUNCE;
            }
            
            if (FABS(gs->ball.vz) < TO_FIXED(1) / 2 || gs->ball.bounce_count > 4) {
                gs->ball.vz = 0;
                gs->ball.state = BALL_ROLLING;
            }
        }
    } else if (gs->ball.state == BALL_ROLLING) {
        SurfaceType surf = golf_get_surface_at(gs, gs->ball.x, gs->ball.y);
        if (surf == SURF_WATER) {
            gs->ball.state = BALL_WATER;
            gs->strokes++;
            gs->audio_event = AUDIO_EVT_WATER_SPLASH;
            gs->caddy.state = CADDY_STATE_DISAPPOINTED;
            set_banner(gs, "SPLASH! ROLLED INTO WATER (+1 PENALTY)", 120);
            return;
        }
        
        fixed fric = 60293;
        if (surf == SURF_GREEN) fric = 63500;
        else if (surf == SURF_ROUGH) fric = 50000;
        else if (surf == SURF_SAND) fric = 32000;
        else if (surf == SURF_BRIDGE || surf == SURF_CARTPATH) fric = 62000;
        
        gs->ball.vx = FMUL(gs->ball.vx, fric);
        gs->ball.vy = FMUL(gs->ball.vy, fric);
        
        /* Slope vector acceleration */
        int32_t bgx = FROM_FIXED(gs->ball.x);
        int32_t bgy = FROM_FIXED(gs->ball.y);
        if (bgx > 0 && bgx < GRID_W - 1 && bgy > 0 && bgy < GRID_H - 1) {
            int32_t de_x = gs->grid[bgx + 1][bgy].elevation - gs->grid[bgx - 1][bgy].elevation;
            int32_t de_y = gs->grid[bgx][bgy + 1].elevation - gs->grid[bgx][bgy - 1].elevation;
            gs->ball.vx -= de_x * SLOPE_ACCEL_FIXED;
            gs->ball.vy -= de_y * SLOPE_ACCEL_FIXED;
        }
        
        gs->ball.x += gs->ball.vx;
        gs->ball.y += gs->ball.vy;
        
        /* Tree / Boulder Collision during roll */
        int32_t r_bgx = FROM_FIXED(gs->ball.x);
        int32_t r_bgy = FROM_FIXED(gs->ball.y);
        bool is_launch_square = (r_bgx == gs->shot_origin_gx && r_bgy == gs->shot_origin_gy);
        if (!is_launch_square && r_bgx >= 0 && r_bgx < GRID_W && r_bgy >= 0 && r_bgy < GRID_H) {
            uint8_t feat = gs->grid[r_bgx][r_bgy].feature;
            if (feat == FEAT_PINE || feat == FEAT_OAK || feat == FEAT_PALM || feat == FEAT_FLOWERING || feat == FEAT_BUSH) {
                gs->ball.vx = gs->ball.vx / 3;
                gs->ball.vy = gs->ball.vy / 3;
                gs->audio_event = AUDIO_EVT_TREE_HIT;
                set_banner(gs, "THWACK! STRUCK TREE TRUNK!", 90);
            } else if (feat == FEAT_ROCK) {
                gs->ball.vx = -gs->ball.vx / 2;
                gs->ball.vy = -gs->ball.vy / 2;
                gs->audio_event = AUDIO_EVT_ROCK_HIT;
                set_banner(gs, "CLANG! STRUCK BOULDER!", 90);
            }
        }
        
        /* Check if ball rolled into gopher hole or hit gopher */
        if (check_gopher_collision(gs)) return;
        
        if (gs->ball.x < 0 || gs->ball.x >= TO_FIXED(GRID_W) ||
            gs->ball.y < 0 || gs->ball.y >= TO_FIXED(GRID_H)) {
            gs->ball.state = BALL_WATER;
            gs->strokes++;
            set_banner(gs, "OUT OF BOUNDS! (+1 PENALTY)", 120);
            return;
        }
        
        gs->ball.ground_z = golf_get_ground_z_fixed(gs, gs->ball.x, gs->ball.y);
        gs->ball.z = gs->ball.ground_z;
        
        /* Cup capture */
        if (gs->has_pin) {
            fixed pin_x = TO_FIXED(gs->pin_gx) + FIXED_HALF;
            fixed pin_y = TO_FIXED(gs->pin_gy) + FIXED_HALF;
            fixed dx = pin_x - gs->ball.x;
            fixed dy = pin_y - gs->ball.y;
            fixed dist = fixed_sqrt(FMUL(dx, dx) + FMUL(dy, dy));
            
            if (dist < CUP_RADIUS_FIXED) {
                fixed speed = fixed_sqrt(FMUL(gs->ball.vx, gs->ball.vx) + FMUL(gs->ball.vy, gs->ball.vy));
                if (speed < CUP_MAX_SPEED) {
                    gs->ball.x = pin_x;
                    gs->ball.y = pin_y;
                    gs->ball.vx = 0;
                    gs->ball.vy = 0;
                    gs->ball.state = BALL_HOLED;
                    gs->hole_completed = true;
                    gs->audio_event = AUDIO_EVT_CUP_SINK;
                    gs->caddy.state = CADDY_STATE_CELEBRATING;
                    
                    int32_t rel = gs->strokes - gs->par;
                    if (gs->strokes == 1) {
                        set_banner(gs, "*** HOLE IN ONE! LEGENDARY! ***", 300);
                        gs->club_funds += 25000;
                    } else if (rel <= -2) {
                        set_banner(gs, "*** EAGLE! MAGNIFICENT! ***", 300);
                        gs->club_funds += 10000;
                    } else if (rel == -1) {
                        set_banner(gs, "** BIRDIE! EXCELLENT SHOT! **", 300);
                        gs->club_funds += 5000;
                    } else if (rel == 0) {
                        set_banner(gs, "* PAR! SOLID GOLF! *", 240);
                        gs->club_funds += 1000;
                    } else {
                        set_banner(gs, "HOLED OUT!", 200);
                    }
                    return;
                } else {
                    set_banner(gs, "LIPPED OUT!", 60);
                }
            }
        }
        
        /* Stop threshold check */
        fixed speed = fixed_sqrt(FMUL(gs->ball.vx, gs->ball.vx) + FMUL(gs->ball.vy, gs->ball.vy));
        if (speed < STOP_SPEED_THRESHOLD) {
            gs->ball.vx = 0;
            gs->ball.vy = 0;
            gs->ball.state = BALL_IDLE;
            gs->ball.current_surf = golf_get_surface_at(gs, gs->ball.x, gs->ball.y);
            
            /* Check gallery applause if ball settled close to the cup */
            if (gs->has_pin) {
                fixed pdx = (TO_FIXED(gs->pin_gx) + FIXED_HALF) - gs->ball.x;
                fixed pdy = (TO_FIXED(gs->pin_gy) + FIXED_HALF) - gs->ball.y;
                fixed pdist = fixed_sqrt(FMUL(pdx, pdx) + FMUL(pdy, pdy));
                if (pdist < TO_FIXED(3)) {
                    gs->audio_event = AUDIO_EVT_APPLAUSE_GOLF;
                }
            }
            
            /* Start golfer walking to ball */
            fixed gdx = gs->ball.x - gs->golfer.x;
            fixed gdy = gs->ball.y - gs->golfer.y;
            fixed gdist = fixed_sqrt(FMUL(gdx, gdx) + FMUL(gdy, gdy));
            if (gdist > TO_FIXED(1) / 2) {
                gs->golfer.state = GOLFER_STATE_WALKING;
                gs->golfer.target_x = gs->ball.x;
                gs->golfer.target_y = gs->ball.y;
                gs->golfer.facing_angle = fixed_atan2(gdy, gdx);
                gs->golfer.walk_ticks = 0;
                gs->golfer.step_timer = 0;
                gs->caddy.state = CADDY_STATE_FOLLOWING;
            } else {
                golf_golfer_arrive_at_ball(gs);
            }
            
            /* Aim toward pin */
            if (gs->has_pin) {
                gs->aim_target_x = TO_FIXED(gs->pin_gx) + FIXED_HALF;
                gs->aim_target_y = TO_FIXED(gs->pin_gy) + FIXED_HALF;
                fixed dx = gs->aim_target_x - gs->ball.x;
                fixed dy = gs->aim_target_y - gs->ball.y;
                gs->aim_angle = fixed_atan2(dy, dx);
                fixed dist_tiles = fixed_sqrt(FMUL(dx, dx) + FMUL(dy, dy));
                gs->aim_distance_yds = FROM_FIXED(FMUL(dist_tiles, TO_FIXED(YARDS_PER_TILE)));
                gs->has_custom_aim = false;
            }
            
            gs->swing.state = SWING_STATE_READY;
            gs->swing.cursor_pos = gs->swing.sweetspot_center;
            
            int32_t yds = golf_distance_to_pin_yards(gs);
            gs->current_club = recommend_club_for_distance(yds, gs->ball.current_surf);
        }
    } else if (gs->ball.state == BALL_WATER) {
        if (gs->prev_ball_gx < 0) gs->prev_ball_gx = 0;
        if (gs->prev_ball_gx >= GRID_W) gs->prev_ball_gx = GRID_W - 1;
        if (gs->prev_ball_gy < 0) gs->prev_ball_gy = 0;
        if (gs->prev_ball_gy >= GRID_H) gs->prev_ball_gy = GRID_H - 1;
        
        gs->ball.x = TO_FIXED(gs->prev_ball_gx) + FIXED_HALF;
        gs->ball.y = TO_FIXED(gs->prev_ball_gy) + FIXED_HALF;
        gs->ball.ground_z = golf_get_ground_z_fixed(gs, gs->ball.x, gs->ball.y);
        gs->ball.z = gs->ball.ground_z;
        gs->ball.vx = 0;
        gs->ball.vy = 0;
        gs->ball.vz = 0;
        gs->ball.state = BALL_IDLE;
        gs->ball.current_surf = golf_get_surface_at(gs, gs->ball.x, gs->ball.y);
        gs->swing.state = SWING_STATE_READY;
        gs->swing.cursor_pos = gs->swing.sweetspot_center;
    }
    
    /* 3. Golfer Walking & Footsteps */
    if (gs->mode == MODE_PLAY && gs->golfer.state == GOLFER_STATE_WALKING) {
        fixed gdx = gs->golfer.target_x - gs->golfer.x;
        fixed gdy = gs->golfer.target_y - gs->golfer.y;
        fixed gdist = fixed_sqrt(FMUL(gdx, gdx) + FMUL(gdy, gdy));
        fixed walk_spd = 5200;
        
        if (gdist <= walk_spd || gdist < TO_FIXED(1) / 3) {
            golf_golfer_arrive_at_ball(gs);
        } else {
            gs->golfer.facing_angle = fixed_atan2(gdy, gdx);
            gs->golfer.x += FMUL(walk_spd, fixed_cos(gs->golfer.facing_angle));
            gs->golfer.y += FMUL(walk_spd, fixed_sin(gs->golfer.facing_angle));
            gs->golfer.walk_ticks++;
            gs->golfer.walk_anim_frame = (gs->golfer.walk_ticks / 7) % 6;
            
            fixed target_gz = golf_get_ground_z_fixed(gs, gs->golfer.x, gs->golfer.y);
            /* Responsive elevation tracking so feet stay firmly planted on turf */
            fixed z_diff = target_gz - gs->golfer.current_z;
            fixed z_step = TO_FIXED(3); /* Responsive 3 px per tick */
            if (z_diff > z_step) gs->golfer.current_z += z_step;
            else if (z_diff < -z_step) gs->golfer.current_z -= z_step;
            else gs->golfer.current_z = target_gz;
            
            gs->golfer.step_timer++;
            if (gs->golfer.step_timer >= 18) {
                gs->golfer.step_timer = 0;
                SurfaceType surf = golf_get_surface_at(gs, gs->golfer.x, gs->golfer.y);
                if (surf == SURF_BRIDGE) gs->audio_event = AUDIO_EVT_FOOTSTEP_WOOD;
                else if (surf == SURF_SAND) gs->audio_event = AUDIO_EVT_FOOTSTEP_SAND;
                else if (surf == SURF_ROUGH) gs->audio_event = AUDIO_EVT_FOOTSTEP_ROUGH;
                else gs->audio_event = AUDIO_EVT_FOOTSTEP_GRASS;
            }
        }
    }
    
    /* 4. Caddy Companion Simulation */
    if (gs->mode == MODE_PLAY) {
        if (gs->caddy.state == CADDY_STATE_FOLLOWING) {
            fixed c_target_x = gs->golfer.x - FMUL(TO_FIXED(2), fixed_cos(gs->golfer.facing_angle));
            fixed c_target_y = gs->golfer.y - FMUL(TO_FIXED(2), fixed_sin(gs->golfer.facing_angle));
            fixed cdx = c_target_x - gs->caddy.x;
            fixed cdy = c_target_y - gs->caddy.y;
            fixed cdist = fixed_sqrt(FMUL(cdx, cdx) + FMUL(cdy, cdy));
            fixed c_spd = 5300;
            
            if (cdist > TO_FIXED(1) / 2) {
                gs->caddy.facing_angle = fixed_atan2(cdy, cdx);
                gs->caddy.x += FMUL(c_spd, fixed_cos(gs->caddy.facing_angle));
                gs->caddy.y += FMUL(c_spd, fixed_sin(gs->caddy.facing_angle));
                gs->caddy.walk_anim_frame = (gs->golfer.walk_ticks / 7) % 6;
            } else if (gs->golfer.state != GOLFER_STATE_WALKING) {
                gs->caddy.state = CADDY_STATE_READY;
                gs->caddy.facing_angle = gs->golfer.facing_angle;
            }
            fixed c_target_gz = golf_get_ground_z_fixed(gs, gs->caddy.x, gs->caddy.y);
            fixed cz_diff = c_target_gz - gs->caddy.current_z;
            fixed cz_step = TO_FIXED(3);
            if (cz_diff > cz_step) gs->caddy.current_z += cz_step;
            else if (cz_diff < -cz_step) gs->caddy.current_z -= cz_step;
            else gs->caddy.current_z = c_target_gz;
        }
    }
    
    /* 5. Spectator Gallery Simulation */
    for (int i = 0; i < gs->spectator_count; ++i) {
        SpectatorEntity *sp = &gs->spectators[i];
        if (!sp->active) continue;
        
        sp->timer++;
        if (gs->ball.state == BALL_IN_FLIGHT) {
            /* Heads turn to follow ball */
            fixed sdx = gs->ball.x - (TO_FIXED(sp->gx) + FIXED_HALF);
            fixed sdy = gs->ball.y - (TO_FIXED(sp->gy) + FIXED_HALF);
            sp->facing_angle = fixed_atan2(sdy, sdx);
            sp->state = SPEC_STATE_WATCHING;
        } else if (gs->hole_completed) {
            sp->state = SPEC_STATE_CHEERING;
            sp->anim_frame = (sp->timer / 12) % 2;
        } else if (sp->state == SPEC_STATE_WATCHING && gs->ball.state == BALL_IDLE) {
            sp->state = SPEC_STATE_IDLE;
        }
    }
    
    /* 6. Beverage Cart Simulation */
    if (gs->bev_cart.active) {
        if (gs->bev_cart.speech_timer > 0) {
            gs->bev_cart.speech_timer--;
        }
        
        if (gs->bev_cart.whistle_timer > 0) {
            gs->bev_cart.whistle_timer--;
            if (gs->bev_cart.whistle_timer == 0) {
                gs->audio_event = AUDIO_EVT_BEV_WHISTLE;
                gs->bev_cart.speech_timer = 150; /* Show speech bubble for ~2.5 seconds */
                gs->bev_cart.whistle_timer = 400 + (core_rand() % 400); /* Next whistle in 7..13 sec */
            }
        }
        
        if (gs->bev_cart.state == BEV_STATE_DRIVING) {
            fixed dx = gs->bev_cart.target_x - gs->bev_cart.x;
            fixed dy = gs->bev_cart.target_y - gs->bev_cart.y;
            fixed dist = fixed_sqrt(FMUL(dx, dx) + FMUL(dy, dy));
            
            if (dist < TO_FIXED(1)) {
                /* Arrived at waypoint! Pause to service golfers/spectators */
                gs->bev_cart.state = BEV_STATE_SERVICING;
                gs->bev_cart.wait_timer = 120 + (core_rand() % 120); /* 2-4 seconds pause */
            } else {
                gs->bev_cart.facing_angle = fixed_atan2(dy, dx);
                gs->bev_cart.x += FMUL(gs->bev_cart.speed, fixed_cos(gs->bev_cart.facing_angle));
                gs->bev_cart.y += FMUL(gs->bev_cart.speed, fixed_sin(gs->bev_cart.facing_angle));
                
                fixed target_gz = golf_get_ground_z_fixed(gs, gs->bev_cart.x, gs->bev_cart.y);
                fixed z_diff = target_gz - gs->bev_cart.current_z;
                fixed z_step = TO_FIXED(3);
                if (z_diff > z_step) gs->bev_cart.current_z += z_step;
                else if (z_diff < -z_step) gs->bev_cart.current_z -= z_step;
                else gs->bev_cart.current_z = target_gz;
            }
        } else if (gs->bev_cart.state == BEV_STATE_SERVICING) {
            if (gs->bev_cart.wait_timer > 0) {
                gs->bev_cart.wait_timer--;
            } else {
                /* Pick new destination waypoint across the course */
                int way_choice = core_rand() % 4;
                int dest_gx = gs->tee_gx;
                int dest_gy = gs->tee_gy;
                
                if (way_choice == 0) {
                    /* Near tee box */
                    dest_gx = gs->tee_gx + ((int)(core_rand() % 5) - 2);
                    dest_gy = gs->tee_gy + ((int)(core_rand() % 5) - 2);
                } else if (way_choice == 1) {
                    /* Mid-fairway corridor */
                    dest_gx = (gs->tee_gx + gs->pin_gx) / 2 + ((int)(core_rand() % 7) - 3);
                    dest_gy = (gs->tee_gy + gs->pin_gy) / 2 + ((int)(core_rand() % 7) - 3);
                } else if (way_choice == 2) {
                    /* Near putting green */
                    dest_gx = gs->pin_gx + 3 + ((int)(core_rand() % 4) - 2);
                    dest_gy = gs->pin_gy + 3 + ((int)(core_rand() % 4) - 2);
                } else {
                    /* Near clubhouse */
                    if (gs->has_clubhouse) {
                        dest_gx = gs->clubhouse_gx + 2 + ((int)(core_rand() % 3));
                        dest_gy = gs->clubhouse_gy + 3 + ((int)(core_rand() % 3));
                    } else {
                        dest_gx = gs->tee_gx - 3;
                        dest_gy = gs->tee_gy - 4;
                    }
                }
                
                if (dest_gx < 2) dest_gx = 2;
                if (dest_gx >= GRID_W - 2) dest_gx = GRID_W - 3;
                if (dest_gy < 2) dest_gy = 2;
                if (dest_gy >= GRID_H - 2) dest_gy = GRID_H - 3;
                
                /* Avoid water hazard */
                if (gs->grid[dest_gx][dest_gy].surface != SURF_WATER) {
                    gs->bev_cart.target_x = TO_FIXED(dest_gx) + FIXED_HALF;
                    gs->bev_cart.target_y = TO_FIXED(dest_gy) + FIXED_HALF;
                    gs->bev_cart.state = BEV_STATE_DRIVING;
                }
            }
        }
    }
    
    /* 7. Gopher / Raver / Zombie Entity Simulation */
    if (gs->mode == MODE_PLAY) {
        if (gs->gopher.state == GOPHER_STATE_HIDDEN) {
            if (gs->gopher.pop_cooldown > 0) {
                gs->gopher.pop_cooldown--;
            }
            if (gs->gopher.pop_cooldown <= 0) {
                int gx = 0, gy = 0;
                bool found = false;
                for (int attempts = 0; attempts < 30; ++attempts) {
                    int t_step = core_rand() % 100;
                    int cand_gx = gs->tee_gx + ((gs->pin_gx - gs->tee_gx) * t_step) / 100 + ((int)(core_rand() % 9) - 4);
                    int cand_gy = gs->tee_gy + ((gs->pin_gy - gs->tee_gy) * t_step) / 100 + ((int)(core_rand() % 9) - 4);
                    
                    if (cand_gx >= 2 && cand_gx < GRID_W - 2 && cand_gy >= 2 && cand_gy < GRID_H - 2) {
                        SurfaceType s = gs->grid[cand_gx][cand_gy].surface;
                        FeatureType f = gs->grid[cand_gx][cand_gy].feature;
                        if ((s == SURF_FAIRWAY || s == SURF_ROUGH || s == SURF_GREEN) &&
                            f != FEAT_CLUBHOUSE && f != FEAT_ROCK && gs->grid[cand_gx][cand_gy].elevation > 0) {
                            gx = cand_gx;
                            gy = cand_gy;
                            found = true;
                            break;
                        }
                    }
                }
                
                if (found) {
                    gs->gopher.gx = gx;
                    gs->gopher.gy = gy;
                    gs->gopher.x = TO_FIXED(gx) + FIXED_HALF;
                    gs->gopher.y = TO_FIXED(gy) + FIXED_HALF;
                    gs->gopher.current_z = golf_get_ground_z_fixed(gs, gs->gopher.x, gs->gopher.y);
                    gs->gopher.state = GOPHER_STATE_POPPING_UP;
                    gs->gopher.visible = true;
                    gs->gopher.anim_frame = 0;
                    gs->gopher.timer = 0;
                    gs->gopher_encounter_count++;
                    
                    if (gs->theme == THEME_NOCTURNAL_GREEN) {
                        gs->audio_event = AUDIO_EVT_ZOMBIE;
                        set_banner(gs, "A ZOMBIE CLAWS ITS WAY UP FROM THE GRAVE!", 180);
                    } else if (gs->theme == THEME_LA_PLAYA) {
                        gs->audio_event = AUDIO_EVT_GOPHER_CHUCKLE;
                        gs->grid[gx][gy].surface = SURF_SAND; /* Raver tramples ground to dirt */
                        set_banner(gs, "A DANCING RAVER APPEARS AND TRAMPLES THE GROUND TO DIRT!", 180);
                    } else {
                        gs->audio_event = AUDIO_EVT_GOPHER_CHUCKLE;
                    }
                    
                    /* Leave a permanent gopher hole hazard at this spot for Bushwood */
                    if (gs->theme == THEME_BUSHWOOD) {
                        bool already_has_hole = false;
                        for (int i = 0; i < gs->gopher.hole_count; ++i) {
                            if (gs->gopher.holes[i].gx == gx && gs->gopher.holes[i].gy == gy) {
                                already_has_hole = true;
                                break;
                            }
                        }
                        if (!already_has_hole) {
                            int h_idx = gs->gopher.hole_count;
                            if (h_idx >= MAX_GOPHER_HOLES) {
                                h_idx = core_rand() % MAX_GOPHER_HOLES;
                            } else {
                                gs->gopher.hole_count++;
                            }
                            gs->gopher.holes[h_idx].gx = gx;
                            gs->gopher.holes[h_idx].gy = gy;
                            gs->gopher.holes[h_idx].current_z = gs->gopher.current_z;
                            gs->gopher.holes[h_idx].active = true;
                        }
                    }
                } else {
                    gs->gopher.pop_cooldown = 600;
                }
            }
        } else if (gs->gopher.state == GOPHER_STATE_POPPING_UP) {
            gs->gopher.timer++;
            gs->gopher.anim_frame = (gs->gopher.timer / 10);
            if (gs->gopher.timer >= 20) {
                gs->gopher.state = GOPHER_STATE_TAUNTING;
                gs->gopher.timer = 0;
            }
        } else if (gs->gopher.state == GOPHER_STATE_TAUNTING) {
            gs->gopher.timer++;
            gs->gopher.anim_frame = (gs->gopher.timer / 10) % 4;
            
            if (gs->theme == THEME_LA_PLAYA) {
                /* Raver dancing, tramples current square */
                int rgx = gs->gopher.gx;
                int rgy = gs->gopher.gy;
                if (rgx >= 0 && rgx < GRID_W && rgy >= 0 && rgy < GRID_H) {
                    if (gs->grid[rgx][rgy].surface == SURF_FAIRWAY || gs->grid[rgx][rgy].surface == SURF_ROUGH) {
                        gs->grid[rgx][rgy].surface = SURF_SAND;
                    }
                }
                
                if (gs->gopher.timer >= 160 && gs->raver_passed_out_timer == 0) {
                    gs->raver_passed_out_timer = 180; /* Raver passes out! Zzz */
                }
                if (gs->raver_passed_out_timer > 0) {
                    gs->raver_passed_out_timer--;
                    if (gs->raver_passed_out_timer == 1) {
                        /* Wakes up and tramples another square! */
                        int n_gx = rgx + (int)(core_rand() % 3) - 1;
                        int n_gy = rgy + (int)(core_rand() % 3) - 1;
                        if (n_gx >= 2 && n_gx < GRID_W - 2 && n_gy >= 2 && n_gy < GRID_H - 2) {
                            gs->gopher.gx = n_gx;
                            gs->gopher.gy = n_gy;
                            gs->gopher.x = TO_FIXED(n_gx) + FIXED_HALF;
                            gs->gopher.y = TO_FIXED(n_gy) + FIXED_HALF;
                            gs->grid[n_gx][n_gy].surface = SURF_SAND;
                            set_banner(gs, "RAVER WAKES UP & TRAMPLES ANOTHER SQUARE!", 180);
                        }
                        gs->gopher.state = GOPHER_STATE_RETREATING;
                        gs->gopher.timer = 0;
                    }
                }
            } else if (gs->theme == THEME_NOCTURNAL_GREEN) {
                /* Zombie wanders and chases caddy if hit */
                if (gs->gopher.timer % 30 == 0) {
                    int w_dx = (int)(core_rand() % 3) - 1;
                    int w_dy = (int)(core_rand() % 3) - 1;
                    gs->gopher.x += w_dx * 9830;
                    gs->gopher.y += w_dy * 9830;
                    gs->gopher.gx = FROM_FIXED(gs->gopher.x);
                    gs->gopher.gy = FROM_FIXED(gs->gopher.y);
                    gs->gopher.current_z = golf_get_ground_z_fixed(gs, gs->gopher.x, gs->gopher.y);
                }
                
                fixed bdx = gs->ball.x - gs->gopher.x;
                fixed bdy = gs->ball.y - gs->gopher.y;
                fixed bdist2 = FMUL(bdx, bdx) + FMUL(bdy, bdy);
                
                fixed pdx = gs->golfer.x - gs->gopher.x;
                fixed pdy = gs->golfer.y - gs->gopher.y;
                fixed pdist2 = FMUL(pdx, pdx) + FMUL(pdy, pdy);
                
                bool ball_touch = (gs->ball.state == BALL_IN_FLIGHT || gs->ball.state == BALL_ROLLING || gs->ball.state == BALL_BOUNCING) && (bdist2 < TO_FIXED(2));
                bool player_touch = (pdist2 < (TO_FIXED(15) / 10));
                
                if (ball_touch || player_touch) {
                    fixed cdx = gs->caddy.x - gs->gopher.x;
                    fixed cdy = gs->caddy.y - gs->gopher.y;
                    fixed cdist = fixed_sqrt(FMUL(cdx, cdx) + FMUL(cdy, cdy));
                    if (cdist < (TO_FIXED(6) / 10)) {
                        gs->caddy_eaten = true;
                        gs->caddy_eaten_timer = 360;
                        gs->audio_event = AUDIO_EVT_ZOMBIE;
                        set_banner(gs, "THE ZOMBIE ATE YOUR CADDY! NO CLUB ADVICE!", 240);
                        gs->gopher.state = GOPHER_STATE_RETREATING;
                        gs->gopher.timer = 0;
                    } else if (cdist > 0) {
                        gs->gopher.x += FDIV(FMUL(cdx, TO_FIXED(8) / 100), cdist);
                        gs->gopher.y += FDIV(FMUL(cdy, TO_FIXED(8) / 100), cdist);
                        gs->gopher.current_z = golf_get_ground_z_fixed(gs, gs->gopher.x, gs->gopher.y);
                    }
                }
                
                if (gs->gopher.timer >= 240) {
                    gs->gopher.state = GOPHER_STATE_RETREATING;
                    gs->gopher.timer = 0;
                }
            } else {
                /* Bushwood Gopher */
                if (gs->gopher.timer >= 180) {
                    gs->gopher.state = GOPHER_STATE_RETREATING;
                    gs->gopher.timer = 0;
                }
            }
        } else if (gs->gopher.state == GOPHER_STATE_RETREATING) {
            gs->gopher.timer++;
            gs->gopher.anim_frame = 1 - (gs->gopher.timer / 10);
            if (gs->gopher.timer >= 20) {
                gs->gopher.state = GOPHER_STATE_HIDDEN;
                gs->gopher.visible = false;
                gs->gopher.timer = 0;
                gs->gopher.pop_cooldown = 3600 + (core_rand() % 2400); /* 60 to 100 seconds */
            }
        }
    }
    
    /* 8. Carl / Police Cop / Ash Williams Nemesis Simulation */
    if (gs->mode == MODE_PLAY) {
        if (gs->carl_cooldown > 0) {
            gs->carl_cooldown--;
        }
        
        if (!gs->carl.active && gs->carl_cooldown <= 0 && (core_rand() % 20 == 0) &&
            gs->gopher.state >= GOPHER_STATE_POPPING_UP && gs->gopher.state <= GOPHER_STATE_RETREATING) {
            
            int tgx = gs->gopher.gx;
            int tgy = gs->gopher.gy;
            
            bool safe_from_tee = (abs(tgx - gs->tee_gx) >= 4 || abs(tgy - gs->tee_gy) >= 4);
            bool safe_from_pin = (abs(tgx - gs->pin_gx) >= 4 || abs(tgy - gs->pin_gy) >= 4);
            bool safe_from_bounds = (tgx >= 3 && tgx < GRID_W - 3 && tgy >= 3 && tgy < GRID_H - 3);
            
            if (safe_from_tee && safe_from_pin && safe_from_bounds) {
                gs->carl.active = true;
                gs->carl.target_gx = tgx;
                gs->carl.target_gy = tgy;
                gs->carl.triggered_effect = false;
                gs->carl.timer = 0;
                gs->carl.anim_frame = 0;
                gs->gopher_encounter_count = 0;
                gs->carl_cooldown = 3600; /* 60 seconds */
                
                if (gs->theme == THEME_LA_PLAYA) {
                    gs->carl.attack_type = CARL_ATTACK_HOSE;
                    gs->audio_event = AUDIO_EVT_SIREN;
                } else {
                    gs->carl.attack_type = (core_rand() % 2 == 0) ? CARL_ATTACK_DYNAMITE : CARL_ATTACK_HOSE;
                }
                
                int off_x = (tgx < GRID_W / 2) ? 3 : -3;
                int off_y = (tgy < GRID_H / 2) ? 3 : -3;
                gs->carl.start_x = TO_FIXED(tgx + off_x) + FIXED_HALF;
                gs->carl.start_y = TO_FIXED(tgy + off_y) + FIXED_HALF;
                gs->carl.x = gs->carl.start_x;
                gs->carl.y = gs->carl.start_y;
                gs->carl.current_z = golf_get_ground_z_fixed(gs, gs->carl.x, gs->carl.y);
                
                gs->carl.plunger_x = TO_FIXED(tgx) + FIXED_HALF + TO_FIXED(off_x * 6 / 10);
                gs->carl.plunger_y = TO_FIXED(tgy) + FIXED_HALF + TO_FIXED(off_y * 6 / 10);
                
                gs->carl.state = CARL_STATE_APPROACHING;
            }
        }
        
        if (gs->carl.active) {
            gs->carl.timer++;
            
            switch (gs->carl.state) {
                case CARL_STATE_APPROACHING: {
                    fixed dest_x = TO_FIXED(gs->carl.target_gx) + FIXED_HALF;
                    fixed dest_y = TO_FIXED(gs->carl.target_gy) + FIXED_HALF;
                    fixed dx = dest_x - gs->carl.x;
                    fixed dy = dest_y - gs->carl.y;
                    fixed dist = fixed_sqrt(FMUL(dx, dx) + FMUL(dy, dy));
                    fixed step = (gs->theme == THEME_LA_PLAYA) ? (TO_FIXED(8) / 100) : (TO_FIXED(5) / 100);
                    
                    if (dist <= step || gs->carl.timer >= 120) {
                        gs->carl.x = dest_x;
                        gs->carl.y = dest_y;
                        gs->carl.timer = 0;
                        
                        if (gs->theme == THEME_LA_PLAYA) {
                            /* Cop catches Raver! */
                            gs->gopher.state = GOPHER_STATE_HIDDEN;
                            gs->gopher.visible = false;
                            gs->audio_event = AUDIO_EVT_SIREN;
                            set_banner(gs, "OFFICER: 'PARTY OVER!' RAVER ARRESTED ON POLICE GOLF CART!", 240);
                            gs->carl.state = CARL_STATE_LEAVING;
                        } else if (gs->theme == THEME_NOCTURNAL_GREEN) {
                            /* Ash Williams attacks Zombie */
                            gs->gopher.state = GOPHER_STATE_HIDDEN;
                            gs->gopher.visible = false;
                            if (core_rand() % 2 == 0) {
                                gs->audio_event = AUDIO_EVT_SHOTGUN;
                                set_banner(gs, "ASH: 'THIS IS MY BOOMSTICK!' ZOMBIE ANNIHILATED!", 240);
                            } else {
                                gs->audio_event = AUDIO_EVT_CHAINSAW;
                                set_banner(gs, "ASH: 'GROOVY!' ZOMBIE CHAINSAWED IN HALF!", 240);
                            }
                            gs->carl.state = CARL_STATE_CELEBRATING;
                        } else {
                            if (gs->carl.attack_type == CARL_ATTACK_DYNAMITE) {
                                gs->carl.state = CARL_STATE_DYNAMITE_PLANT;
                            } else {
                                gs->carl.state = CARL_STATE_HOSE_SPRAY;
                                gs->audio_event = AUDIO_EVT_CARL_HOSE;
                            }
                        }
                    } else {
                        gs->carl.x += FDIV(FMUL(dx, step), dist);
                        gs->carl.y += FDIV(FMUL(dy, step), dist);
                        gs->carl.current_z = golf_get_ground_z_fixed(gs, gs->carl.x, gs->carl.y);
                        gs->carl.anim_frame = (gs->carl.timer / 8) % 4;
                    }
                    break;
                }
                
                case CARL_STATE_DYNAMITE_PLANT: {
                    gs->carl.anim_frame = (gs->carl.timer / 15) % 2;
                    if (gs->carl.timer >= 35) {
                        gs->carl.state = CARL_STATE_DYNAMITE_PLUNGE;
                        gs->carl.timer = 0;
                    }
                    break;
                }
                
                case CARL_STATE_DYNAMITE_PLUNGE: {
                    gs->carl.anim_frame = (gs->carl.timer < 14) ? 0 : 1;
                    if (gs->carl.timer == 14 && !gs->carl.triggered_effect) {
                        gs->carl.triggered_effect = true;
                        gs->audio_event = AUDIO_EVT_CARL_EXPLOSION;
                        int tgx = gs->carl.target_gx;
                        int tgy = gs->carl.target_gy;
                        if (gs->grid[tgx][tgy].elevation > 0) gs->grid[tgx][tgy].elevation--;
                        for (int dy = -1; dy <= 1; ++dy) {
                            for (int dx = -1; dx <= 1; ++dx) {
                                int cx = tgx + dx, cy = tgy + dy;
                                if (cx >= 1 && cx < GRID_W - 1 && cy >= 1 && cy < GRID_H - 1) {
                                    if (abs(cx - gs->tee_gx) <= 2 && abs(cy - gs->tee_gy) <= 2) continue;
                                    if (abs(cx - gs->pin_gx) <= 2 && abs(cy - gs->pin_gy) <= 2) continue;
                                    if (gs->grid[cx][cy].surface == SURF_GREEN || gs->grid[cx][cy].surface == SURF_TEE) continue;
                                    if (gs->grid[cx][cy].feature == FEAT_CLUBHOUSE) continue;
                                    gs->grid[cx][cy].feature = FEAT_NONE;
                                    gs->grid[cx][cy].surface = (dx == 0 && dy == 0) ? SURF_SAND : SURF_ROUGH;
                                }
                            }
                        }
                        golf_update_autotile_masks(gs);
                        set_banner(gs, "CARL SPACKLER: 'CINDERELLA STORY!' DYNAMITE CRATER CREATED! lol", 240);
                    }
                    if (gs->carl.timer >= 70) {
                        gs->carl.state = CARL_STATE_CELEBRATING;
                        gs->carl.timer = 0;
                    }
                    break;
                }
                
                case CARL_STATE_HOSE_SPRAY: {
                    gs->carl.anim_frame = (gs->carl.timer / 6) % 2;
                    if (gs->carl.timer == 45) gs->audio_event = AUDIO_EVT_CARL_HOSE;
                    if (gs->carl.timer == 50 && !gs->carl.triggered_effect) {
                        gs->carl.triggered_effect = true;
                        int tgx = gs->carl.target_gx;
                        int tgy = gs->carl.target_gy;
                        for (int dy = -1; dy <= 1; ++dy) {
                            for (int dx = -1; dx <= 1; ++dx) {
                                int cx = tgx + dx, cy = tgy + dy;
                                if (cx >= 1 && cx < GRID_W - 1 && cy >= 1 && cy < GRID_H - 1) {
                                    if (abs(cx - gs->tee_gx) <= 2 && abs(cy - gs->tee_gy) <= 2) continue;
                                    if (abs(cx - gs->pin_gx) <= 2 && abs(cy - gs->pin_gy) <= 2) continue;
                                    if (gs->grid[cx][cy].surface == SURF_GREEN || gs->grid[cx][cy].surface == SURF_TEE) continue;
                                    if (gs->grid[cx][cy].feature == FEAT_CLUBHOUSE) continue;
                                    gs->grid[cx][cy].feature = FEAT_NONE;
                                    if (abs(dx) + abs(dy) <= 1) gs->grid[cx][cy].surface = SURF_WATER;
                                }
                            }
                        }
                        golf_update_autotile_masks(gs);
                        set_banner(gs, "CARL SPACKLER: 'IN THE HOLE!' HOSE FLOODED A WATER HAZARD! lol", 240);
                    }
                    if (gs->carl.timer >= 95) {
                        gs->carl.state = CARL_STATE_CELEBRATING;
                        gs->carl.timer = 0;
                    }
                    break;
                }
                
                case CARL_STATE_CELEBRATING: {
                    gs->carl.anim_frame = (gs->carl.timer / 12) % 2;
                    if (gs->carl.timer >= 85) {
                        gs->carl.state = CARL_STATE_LEAVING;
                        gs->carl.timer = 0;
                    }
                    break;
                }
                
                case CARL_STATE_LEAVING: {
                    fixed dest_x = gs->carl.start_x;
                    fixed dest_y = gs->carl.start_y;
                    fixed dx = dest_x - gs->carl.x;
                    fixed dy = dest_y - gs->carl.y;
                    fixed dist = fixed_sqrt(FMUL(dx, dx) + FMUL(dy, dy));
                    fixed step = (TO_FIXED(7) / 100);
                    if (dist <= step || gs->carl.timer >= 100) {
                        gs->carl.state = CARL_STATE_INACTIVE;
                        gs->carl.active = false;
                        gs->carl.timer = 0;
                    } else {
                        gs->carl.x += FDIV(FMUL(dx, step), dist);
                        gs->carl.y += FDIV(FMUL(dy, step), dist);
                        gs->carl.current_z = golf_get_ground_z_fixed(gs, gs->carl.x, gs->carl.y);
                        gs->carl.anim_frame = (gs->carl.timer / 8) % 4;
                    }
                    break;
                }
                
                default:
                    gs->carl.state = CARL_STATE_INACTIVE;
                    gs->carl.active = false;
                    break;
            }
        }
    }
    
    /* 9. Tumbleweeds Simulation for La Playa Theme */
    if (gs->mode == MODE_PLAY && gs->theme == THEME_LA_PLAYA) {
        for (int i = 0; i < MAX_TUMBLEWEEDS; ++i) {
            TumbleweedEntity *tw = &gs->tumbleweeds[i];
            if (!tw->active) {
                if (core_rand() % 1200 == 0) {
                    tw->active = true;
                    tw->x = 0;
                    tw->y = TO_FIXED(core_rand() % GRID_H) + FIXED_HALF;
                    tw->vx = (TO_FIXED(4) / 100) + (core_rand() % 4) * (TO_FIXED(1) / 100);
                    tw->vy = (TO_FIXED(1) / 100) + (core_rand() % 3) * (TO_FIXED(1) / 100);
                    tw->current_z = golf_get_ground_z_fixed(gs, tw->x, tw->y);
                    tw->rotation = 0;
                    tw->stuck = false;
                }
            } else if (!tw->stuck) {
                tw->x += tw->vx;
                tw->y += tw->vy;
                tw->rotation = (tw->rotation + 8) % 360;
                
                if (tw->x >= TO_FIXED(GRID_W) || tw->y >= TO_FIXED(GRID_H)) {
                    tw->active = false;
                } else {
                    int tgx = FROM_FIXED(tw->x);
                    int tgy = FROM_FIXED(tw->y);
                    if (tgx >= 0 && tgx < GRID_W && tgy >= 0 && tgy < GRID_H) {
                        FeatureType f = gs->grid[tgx][tgy].feature;
                        if (f == FEAT_PINE || f == FEAT_OAK || f == FEAT_PALM || f == FEAT_ROCK) {
                            tw->stuck = true; /* Piled up on tree or boulder! */
                        }
                    }
                }
            }
        }
    }
    
    /* 10. Sea Monster Simulation for Nocturnal Green Theme */
    if (gs->mode == MODE_PLAY && gs->theme == THEME_NOCTURNAL_GREEN) {
        if (gs->sea_monster.timer > 0) {
            gs->sea_monster.timer--;
        }
        if (gs->sea_monster.timer == 0) {
            int found_wx = -1, found_wy = -1;
            for (int a = 0; a < 20; ++a) {
                int wx = core_rand() % GRID_W;
                int wy = 10 + (core_rand() % (GRID_H - 20));
                if (gs->grid[wx][wy].surface == SURF_WATER) {
                    found_wx = wx; found_wy = wy;
                    break;
                }
            }
            if (found_wx >= 0) {
                gs->sea_monster.active = true;
                gs->sea_monster.state = 1; /* Eyes and head peeking out */
                gs->sea_monster.x = TO_FIXED(found_wx) + FIXED_HALF;
                gs->sea_monster.y = TO_FIXED(found_wy) + FIXED_HALF;
                gs->sea_monster.current_z = golf_get_ground_z_fixed(gs, gs->sea_monster.x, gs->sea_monster.y);
                gs->sea_monster.timer = 180;
            } else {
                gs->sea_monster.timer = 600;
            }
        } else if (gs->sea_monster.state == 1 && gs->sea_monster.timer <= 1) {
            gs->sea_monster.active = false;
            gs->sea_monster.state = 0;
            gs->sea_monster.timer = 2400 + (core_rand() % 1800);
        }
        
        /* Rare sea monster appendage swipe: ONLY when ball came to rest right at water's edge */
        if (gs->ball.state == BALL_IDLE && gs->sea_monster.state == 0 && gs->strokes > 0) {
            int bgx = FROM_FIXED(gs->ball.x);
            int bgy = FROM_FIXED(gs->ball.y);
            bool near_water = false;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    int wx = bgx + dx, wy = bgy + dy;
                    if (wx >= 0 && wx < GRID_W && wy >= 0 && wy < GRID_H) {
                        if (gs->grid[wx][wy].surface == SURF_WATER) near_water = true;
                    }
                }
            }
            if (near_water && (core_rand() % 20 == 0)) {
                gs->sea_monster.active = true;
                gs->sea_monster.state = 2; /* Tentacle swipe! */
                gs->sea_monster.x = gs->ball.x;
                gs->sea_monster.y = gs->ball.y;
                gs->sea_monster.current_z = gs->ball.ground_z;
                gs->sea_monster.timer = 90;
                gs->ball.state = BALL_WATER;
                gs->strokes++;
                gs->audio_event = AUDIO_EVT_SEAMONSTER;
                set_banner(gs, "A LOCH NESS LEVIATHAN SWIPES YOUR BALL INTO THE DEPTHS!", 240);
            }
        } else if (gs->sea_monster.state == 2) {
            if (gs->sea_monster.timer > 0) gs->sea_monster.timer--;
            else {
                gs->sea_monster.state = 0;
                gs->sea_monster.active = false;
                gs->sea_monster.timer = 2400 + (core_rand() % 1800);
            }
        }
    }
    
    /* 11. Ranger Patrol & Scolding Simulation for Sneak On Mode */
    if (gs->mode == MODE_PLAY && gs->is_sneak_on && gs->ranger.active) {
        if (gs->ranger.cooldown > 0) {
            gs->ranger.cooldown--;
        }
        
        if (gs->ranger.state == RANGER_STATE_PATROLLING) {
            fixed rdx = gs->ranger.target_x - gs->ranger.x;
            fixed rdy = gs->ranger.target_y - gs->ranger.y;
            fixed rdist = fixed_sqrt(FMUL(rdx, rdx) + FMUL(rdy, rdy));
            if (rdist < FIXED_HALF) {
                /* Pick a patrol waypoint along outer perimeters / rough edges */
                int side = core_rand() % 4;
                int rgx = 4, rgy = 4;
                if (side == 0) { rgx = 3 + (core_rand() % 6); rgy = 6 + (core_rand() % (GRID_H - 12)); }
                else if (side == 1) { rgx = GRID_W - 9 + (core_rand() % 6); rgy = 6 + (core_rand() % (GRID_H - 12)); }
                else if (side == 2) { rgx = 6 + (core_rand() % (GRID_W - 12)); rgy = 3 + (core_rand() % 6); }
                else { rgx = 6 + (core_rand() % (GRID_W - 12)); rgy = GRID_H - 9 + (core_rand() % 6); }
                
                gs->ranger.target_x = TO_FIXED(rgx) + FIXED_HALF;
                gs->ranger.target_y = TO_FIXED(rgy) + FIXED_HALF;
                gs->ranger.facing_angle = fixed_atan2(gs->ranger.target_y - gs->ranger.y, gs->ranger.target_x - gs->ranger.x);
            } else {
                fixed step = gs->ranger.speed;
                gs->ranger.x += FDIV(FMUL(rdx, step), rdist);
                gs->ranger.y += FDIV(FMUL(rdy, step), rdist);
                gs->ranger.facing_angle = fixed_atan2(rdy, rdx);
                gs->ranger.current_z = golf_get_ground_z_fixed(gs, gs->ranger.x, gs->ranger.y);
            }
            
            fixed bdx = gs->ball.x - gs->ranger.x;
            fixed bdy = gs->ball.y - gs->ranger.y;
            fixed bdist = fixed_sqrt(FMUL(bdx, bdx) + FMUL(bdy, bdy));
            
            fixed gdx = gs->golfer.x - gs->ranger.x;
            fixed gdy = gs->golfer.y - gs->ranger.y;
            fixed gdist = fixed_sqrt(FMUL(gdx, gdx) + FMUL(gdy, gdy));
            
            bool ball_hit = (gs->ball.state == BALL_IN_FLIGHT || gs->ball.state == BALL_ROLLING || gs->ball.state == BALL_BOUNCING) && (bdist < (TO_FIXED(9) / 10));
            bool golfer_hit = (gdist < (TO_FIXED(9) / 10));
            
            if (gs->ranger.cooldown <= 0 && (ball_hit || golfer_hit)) {
                /* User request: catch only ~1 in 20 times (5%) */
                if (core_rand() % 20 == 0) {
                    golf_reset_ball_to_tee(gs);
                    golf_golfer_arrive_at_ball(gs);
                    gs->ranger.state = RANGER_STATE_SCOLDING;
                    gs->ranger.speech_timer = 180;
                    gs->ranger.cooldown = 1800; /* 30 seconds immunity */
                    
                    /* Send ranger driving away from the tee */
                    gs->ranger.target_x = TO_FIXED(GRID_W - 6) + FIXED_HALF;
                    gs->ranger.target_y = TO_FIXED(GRID_H - 6) + FIXED_HALF;
                    
                    const char *r_msgs[3] = {
                        "RANGER: 'HEY! NO SNEAKING ON BUSHWOOD! BACK TO TEE #1!'",
                        "RANGER: 'MEMBERS ONLY! YOU GET OFF MY COURSE!'",
                        "RANGER: 'CAUGHT YOU! TEE OFF LEGALLY OR GET ESCORTED OFF!'"
                    };
                    strncpy(gs->ranger.speech_msg, r_msgs[core_rand() % 3], sizeof(gs->ranger.speech_msg) - 1);
                    set_banner(gs, gs->ranger.speech_msg, 240);
                } else {
                    /* 95%: Ranger doesn't catch you, just drives by with warning shout */
                    gs->ranger.cooldown = 900; /* 15 seconds immunity */
                    const char *warn_msgs[3] = {
                        "RANGER: 'KEEP PACE! 4 HOURS OR LESS!'",
                        "RANGER: 'WATCH OUT FOR MY CART!'",
                        "RANGER: 'KEEP THOSE TIRES OFF THE GREEN!'"
                    };
                    strncpy(gs->ranger.speech_msg, warn_msgs[core_rand() % 3], sizeof(gs->ranger.speech_msg) - 1);
                    gs->ranger.speech_timer = 90;
                }
            }
        } else if (gs->ranger.state == RANGER_STATE_SCOLDING) {
            if (gs->ranger.speech_timer > 0) {
                gs->ranger.speech_timer--;
            } else {
                gs->ranger.state = RANGER_STATE_PATROLLING;
            }
        }
    }
}

void golf_golfer_arrive_at_ball(GolfState *gs) {
    gs->golfer.x = gs->ball.x;
    gs->golfer.y = gs->ball.y;
    gs->golfer.current_z = gs->ball.ground_z;
    gs->golfer.target_x = gs->ball.x;
    gs->golfer.target_y = gs->ball.y;
    gs->golfer.state = GOLFER_STATE_READY;
    gs->golfer.walk_anim_frame = 0;
    gs->golfer.facing_angle = gs->aim_angle;
    
    gs->caddy.state = CADDY_STATE_READY;
    gs->caddy.facing_angle = gs->aim_angle;
}

int golf_get_water_proximity(const GolfState *gs, fixed view_x, fixed view_y) {
    int32_t cx = FROM_FIXED(view_x);
    int32_t cy = FROM_FIXED(view_y);
    int min_dist_sq = 999999;
    
    for (int dy = -10; dy <= 10; ++dy) {
        for (int dx = -10; dx <= 10; ++dx) {
            int gx = cx + dx;
            int gy = cy + dy;
            if (gx >= 0 && gx < GRID_W && gy >= 0 && gy < GRID_H) {
                if (gs->grid[gx][gy].surface == SURF_WATER) {
                    int d2 = dx * dx + dy * dy;
                    if (d2 < min_dist_sq) min_dist_sq = d2;
                }
            }
        }
    }
    
    if (min_dist_sq >= 100) return 0;
    int dist = 0;
    while ((dist + 1) * (dist + 1) <= min_dist_sq) dist++;
    int pct = 100 - dist * 10;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return pct;
}

int golf_get_clubhouse_proximity(const GolfState *gs, fixed view_x, fixed view_y) {
    if (!gs->has_clubhouse) return 0;
    int32_t cx = FROM_FIXED(view_x);
    int32_t cy = FROM_FIXED(view_y);
    int32_t dx = cx - (gs->clubhouse_gx + 1);
    int32_t dy = cy - (gs->clubhouse_gy + 1);
    int d2 = dx * dx + dy * dy;
    if (d2 >= 64) return 0;
    int dist = 0;
    while ((dist + 1) * (dist + 1) <= d2) dist++;
    int pct = 100 - dist * 12;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return pct;
}
