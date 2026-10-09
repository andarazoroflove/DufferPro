#include "golf_core.h"
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
    { "DRIVER 1W",  "1W", 240, 31457,  23592,  19660, 60293, false }, /* 14 deg */
    { "3-WOOD 3W",  "3W", 215, 28180,  27525,  26214, 58982, false }, /* 18 deg */
    { "5-IRON 5I",  "5I", 175, 23592,  34078,  36044, 55705, false }, /* 28 deg */
    { "9-IRON 9I",  "9I", 130, 18350,  43253,  45875, 52428, false }, /* 40 deg */
    { "WEDGE PW",   "PW",  90, 13762,  52428,  55705, 49152, false }, /* 50 deg */
    { "PUTTER PT",  "PT",  25,  6553,      0,      0, 62259, true  }  /* Ground roll */
};

const ClubData* golf_get_club_data(ClubType club) {
    if (club >= CLUB_COUNT) club = CLUB_1W;
    return &s_clubs[club];
}

/* Simple fast LCG PRNG for determinism */
static uint32_t s_rng_state = 123456789;
static uint32_t core_rand(void) {
    s_rng_state = s_rng_state * 1103515245 + 12345;
    return (s_rng_state >> 16) & 0x7FFF;
}
static void core_srand(uint32_t seed) {
    s_rng_state = seed ? seed : 123456789;
}

/* Physics parameters in 16.16 fixed point */
#define GRAVITY_FIXED       4500    /* ~0.068 pixels/tick^2 downward */
#define AIR_DRAG_FIXED      65208   /* ~0.995 */
#define WIND_FACTOR_FIXED     120   /* Wind velocity perturbation */
#define SLOPE_ACCEL_FIXED     350   /* Roll acceleration down elevation gradient */
#define STOP_SPEED_THRESHOLD 1200   /* Threshold to stop rolling */
#define CUP_RADIUS_FIXED    22000   /* Cup capture radius (~0.33 tiles) */
#define CUP_MAX_SPEED       18000   /* Maximum entry speed to sink */

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
            
            /* Cardinal edges in diamond isometric:
             * NW: (x, y-1), NE: (x+1, y), SE: (x, y+1), SW: (x-1, y) */
            uint8_t nw_surf = (y > 0) ? gs->grid[x][y - 1].surface : surf;
            uint8_t ne_surf = (x < GRID_W - 1) ? gs->grid[x + 1][y].surface : surf;
            uint8_t se_surf = (y < GRID_H - 1) ? gs->grid[x][y + 1].surface : surf;
            uint8_t sw_surf = (x > 0) ? gs->grid[x - 1][y].surface : surf;
            
            if (surf == SURF_FAIRWAY || surf == SURF_TEE) {
                /* Fairway connects with Fairway, Green, or Tee */
                if (nw_surf == SURF_FAIRWAY || nw_surf == SURF_GREEN || nw_surf == SURF_TEE) mask |= MASK_NW;
                if (ne_surf == SURF_FAIRWAY || ne_surf == SURF_GREEN || ne_surf == SURF_TEE) mask |= MASK_NE;
                if (se_surf == SURF_FAIRWAY || se_surf == SURF_GREEN || se_surf == SURF_TEE) mask |= MASK_SE;
                if (sw_surf == SURF_FAIRWAY || sw_surf == SURF_GREEN || sw_surf == SURF_TEE) mask |= MASK_SW;
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
            } else if (surf == SURF_WATER) {
                if (nw_surf == SURF_WATER) mask |= MASK_NW;
                if (ne_surf == SURF_WATER) mask |= MASK_NE;
                if (se_surf == SURF_WATER) mask |= MASK_SE;
                if (sw_surf == SURF_WATER) mask |= MASK_SW;
            } else {
                mask = 15;
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
    
    int32_t dx = gs->pin_gx - gs->tee_gx;
    int32_t dy = gs->pin_gy - gs->tee_gy;
    fixed fdx = TO_FIXED(dx);
    fixed fdy = TO_FIXED(dy);
    fixed dist_tiles = fixed_sqrt(FMUL(fdx, fdx) + FMUL(fdy, fdy));
    gs->hole_yards = FROM_FIXED(FMUL(dist_tiles, TO_FIXED(YARDS_PER_TILE)));
    
    if (gs->hole_yards < 225) {
        gs->par = 3;
    } else if (gs->hole_yards < 450) {
        gs->par = 4;
    } else {
        gs->par = 5;
    }
}

static void set_banner(GolfState *gs, const char *msg, int32_t ticks) {
    strncpy(gs->banner_text, msg, sizeof(gs->banner_text) - 1);
    gs->banner_text[sizeof(gs->banner_text) - 1] = '\0';
    gs->banner_timer = ticks;
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
    gs->ball.current_surf = (SurfaceType)gs->grid[gs->tee_gx][gs->tee_gy].surface;
    gs->ball.bounce_count = 0;
    gs->ball.air_ticks = 0;
    
    gs->prev_ball_gx = gs->tee_gx;
    gs->prev_ball_gy = gs->tee_gy;
    gs->strokes = 0;
    gs->hole_completed = false;
    
    /* Turn aim toward pin */
    if (gs->has_pin) {
        fixed dx = (TO_FIXED(gs->pin_gx) + FIXED_HALF) - gs->ball.x;
        fixed dy = (TO_FIXED(gs->pin_gy) + FIXED_HALF) - gs->ball.y;
        gs->aim_angle = fixed_atan2(dy, dx);
    } else {
        gs->aim_angle = 192; /* North */
    }
    
    /* Recommend club */
    int32_t yds = golf_distance_to_pin_yards(gs);
    if (yds > 210) gs->current_club = CLUB_1W;
    else if (yds > 165) gs->current_club = CLUB_3W;
    else if (yds > 120) gs->current_club = CLUB_5I;
    else if (yds > 70) gs->current_club = CLUB_9I;
    else gs->current_club = CLUB_PW;
}

/* Procedural terrain generation using integer noise and cellular smoothing */
void golf_generate_terrain(GolfState *gs, uint32_t seed) {
    core_srand(seed);
    
    /* Step 1: Low-resolution height lattice (9x17) */
    #define LAT_W 9
    #define LAT_H 17
    int32_t lattice[LAT_W][LAT_H];
    
    for (int ly = 0; ly < LAT_H; ++ly) {
        for (int lx = 0; lx < LAT_W; ++lx) {
            /* Create natural basin towards bottom-left or center */
            int32_t val = (core_rand() % 100);
            
            /* Gradient: higher ground towards north/east, water/basin towards south/west */
            int32_t bias = (lx * 25) / LAT_W + ((LAT_H - ly) * 20) / LAT_H;
            val = (val * 6 + bias * 4) / 10;
            lattice[lx][ly] = val;
        }
    }
    
    /* Step 2: Bilinear interpolation across 32x64 grid */
    int32_t temp_height[GRID_W][GRID_H];
    for (int y = 0; y < GRID_H; ++y) {
        for (int x = 0; x < GRID_W; ++x) {
            int32_t lx = (x * (LAT_W - 1)) / (GRID_W - 1);
            int32_t ly = (y * (LAT_H - 1)) / (GRID_H - 1);
            int32_t fx = ((x * (LAT_W - 1)) % (GRID_W - 1)) * 256 / (GRID_W - 1);
            int32_t fy = ((y * (LAT_H - 1)) % (GRID_H - 1)) * 256 / (GRID_H - 1);
            
            int32_t v00 = lattice[lx][ly];
            int32_t v10 = (lx + 1 < LAT_W) ? lattice[lx + 1][ly] : v00;
            int32_t v01 = (ly + 1 < LAT_H) ? lattice[lx][ly + 1] : v00;
            int32_t v11 = (lx + 1 < LAT_W && ly + 1 < LAT_H) ? lattice[lx + 1][ly + 1] : v00;
            
            int32_t top = v00 + ((v10 - v00) * fx) / 256;
            int32_t bot = v01 + ((v11 - v01) * fx) / 256;
            int32_t h = top + ((bot - top) * fy) / 256;
            temp_height[x][y] = h;
        }
    }
    
    /* Step 3: Cellular smoothing filter (3 passes) */
    for (int pass = 0; pass < 3; ++pass) {
        int32_t smoothed[GRID_W][GRID_H];
        for (int y = 0; y < GRID_H; ++y) {
            for (int x = 0; x < GRID_W; ++x) {
                int32_t sum = 0;
                int32_t count = 0;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx >= 0 && nx < GRID_W && ny >= 0 && ny < GRID_H) {
                            sum += temp_height[nx][ny];
                            count++;
                        }
                    }
                }
                smoothed[x][y] = sum / count;
            }
        }
        memcpy(temp_height, smoothed, sizeof(temp_height));
    }
    
    /* Step 4: Quantize into 4 discrete elevation tiers */
    for (int y = 0; y < GRID_H; ++y) {
        for (int x = 0; x < GRID_W; ++x) {
            int32_t h = temp_height[x][y];
            uint8_t elev;
            uint8_t surf;
            
            if (h < 26) {
                elev = ELEV_WATER;
                surf = SURF_WATER;
            } else if (h < 56) {
                elev = ELEV_LOWLAND;
                surf = SURF_ROUGH;
            } else if (h < 78) {
                elev = ELEV_RIDGE;
                surf = SURF_ROUGH;
            } else {
                elev = ELEV_PLATEAU;
                surf = SURF_ROUGH;
            }
            
            gs->grid[x][y].elevation = elev;
            gs->grid[x][y].surface = surf;
            gs->grid[x][y].feature = FEAT_NONE;
            gs->grid[x][y].variant = (uint8_t)(core_rand() % 4);
        }
    }
    
    /* Step 5: Natural features (trees & rocks) on rough areas & slopes */
    for (int y = 2; y < GRID_H - 2; ++y) {
        for (int x = 2; x < GRID_W - 2; ++x) {
            if (gs->grid[x][y].surface != SURF_ROUGH) continue;
            
            /* Clustered feature generation */
            int roll = core_rand() % 100;
            if (roll < 9) {
                /* Pine cluster or Oak cluster */
                gs->grid[x][y].feature = (core_rand() % 2 == 0) ? FEAT_PINE : FEAT_OAK;
            } else if (roll == 10) {
                /* Granite boulder */
                gs->grid[x][y].feature = FEAT_ROCK;
            }
        }
    }
    
    gs->has_tee = false;
    gs->has_pin = false;
    golf_update_autotile_masks(gs);
}

/* Places a scenic, well-sculpted default golf hole routing */
void golf_place_default_routing(GolfState *gs) {
    /* Find dry land for Tee near south (gy ~ 54..58, gx ~ 12..16) */
    int tee_x = 14, tee_y = 56;
    while (tee_y > 40 && gs->grid[tee_x][tee_y].elevation == ELEV_WATER) {
        tee_y--;
    }
    
    /* Find dry land for Pin near north (gy ~ 8..12, gx ~ 16..20) */
    int pin_x = 18, pin_y = 10;
    while (pin_y < 30 && gs->grid[pin_x][pin_y].elevation == ELEV_WATER) {
        pin_y++;
    }
    
    gs->tee_gx = tee_x;
    gs->tee_gy = tee_y;
    gs->has_tee = true;
    gs->grid[tee_x][tee_y].surface = SURF_TEE;
    gs->grid[tee_x][tee_y].feature = FEAT_NONE;
    if (tee_x > 0) gs->grid[tee_x - 1][tee_y].surface = SURF_TEE;
    
    gs->pin_gx = pin_x;
    gs->pin_gy = pin_y;
    gs->has_pin = true;
    
    /* Green around pin (3x3 circular) */
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
            int gx = pin_x + dx;
            int gy = pin_y + dy;
            if (gx >= 0 && gx < GRID_W && gy >= 0 && gy < GRID_H) {
                if (dx * dx + dy * dy <= 4 && gs->grid[gx][gy].elevation > ELEV_WATER) {
                    gs->grid[gx][gy].surface = SURF_GREEN;
                    gs->grid[gx][gy].feature = FEAT_NONE;
                }
            }
        }
    }
    
    /* Fairway connecting Tee to Green in a natural sweeping curve */
    int steps = 100;
    for (int i = 0; i <= steps; ++i) {
        int cx = tee_x + ((pin_x - tee_x) * i) / steps;
        int cy = tee_y + ((pin_y - tee_y) * i) / steps;
        
        /* Slight dogleg curve */
        if (i > 25 && i < 75) {
            cx += (i < 50) ? -2 : -1;
        }
        
        /* Paint fairway ribbon (radius 2) */
        for (int rdy = -2; rdy <= 2; ++rdy) {
            for (int rdx = -2; rdx <= 2; ++rdx) {
                int fx = cx + rdx;
                int fy = cy + rdy;
                if (fx >= 0 && fx < GRID_W && fy >= 0 && fy < GRID_H) {
                    if (gs->grid[fx][fy].surface == SURF_ROUGH) {
                        gs->grid[fx][fy].surface = SURF_FAIRWAY;
                        gs->grid[fx][fy].feature = FEAT_NONE;
                    }
                }
            }
        }
    }
    
    /* Strategic Sand Bunkers */
    int b1x = pin_x - 3, b1y = pin_y + 1;
    if (b1x >= 0 && gs->grid[b1x][b1y].elevation > ELEV_WATER) {
        gs->grid[b1x][b1y].surface = SURF_SAND;
        gs->grid[b1x][b1y].feature = FEAT_NONE;
        if (b1y + 1 < GRID_H) gs->grid[b1x][b1y + 1].surface = SURF_SAND;
    }
    int b2x = tee_x + 3, b2y = (tee_y + pin_y) / 2;
    if (b2x < GRID_W && gs->grid[b2x][b2y].elevation > ELEV_WATER) {
        gs->grid[b2x][b2y].surface = SURF_SAND;
        gs->grid[b2x][b2y].feature = FEAT_NONE;
    }
    
    golf_update_autotile_masks(gs);
    golf_recalc_hole_stats(gs);
    golf_reset_ball_to_tee(gs);
    set_banner(gs, "DUFFER - READY TO TEE OFF", 180);
}

void golf_init(GolfState *gs) {
    memset(gs, 0, sizeof(GolfState));
    gs->mode = MODE_SPLASH;
    gs->current_tool = TOOL_FAIRWAY;
    gs->brush_size = 1;
    gs->current_club = CLUB_1W;
    gs->wind.angle = 48;
    gs->wind.speed = TO_FIXED(8); /* 8 mph */
    
    /* Gauge settings */
    gs->swing.state = SWING_STATE_READY;
    gs->swing.cursor_pos = 0;
    gs->swing.cursor_speed = 1800; /* ~0.027 per tick */
    gs->swing.moving_up = true;
    gs->swing.sweetspot_center = 0;
    gs->swing.sweetspot_width = 7500;
    
    golf_generate_terrain(gs, 42);
    golf_place_default_routing(gs);
}

/* Tool execution in Build Mode */
bool golf_build_apply_tool(GolfState *gs, int32_t gx, int32_t gy) {
    if (gx < 0 || gx >= GRID_W || gy < 0 || gy >= GRID_H) return false;
    
    if (gs->current_tool == TOOL_TEE) {
        if (gs->grid[gx][gy].elevation == ELEV_WATER) return false;
        if (gs->has_tee) {
            gs->grid[gs->tee_gx][gs->tee_gy].surface = SURF_ROUGH;
        }
        gs->tee_gx = gx;
        gs->tee_gy = gy;
        gs->has_tee = true;
        gs->grid[gx][gy].surface = SURF_TEE;
        gs->grid[gx][gy].feature = FEAT_NONE;
        golf_recalc_hole_stats(gs);
        golf_update_autotile_masks(gs);
        golf_reset_ball_to_tee(gs);
        set_banner(gs, "TEE MARKER PLACED", 120);
        return true;
    }
    
    if (gs->current_tool == TOOL_PIN) {
        if (gs->grid[gx][gy].elevation == ELEV_WATER) return false;
        gs->pin_gx = gx;
        gs->pin_gy = gy;
        gs->has_pin = true;
        gs->grid[gx][gy].surface = SURF_GREEN;
        gs->grid[gx][gy].feature = FEAT_NONE;
        
        /* Auto create green ring around pin */
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                int nx = gx + dx;
                int ny = gy + dy;
                if (nx >= 0 && nx < GRID_W && ny >= 0 && ny < GRID_H) {
                    if (gs->grid[nx][ny].elevation > ELEV_WATER && gs->grid[nx][ny].surface != SURF_TEE) {
                        gs->grid[nx][ny].surface = SURF_GREEN;
                        gs->grid[nx][ny].feature = FEAT_NONE;
                    }
                }
            }
        }
        golf_recalc_hole_stats(gs);
        golf_update_autotile_masks(gs);
        set_banner(gs, "PIN CUP PLACED", 120);
        return true;
    }
    
    int b = gs->brush_size;
    for (int dy = -(b - 1); dy <= (b - 1); ++dy) {
        for (int dx = -(b - 1); dx <= (b - 1); ++dx) {
            int tx = gx + dx;
            int ty = gy + dy;
            if (tx < 0 || tx >= GRID_W || ty < 0 || ty >= GRID_H) continue;
            
            switch (gs->current_tool) {
                case TOOL_FAIRWAY:
                    if (gs->grid[tx][ty].elevation > ELEV_WATER) {
                        gs->grid[tx][ty].surface = SURF_FAIRWAY;
                        gs->grid[tx][ty].feature = FEAT_NONE;
                    }
                    break;
                case TOOL_GREEN:
                    if (gs->grid[tx][ty].elevation > ELEV_WATER) {
                        gs->grid[tx][ty].surface = SURF_GREEN;
                        gs->grid[tx][ty].feature = FEAT_NONE;
                    }
                    break;
                case TOOL_SAND:
                    if (gs->grid[tx][ty].elevation > ELEV_WATER) {
                        gs->grid[tx][ty].surface = SURF_SAND;
                        gs->grid[tx][ty].feature = FEAT_NONE;
                    }
                    break;
                case TOOL_ROUGH:
                    if (gs->grid[tx][ty].elevation > ELEV_WATER) {
                        gs->grid[tx][ty].surface = SURF_ROUGH;
                    }
                    break;
                case TOOL_PINE:
                    if (gs->grid[tx][ty].elevation > ELEV_WATER && gs->grid[tx][ty].surface != SURF_GREEN && gs->grid[tx][ty].surface != SURF_TEE) {
                        gs->grid[tx][ty].feature = FEAT_PINE;
                    }
                    break;
                case TOOL_OAK:
                    if (gs->grid[tx][ty].elevation > ELEV_WATER && gs->grid[tx][ty].surface != SURF_GREEN && gs->grid[tx][ty].surface != SURF_TEE) {
                        gs->grid[tx][ty].feature = FEAT_OAK;
                    }
                    break;
                case TOOL_ROCK:
                    if (gs->grid[tx][ty].elevation > ELEV_WATER && gs->grid[tx][ty].surface != SURF_GREEN && gs->grid[tx][ty].surface != SURF_TEE) {
                        gs->grid[tx][ty].feature = FEAT_ROCK;
                    }
                    break;
                case TOOL_BULLDOZE:
                    if (gs->grid[tx][ty].elevation > ELEV_WATER) {
                        gs->grid[tx][ty].surface = SURF_ROUGH;
                        gs->grid[tx][ty].feature = FEAT_NONE;
                    }
                    break;
                default:
                    break;
            }
        }
    }
    
    golf_update_autotile_masks(gs);
    return true;
}

void golf_set_tool(GolfState *gs, BuildTool tool) {
    if (tool < TOOL_COUNT) {
        gs->current_tool = tool;
    }
}

void golf_set_mode(GolfState *gs, GameMode mode) {
    gs->mode = mode;
    if (mode == MODE_PLAY) {
        set_banner(gs, "PLAY MODE - HIT [SPACE] OR CLICK TO SWING", 120);
    } else {
        set_banner(gs, "BUILD MODE - SCULPT HOLE ROUTING", 120);
    }
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
}

/* 3-Click Swing Gauge handler */
void golf_swing_click(GolfState *gs) {
    if (gs->mode != MODE_PLAY || gs->ball.state != BALL_IDLE || gs->hole_completed) {
        return;
    }
    
    switch (gs->swing.state) {
        case SWING_STATE_READY:
            /* Click 1: Start power needle */
            gs->swing.state = SWING_STATE_POWER;
            gs->swing.cursor_pos = 0;
            gs->swing.moving_up = true;
            break;
            
        case SWING_STATE_POWER:
            /* Click 2: Lock power, start needle downward towards snap mark */
            gs->swing.locked_power = gs->swing.cursor_pos;
            if (gs->swing.locked_power < TO_FIXED(1) / 10) {
                gs->swing.locked_power = TO_FIXED(1) / 10; /* Min 10% */
            }
            gs->swing.state = SWING_STATE_ACCURACY;
            gs->swing.moving_up = false;
            break;
            
        case SWING_STATE_ACCURACY:
            /* Click 3: Lock snap/accuracy */
            gs->swing.locked_accuracy = gs->swing.cursor_pos - gs->swing.sweetspot_center;
            gs->swing.state = SWING_STATE_RESOLVED;
            
            /* Execute ball strike! */
            {
                const ClubData *club = golf_get_club_data(gs->current_club);
                gs->strokes++;
                gs->prev_ball_gx = FROM_FIXED(gs->ball.x);
                gs->prev_ball_gy = FROM_FIXED(gs->ball.y);
                
                fixed pwr = gs->swing.locked_power;
                fixed acc = gs->swing.locked_accuracy;
                
                /* Slice/Hook deviation in angle (up to ~20 brads) */
                int32_t angle_dev = FROM_FIXED(FMUL(acc, TO_FIXED(24)));
                uint8_t final_angle = (uint8_t)(gs->aim_angle + angle_dev);
                
                if (club->is_putter) {
                    /* Ground roll */
                    fixed roll_speed = FMUL(club->launch_speed, pwr);
                    gs->ball.vx = FMUL(roll_speed, fixed_cos(final_angle));
                    gs->ball.vy = FMUL(roll_speed, fixed_sin(final_angle));
                    gs->ball.vz = 0;
                    gs->ball.state = BALL_ROLLING;
                } else {
                    /* Ballistic flight */
                    fixed launch_spd = FMUL(club->launch_speed, pwr);
                    fixed horiz_spd = FMUL(launch_spd, fixed_cos((uint8_t)(club->launch_angle >> 10)));
                    fixed vert_spd  = FMUL(launch_spd, fixed_sin((uint8_t)(club->launch_angle >> 10)));
                    
                    gs->ball.vx = FMUL(horiz_spd, fixed_cos(final_angle));
                    gs->ball.vy = FMUL(horiz_spd, fixed_sin(final_angle));
                    gs->ball.vz = vert_spd;
                    gs->ball.state = BALL_IN_FLIGHT;
                    gs->ball.air_ticks = 0;
                    gs->ball.bounce_count = 0;
                }
                
                if (FABS(acc) < gs->swing.sweetspot_width) {
                    set_banner(gs, "SWEET SPOT! CLEAN STRIKE!", 90);
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

/* Physics and Simulation Tick */
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
                /* Auto-lock at 100% and reverse */
                gs->swing.locked_power = FIXED_ONE;
                gs->swing.state = SWING_STATE_ACCURACY;
                gs->swing.moving_up = false;
            }
        } else if (gs->swing.state == SWING_STATE_ACCURACY) {
            gs->swing.cursor_pos -= (gs->swing.cursor_speed * 5) / 4;
            /* If needle drops way past snap, miss-hit */
            if (gs->swing.cursor_pos <= -TO_FIXED(1) / 3) {
                gs->swing.cursor_pos = -TO_FIXED(1) / 3;
                golf_swing_click(gs); /* Force miss-hit lock */
            }
        }
    }
    
    /* 2. Ball Simulation */
    if (gs->ball.state == BALL_IN_FLIGHT) {
        gs->ball.air_ticks++;
        
        /* Gravity */
        gs->ball.vz -= GRAVITY_FIXED;
        
        /* Wind force */
        fixed wx = FMUL(gs->wind.speed, fixed_cos(gs->wind.angle));
        fixed wy = FMUL(gs->wind.speed, fixed_sin(gs->wind.angle));
        gs->ball.vx += FMUL(wx, WIND_FACTOR_FIXED);
        gs->ball.vy += FMUL(wy, WIND_FACTOR_FIXED);
        
        /* Air drag */
        gs->ball.vx = FMUL(gs->ball.vx, AIR_DRAG_FIXED);
        gs->ball.vy = FMUL(gs->ball.vy, AIR_DRAG_FIXED);
        gs->ball.vz = FMUL(gs->ball.vz, AIR_DRAG_FIXED);
        
        /* Integrate position */
        gs->ball.x += gs->ball.vx;
        gs->ball.y += gs->ball.vy;
        gs->ball.z += gs->ball.vz;
        
        /* Tree canopy collision check */
        int32_t bgx = FROM_FIXED(gs->ball.x);
        int32_t bgy = FROM_FIXED(gs->ball.y);
        if (bgx >= 0 && bgx < GRID_W && bgy >= 0 && bgy < GRID_H) {
            uint8_t feat = gs->grid[bgx][bgy].feature;
            if (feat == FEAT_PINE || feat == FEAT_OAK) {
                fixed ground_z = TO_FIXED(gs->grid[bgx][bgy].elevation * HEIGHT_STEP_PX);
                if (gs->ball.z >= ground_z + TO_FIXED(4) && gs->ball.z <= ground_z + TO_FIXED(36)) {
                    /* Struck tree canopy! Deflect downward and lose velocity */
                    gs->ball.vz = -FABS(gs->ball.vz) / 2 - TO_FIXED(1);
                    gs->ball.vx = gs->ball.vx / 3;
                    gs->ball.vy = gs->ball.vy / 3;
                    set_banner(gs, "THWACK! STRUCK TREE BRANCHES!", 90);
                }
            } else if (feat == FEAT_ROCK) {
                fixed ground_z = TO_FIXED(gs->grid[bgx][bgy].elevation * HEIGHT_STEP_PX);
                if (gs->ball.z <= ground_z + TO_FIXED(10)) {
                    /* Struck boulder! Chaotic bounce */
                    gs->ball.vz = FABS(gs->ball.vz) + TO_FIXED(2);
                    gs->ball.vx = -gs->ball.vx / 2;
                    gs->ball.vy = -gs->ball.vy / 2;
                    set_banner(gs, "CLANG! STRUCK BOULDER!", 90);
                }
            }
        }
        
        /* Ground check */
        fixed gz = golf_get_ground_z_fixed(gs, gs->ball.x, gs->ball.y);
        gs->ball.ground_z = gz;
        
        if (gs->ball.z <= gz) {
            gs->ball.z = gz;
            
            /* Check water hazard */
            SurfaceType surf = golf_get_surface_at(gs, gs->ball.x, gs->ball.y);
            if (surf == SURF_WATER) {
                gs->ball.state = BALL_WATER;
                gs->strokes++; /* +1 penalty stroke */
                set_banner(gs, "SPLASH! WATER HAZARD (+1 PENALTY)", 120);
                return;
            }
            
            /* Ground Restitution & Friction */
            fixed rest = 26214;   /* ~0.40 Fairway */
            fixed fric = 60293;   /* ~0.92 Fairway */
            
            if (surf == SURF_GREEN) {
                rest = 32768; /* 0.50 */
                fric = 62914; /* 0.96 */
            } else if (surf == SURF_ROUGH) {
                rest = 9830;  /* 0.15 */
                fric = 52428; /* 0.80 */
            } else if (surf == SURF_SAND) {
                rest = 3276;  /* 0.05 - Bunker dead thud */
                fric = 36044; /* 0.55 */
            }
            
            /* Reverse vertical velocity */
            gs->ball.vz = -FMUL(gs->ball.vz, rest);
            gs->ball.vx = FMUL(gs->ball.vx, fric);
            gs->ball.vy = FMUL(gs->ball.vy, fric);
            gs->ball.bounce_count++;
            
            if (FABS(gs->ball.vz) < TO_FIXED(1) / 2 || gs->ball.bounce_count > 4) {
                gs->ball.vz = 0;
                gs->ball.state = BALL_ROLLING;
            }
        }
    } else if (gs->ball.state == BALL_ROLLING) {
        /* Rolling on ground */
        SurfaceType surf = golf_get_surface_at(gs, gs->ball.x, gs->ball.y);
        if (surf == SURF_WATER) {
            gs->ball.state = BALL_WATER;
            gs->strokes++;
            set_banner(gs, "SPLASH! ROLLED INTO WATER (+1 PENALTY)", 120);
            return;
        }
        
        fixed fric = 60293; /* 0.92 Fairway */
        if (surf == SURF_GREEN) fric = 63500; /* 0.969 Green */
        else if (surf == SURF_ROUGH) fric = 50000; /* 0.76 Rough */
        else if (surf == SURF_SAND) fric = 32000; /* 0.48 Sand */
        
        /* Apply friction */
        gs->ball.vx = FMUL(gs->ball.vx, fric);
        gs->ball.vy = FMUL(gs->ball.vy, fric);
        
        /* Slope vector acceleration down elevation tiers */
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
        gs->ball.ground_z = golf_get_ground_z_fixed(gs, gs->ball.x, gs->ball.y);
        gs->ball.z = gs->ball.ground_z;
        
        /* Cup sinking detection */
        if (gs->has_pin) {
            fixed pin_x = TO_FIXED(gs->pin_gx) + FIXED_HALF;
            fixed pin_y = TO_FIXED(gs->pin_gy) + FIXED_HALF;
            fixed dx = pin_x - gs->ball.x;
            fixed dy = pin_y - gs->ball.y;
            fixed dist = fixed_sqrt(FMUL(dx, dx) + FMUL(dy, dy));
            
            if (dist < CUP_RADIUS_FIXED) {
                fixed speed = fixed_sqrt(FMUL(gs->ball.vx, gs->ball.vx) + FMUL(gs->ball.vy, gs->ball.vy));
                if (speed < CUP_MAX_SPEED) {
                    /* IN THE HOLE! */
                    gs->ball.x = pin_x;
                    gs->ball.y = pin_y;
                    gs->ball.vx = 0;
                    gs->ball.vy = 0;
                    gs->ball.state = BALL_HOLED;
                    gs->hole_completed = true;
                    
                    int32_t rel = gs->strokes - gs->par;
                    if (gs->strokes == 1) {
                        set_banner(gs, "*** HOLE IN ONE! LEGENDARY! ***", 300);
                    } else if (rel <= -3) {
                        set_banner(gs, "*** ALBATROSS! INCREDIBLE! ***", 300);
                    } else if (rel == -2) {
                        set_banner(gs, "*** EAGLE! MAGNIFICENT! ***", 300);
                    } else if (rel == -1) {
                        set_banner(gs, "** BIRDIE! EXCELLENT SHOT! **", 300);
                    } else if (rel == 0) {
                        set_banner(gs, "* PAR! SOLID GOLF! *", 240);
                    } else if (rel == 1) {
                        set_banner(gs, "BOGEY.", 240);
                    } else {
                        set_banner(gs, "DOUBLE BOGEY (+).", 240);
                    }
                    return;
                } else {
                    /* Lip out! Deflect away */
                    set_banner(gs, "LIPPED OUT!", 60);
                }
            }
        }
        
        /* Check stop threshold */
        fixed speed = fixed_sqrt(FMUL(gs->ball.vx, gs->ball.vx) + FMUL(gs->ball.vy, gs->ball.vy));
        if (speed < STOP_SPEED_THRESHOLD) {
            gs->ball.vx = 0;
            gs->ball.vy = 0;
            gs->ball.state = BALL_IDLE;
            gs->ball.current_surf = golf_get_surface_at(gs, gs->ball.x, gs->ball.y);
            
            /* Ready for next shot: turn aim towards pin */
            if (gs->has_pin) {
                fixed pin_x = TO_FIXED(gs->pin_gx) + FIXED_HALF;
                fixed pin_y = TO_FIXED(gs->pin_gy) + FIXED_HALF;
                gs->aim_angle = fixed_atan2(pin_y - gs->ball.y, pin_x - gs->ball.x);
            }
            
            /* Reset swing gauge */
            gs->swing.state = SWING_STATE_READY;
            gs->swing.cursor_pos = 0;
            
            /* Recommend club */
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
        /* Drop ball back to previous lie */
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
        gs->swing.cursor_pos = 0;
        
        if (gs->has_pin) {
            fixed pin_x = TO_FIXED(gs->pin_gx) + FIXED_HALF;
            fixed pin_y = TO_FIXED(gs->pin_gy) + FIXED_HALF;
            gs->aim_angle = fixed_atan2(pin_y - gs->ball.y, pin_x - gs->ball.x);
        }
    }
}

