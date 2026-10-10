#include "golf_core.h"
#include <stdio.h>
#include <string.h>

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

/* Fast binary-radian atan2 */
uint8_t fixed_atan2(fixed dy, fixed dx) {
    if (dx == 0 && dy == 0) return 0;
    
    fixed ax = FABS(dx);
    fixed ay = FABS(dy);
    int32_t angle = 0;
    
    if (ax >= ay) {
        if (ax == 0) return 0;
        int32_t ratio = (int32_t)(((int64_t)ay * 32) / ax);
        angle = ratio;
    } else {
        int32_t ratio = (int32_t)(((int64_t)ax * 32) / ay);
        angle = 64 - ratio;
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

/* Club Definitions */
static const ClubData s_clubs[CLUB_COUNT] = {
    { "DRIVER 1W",  "1W", 240, 36000, 185000, 28000, 60293, false }, /* Carry ~195 yds, Roll ~45 yds */
    { "3-WOOD 3W",  "3W", 215, 29800, 200000, 26000, 59637, false }, /* Carry ~175 yds, Roll ~40 yds */
    { "5-IRON 5I",  "5I", 175, 22900, 215000, 22000, 58327, false }, /* Carry ~145 yds, Roll ~30 yds */
    { "9-IRON 9I",  "9I", 130, 16384, 228000, 16000, 56360, false }, /* Carry ~110 yds, Roll ~20 yds */
    { "WEDGE PW",   "PW",  90, 11400, 238000, 10000, 54394, false }, /* Carry ~80 yds,  Roll ~10 yds */
    { "PUTTER PT",  "PT",  25, 18000,      0,     0, 62259, true  }  /* Ground roll up to 25 yds */
};

const ClubData* golf_get_club_data(ClubType club) {
    if (club >= CLUB_COUNT) club = CLUB_1W;
    return &s_clubs[club];
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

static void set_banner(GolfState *gs, const char *msg, int32_t ticks) {
    strncpy(gs->banner_text, msg, sizeof(gs->banner_text) - 1);
    gs->banner_text[sizeof(gs->banner_text) - 1] = '\0';
    gs->banner_timer = ticks;
}

void golf_generate_terrain(GolfState *gs, uint32_t seed) {
    core_srand(seed);
    
    /* 1. Value noise height generation (0..5 tiers) */
    int32_t noise[GRID_W][GRID_H];
    for (int y = 0; y < GRID_H; ++y) {
        for (int x = 0; x < GRID_W; ++x) {
            noise[x][y] = (int32_t)(core_rand() % 100);
        }
    }
    
    /* 3 passes of 3x3 box smoothing */
    for (int pass = 0; pass < 3; ++pass) {
        int32_t temp[GRID_W][GRID_H];
        for (int y = 0; y < GRID_H; ++y) {
            for (int x = 0; x < GRID_W; ++x) {
                int32_t sum = 0;
                int32_t count = 0;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx >= 0 && nx < GRID_W && ny >= 0 && ny < GRID_H) {
                            sum += noise[nx][ny];
                            count++;
                        }
                    }
                }
                temp[x][y] = sum / count;
            }
        }
        memcpy(noise, temp, sizeof(noise));
    }
    
    /* Map noise to discrete elevation tiers (0..4) */
    for (int y = 0; y < GRID_H; ++y) {
        for (int x = 0; x < GRID_W; ++x) {
            int32_t v = noise[x][y];
            uint8_t elev;
            uint8_t surf;
            
            if (v < 32) {
                elev = ELEV_WATER;
                surf = SURF_WATER;
            } else if (v < 48) {
                elev = ELEV_LOWLAND;
                surf = SURF_ROUGH;
            } else if (v < 66) {
                elev = ELEV_VALLEY;
                surf = SURF_ROUGH;
            } else if (v < 82) {
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
    
    /* Populate natural features in rough areas */
    for (int y = 2; y < GRID_H - 2; ++y) {
        for (int x = 2; x < GRID_W - 2; ++x) {
            if (gs->grid[x][y].surface == SURF_ROUGH) {
                uint32_t r = core_rand() % 100;
                if (r < 4) {
                    gs->grid[x][y].feature = FEAT_PINE;
                } else if (r < 7) {
                    gs->grid[x][y].feature = FEAT_OAK;
                } else if (r < 9) {
                    gs->grid[x][y].feature = FEAT_FLOWERING;
                } else if (r < 11) {
                    gs->grid[x][y].feature = FEAT_BUSH;
                } else if (r < 13 && gs->grid[x][y].elevation >= ELEV_RIDGE) {
                    gs->grid[x][y].feature = FEAT_ROCK;
                }
            }
        }
    }
    
    golf_update_autotile_masks(gs);
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
    for (int y = gs->pin_gy + 4; y < gs->tee_gy - 1; ++y) {
        float t = (float)(y - (gs->pin_gy + 4)) / (float)(gs->tee_gy - 1 - (gs->pin_gy + 4));
        int center_x = (int)((1.0f - t) * gs->pin_gx + t * gs->tee_gx);
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
    gs->gopher.pop_cooldown = 180 + (core_rand() % 180); /* Pops up early in play */
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
    
    golf_update_autotile_masks(gs);
    golf_recalc_hole_stats(gs);
    golf_reset_ball_to_tee(gs);
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
    golf_generate_terrain(gs, seed);
    
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
    
    /* Stamp Tee Box */
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
    
    /* Stamp Putting Green */
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
    
    /* Stamp Fairway Corridor connecting Tee to Green */
    int start_y = gs->pin_gy + 4;
    int end_y = gs->tee_gy - 1;
    for (int y = start_y; y <= end_y; ++y) {
        float t = (float)(y - start_y) / (float)(end_y - start_y);
        int center_x = (int)((1.0f - t) * gs->pin_gx + t * gs->tee_gx);
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
    
    golf_update_autotile_masks(gs);
    golf_recalc_hole_stats(gs);
    golf_reset_ball_to_tee(gs);
    set_banner(gs, "NEW HOLE GENERATED!", 120);
}

void golf_init(GolfState *gs) {
    memset(gs, 0, sizeof(GolfState));
    
    gs->mode = MODE_SPLASH;
    gs->build_category = BUILD_CAT_TERRAIN;
    gs->current_tool = TOOL_FAIRWAY;
    gs->brush_size = 1;
    gs->club_funds = 150000; /* $150,000 SimGolf initial budget */
    
    golf_generate_terrain(gs, 0x51A601F);
    golf_place_default_routing(gs);
    
    /* Swing Gauge defaults */
    gs->swing.state = SWING_STATE_READY;
    gs->swing.cursor_pos = 0;
    gs->swing.cursor_speed = 1000; /* Smooth SimGolf swing meter speed */
    gs->swing.locked_power = FIXED_ONE;
    gs->swing.locked_accuracy = 0;
    gs->swing.moving_up = true;
    gs->swing.sweetspot_center = FIXED_ONE / 8;
    gs->swing.sweetspot_width  = 3200;
    
    /* Wind system */
    gs->wind.angle = 64; /* East-South-East */
    gs->wind.speed = 12; /* 12 mph gentle breeze */
    
    gs->current_club = CLUB_1W;
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
    if (yds > 215) gs->current_club = CLUB_1W;
    else if (yds > 175) gs->current_club = CLUB_3W;
    else if (yds > 130) gs->current_club = CLUB_5I;
    else if (yds > 80) gs->current_club = CLUB_9I;
    else gs->current_club = CLUB_PW;
    
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
    }
}

void golf_change_club(GolfState *gs, int delta) {
    int next = (int)gs->current_club + delta;
    if (next < 0) next = CLUB_COUNT - 1;
    if (next >= CLUB_COUNT) next = 0;
    gs->current_club = (ClubType)next;
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
    
    /* Auto-select optimal club */
    if (gs->ball.current_surf == SURF_GREEN || gs->aim_distance_yds <= 20) {
        gs->current_club = CLUB_PT;
    } else if (gs->aim_distance_yds > 215) {
        gs->current_club = CLUB_1W;
    } else if (gs->aim_distance_yds > 175) {
        gs->current_club = CLUB_3W;
    } else if (gs->aim_distance_yds > 130) {
        gs->current_club = CLUB_5I;
    } else if (gs->aim_distance_yds > 80) {
        gs->current_club = CLUB_9I;
    } else {
        gs->current_club = CLUB_PW;
    }
}

void golf_swing_click(GolfState *gs) {
    if (gs->mode != MODE_PLAY || gs->ball.state != BALL_IDLE) return;
    
    switch (gs->swing.state) {
        case SWING_STATE_READY:
            /* Click 1: Start power gauge oscillation */
            gs->swing.state = SWING_STATE_POWER;
            gs->swing.cursor_pos = 0;
            gs->swing.moving_up = true;
            gs->golfer.state = GOLFER_STATE_SWINGING;
            break;
            
        case SWING_STATE_POWER:
            /* Click 2: Lock shot power */
            gs->swing.locked_power = gs->swing.cursor_pos;
            if (gs->swing.locked_power < TO_FIXED(1) / 5) {
                gs->swing.locked_power = TO_FIXED(1) / 5;
            }
            gs->swing.state = SWING_STATE_ACCURACY;
            gs->swing.moving_up = false;
            break;
            
        case SWING_STATE_ACCURACY:
            /* Click 3: Lock accuracy & strike ball */
            gs->swing.locked_accuracy = gs->swing.cursor_pos - gs->swing.sweetspot_center;
            gs->swing.state = SWING_STATE_RESOLVED;
            gs->golfer.state = GOLFER_STATE_WATCHING;
            
            {
                const ClubData *club = golf_get_club_data(gs->current_club);
                gs->strokes++;
                gs->prev_ball_gx = FROM_FIXED(gs->ball.x);
                gs->prev_ball_gy = FROM_FIXED(gs->ball.y);
                
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
                    fixed roll_speed = FMUL(FMUL(club->launch_speed_h, pwr), dist_scale);
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
    if (gs->mode == MODE_PLAY && gs->ball.state == BALL_IDLE) {
        if (gs->swing.state == SWING_STATE_POWER) {
            gs->swing.cursor_pos += gs->swing.cursor_speed;
            if (gs->swing.cursor_pos >= FIXED_ONE) {
                gs->swing.cursor_pos = FIXED_ONE;
                gs->swing.locked_power = FIXED_ONE;
                gs->swing.state = SWING_STATE_ACCURACY;
                gs->swing.moving_up = false;
            }
        } else if (gs->swing.state == SWING_STATE_ACCURACY) {
            gs->swing.cursor_pos -= (gs->swing.cursor_speed * 11) / 10;
            if (gs->swing.cursor_pos <= 0) {
                gs->swing.cursor_pos = 0;
                golf_swing_click(gs);
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
        if (bgx >= 0 && bgx < GRID_W && bgy >= 0 && bgy < GRID_H) {
            uint8_t feat = gs->grid[bgx][bgy].feature;
            fixed ground_z = TO_FIXED(gs->grid[bgx][bgy].elevation * HEIGHT_STEP_PX);
            
            if (feat == FEAT_PINE || feat == FEAT_OAK || feat == FEAT_PALM || feat == FEAT_FLOWERING) {
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
            if (gs->ball.current_surf == SURF_GREEN || yds < 15) {
                gs->current_club = CLUB_PT;
            } else if (yds > 210) {
                gs->current_club = CLUB_1W;
            } else if (yds > 165) {
                gs->current_club = CLUB_3W;
            } else if (yds > 120) {
                gs->current_club = CLUB_5I;
            } else if (yds > 70) {
                gs->current_club = CLUB_9I;
            } else {
                gs->current_club = CLUB_PW;
            }
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
        fixed walk_spd = 3500;
        
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
            fixed c_spd = 3600;
            
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
    
    /* 7. Gopher Entity Simulation (Caddyshack Hole Hazards) */
    if (gs->mode == MODE_PLAY) {
        if (gs->gopher.state == GOPHER_STATE_HIDDEN) {
            if (gs->gopher.pop_cooldown > 0) {
                gs->gopher.pop_cooldown--;
            }
            if (gs->gopher.pop_cooldown <= 0) {
                /* Find a suitable turf tile along the course corridor */
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
                    gs->audio_event = AUDIO_EVT_GOPHER_CHUCKLE;
                    
                    /* Leave a permanent gopher hole hazard at this spot */
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
                } else {
                    gs->gopher.pop_cooldown = 120;
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
            gs->gopher.anim_frame = 2 + ((gs->gopher.timer / 14) % 2); /* Dance left/right */
            if (gs->gopher.timer >= 180) { /* 3 seconds of dancing/chuckle */
                gs->gopher.state = GOPHER_STATE_RETREATING;
                gs->gopher.timer = 0;
            }
        } else if (gs->gopher.state == GOPHER_STATE_RETREATING) {
            gs->gopher.timer++;
            gs->gopher.anim_frame = 1 - (gs->gopher.timer / 10);
            if (gs->gopher.timer >= 20) {
                gs->gopher.state = GOPHER_STATE_HIDDEN;
                gs->gopher.visible = false;
                gs->gopher.timer = 0;
                gs->gopher.pop_cooldown = 420 + (core_rand() % 480); /* 7..15 seconds cooldown */
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
