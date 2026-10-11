#include "gba_render.h"
#include "font8x8.h"
#include <string.h>

/* Fast pseudo-random for procedural turf detail */
static uint32_t s_gba_rand = 1234567;
static inline uint32_t gba_rand(void) {
    s_gba_rand = s_gba_rand * 1103515245 + 12345;
    return (s_gba_rand >> 16) & 0x7FFF;
}

void gba_draw_rect(uint16_t *fb, int x, int y, int w, int h, uint16_t color) {
    for (int cy = y; cy < y + h; ++cy) {
        if (cy < 0 || cy >= GBA_SCREEN_H) continue;
        for (int cx = x; cx < x + w; ++cx) {
            if (cx < 0 || cx >= GBA_SCREEN_W) continue;
            fb[cy * GBA_SCREEN_W + cx] = color;
        }
    }
}

void gba_draw_text(uint16_t *fb, int x, int y, const char *str, uint16_t color, bool shadow) {
    if (!str) return;
    int cur_x = x;
    while (*str) {
        char ch = *str++;
        if (ch >= 32 && ch <= 126) {
            const uint8_t *bitmap = g_font8x8[(int)(ch - 32)];
            for (int r = 0; r < 8; ++r) {
                int py = y + r;
                if (py < 0 || py >= GBA_SCREEN_H) continue;
                uint8_t row = bitmap[r];
                for (int c = 0; c < 8; ++c) {
                    int px = cur_x + c;
                    if (px < 0 || px >= GBA_SCREEN_W) continue;
                    if (row & (0x80 >> c)) {
                        if (shadow && py + 1 < GBA_SCREEN_H && px + 1 < GBA_SCREEN_W) {
                            fb[(py + 1) * GBA_SCREEN_W + (px + 1)] = GBA_BLACK;
                        }
                        fb[py * GBA_SCREEN_W + px] = color;
                    }
                }
            }
        }
        cur_x += 8;
    }
}

void gba_render_init(GbaRenderContext *grc) {
    grc->cam_x = 0;
    grc->cam_y = 0;
    grc->target_cam_x = 0;
    grc->target_cam_y = 0;
    grc->tick_count = 0;
    grc->look_mode = false;
    grc->look_cam_x = 0;
    grc->look_cam_y = 0;
    grc->look_center_gx = 0;
    grc->look_center_gy = 0;
    grc->look_dist_yds = 0;
}

/* ========================================================================= */
/* GBA 16-BIT ISOMETRIC TILE RENDERING                                       */
/* ========================================================================= */

static void draw_gba_diamond(uint16_t *fb, int sx, int sy, SurfaceType surf, int gx, int gy, int anim_t) {
    uint16_t base_col, alt_col;
    
    switch (surf) {
        case SURF_WATER: {
            int wave = ((anim_t / 12) + gx + gy) & 3;
            base_col = (wave == 0) ? RGB15(6, 16, 28) : RGB15(4, 13, 25);
            alt_col  = RGB15(12, 22, 31); /* Specular ripple */
            break;
        }
        case SURF_ROUGH:
            base_col = ((gx + gy) & 1) ? RGB15(3, 14, 4) : RGB15(4, 16, 5);
            alt_col  = RGB15(2, 11, 3);
            break;
        case SURF_FAIRWAY:
            base_col = ((gx + gy) & 2) ? RGB15(6, 25, 7) : RGB15(8, 27, 9);
            alt_col  = RGB15(7, 26, 8);
            break;
        case SURF_GREEN:
            base_col = RGB15(9, 29, 11);
            alt_col  = RGB15(11, 30, 13);
            break;
        case SURF_SAND:
            base_col = RGB15(29, 26, 16);
            alt_col  = RGB15(26, 22, 13);
            break;
        case SURF_TEE:
            base_col = RGB15(10, 28, 10);
            alt_col  = RGB15(31, 31, 31); /* Tee markers */
            break;
        case SURF_CARTPATH:
            base_col = RGB15(22, 20, 18);
            alt_col  = RGB15(18, 17, 15);
            break;
        case SURF_BRIDGE:
            base_col = RGB15(18, 12, 7);
            alt_col  = RGB15(13, 8, 4);
            break;
        default:
            base_col = RGB15(4, 16, 5);
            alt_col  = RGB15(3, 13, 4);
            break;
    }

    /* Plot 32x16 isometric diamond scanlines */
    for (int dy = 0; dy < GBA_TILE_H; ++dy) {
        int py = sy + dy;
        if (py < 0 || py >= GBA_SCREEN_H) continue;
        
        int span = (dy < GBA_TILE_HALF_H) ? (dy * 4 + 4) : ((GBA_TILE_H - 1 - dy) * 4 + 4);
        if (span > GBA_TILE_W) span = GBA_TILE_W;
        int px_start = sx + GBA_TILE_HALF_W - (span / 2);
        
        for (int dx = 0; dx < span; ++dx) {
            int px = px_start + dx;
            if (px < 0 || px >= GBA_SCREEN_W) continue;
            
            uint16_t c = base_col;
            if (surf == SURF_WATER && (dx == span / 2 && ((dy + (anim_t / 8)) & 3) == 0)) {
                c = alt_col; /* Water shimmer */
            } else if (surf == SURF_SAND && ((dx ^ dy) & 3) == 0) {
                c = alt_col; /* Grain texture */
            }
            fb[py * GBA_SCREEN_W + px] = c;
        }
    }
}

/* Vertical Cliff Facade for 3D Elevation Steps */
static void draw_gba_cliff(uint16_t *fb, int sx, int sy, int elev_drop) {
    if (elev_drop <= 0) return;
    int h = elev_drop * GBA_HEIGHT_STEP;
    if (h > 40) h = 40;
    
    for (int dy = 0; dy < h; ++dy) {
        int py = sy + dy;
        if (py < 0 || py >= GBA_SCREEN_H) continue;
        
        for (int dx = 0; dx < GBA_TILE_W; ++dx) {
            int px = sx + dx;
            if (px < 0 || px >= GBA_SCREEN_W) continue;
            
            uint16_t cliff_col = ((dx + dy) & 2) ? RGB15(11, 11, 12) : RGB15(8, 8, 9);
            fb[py * GBA_SCREEN_W + px] = cliff_col;
        }
    }
}

/* ========================================================================= */
/* GBA DRAWABLE ENTITIES & CHARACTERS                                        */
/* ========================================================================= */

typedef enum {
    GBA_ENT_FLAG = 0,
    GBA_ENT_TREE,
    GBA_ENT_GOLFER,
    GBA_ENT_CADDY,
    GBA_ENT_BALL,
    GBA_ENT_BEV_CART,
    GBA_ENT_GOPHER,
    GBA_ENT_CARL,
    GBA_ENT_TNT_BOX,
    GBA_ENT_CLUBHOUSE
} GbaEntType;

typedef struct {
    int32_t sort_y;
    GbaEntType type;
    int sx, sy;
    int data;
} GbaDrawable;

#define MAX_GBA_DRAWABLES 128
static GbaDrawable s_drawables[MAX_GBA_DRAWABLES];
static int s_drawable_count = 0;

static void add_drawable(GbaEntType type, int sx, int sy, int sort_y, int data) {
    if (s_drawable_count >= MAX_GBA_DRAWABLES) return;
    s_drawables[s_drawable_count].type = type;
    s_drawables[s_drawable_count].sx = sx;
    s_drawables[s_drawable_count].sy = sy;
    s_drawables[s_drawable_count].sort_y = sort_y;
    s_drawables[s_drawable_count].data = data;
    s_drawable_count++;
}

/* Simple bubble sort for drawables */
static void sort_drawables(void) {
    for (int i = 0; i < s_drawable_count - 1; ++i) {
        for (int j = i + 1; j < s_drawable_count; ++j) {
            if (s_drawables[i].sort_y > s_drawables[j].sort_y) {
                GbaDrawable tmp = s_drawables[i];
                s_drawables[i] = s_drawables[j];
                s_drawables[j] = tmp;
            }
        }
    }
}

/* ========================================================================= */
/* PROCEDURAL GBA SPRITE DRAWING (Mode 3 Direct Blitting)                    */
/* ========================================================================= */

/* 1. Flagstick on Green */
static void draw_gba_flag(uint16_t *fb, int sx, int sy) {
    /* Pole */
    for (int y = -20; y <= 0; ++y) {
        int py = sy + y;
        if (py >= 0 && py < GBA_SCREEN_H && sx >= 0 && sx < GBA_SCREEN_W) {
            fb[py * GBA_SCREEN_W + sx] = RGB15(31, 31, 31);
        }
    }
    /* Red Pennant */
    for (int y = -20; y <= -13; ++y) {
        int w = 8 - (y + 20);
        for (int x = 1; x <= w; ++x) {
            int px = sx + x;
            int py = sy + y;
            if (px >= 0 && px < GBA_SCREEN_W && py >= 0 && py < GBA_SCREEN_H) {
                fb[py * GBA_SCREEN_W + px] = GBA_RED;
            }
        }
    }
}

/* 2. Pine / Scenery Tree */
static void draw_gba_tree(uint16_t *fb, int sx, int sy, int tree_type) {
    /* Trunk */
    for (int y = -8; y <= 0; ++y) {
        int py = sy + y;
        if (py >= 0 && py < GBA_SCREEN_H) {
            for (int x = -1; x <= 1; ++x) {
                int px = sx + x;
                if (px >= 0 && px < GBA_SCREEN_W) fb[py * GBA_SCREEN_W + px] = GBA_BROWN;
            }
        }
    }
    /* Foliage Canopy */
    uint16_t leaf_col = (tree_type == 4) ? RGB15(31, 20, 24) : /* Cherry blossom */
                        ((tree_type == 1) ? RGB15(2, 14, 4) : RGB15(4, 20, 5));
    
    for (int layer = 0; layer < 3; ++layer) {
        int ly = sy - 8 - layer * 7;
        int rad = 9 - layer * 2;
        for (int dy = -rad; dy <= 2; ++dy) {
            int py = ly + dy;
            if (py < 0 || py >= GBA_SCREEN_H) continue;
            for (int dx = -rad; dx <= rad; ++dx) {
                if (dx * dx + dy * dy <= rad * rad) {
                    int px = sx + dx;
                    if (px >= 0 && px < GBA_SCREEN_W) {
                        fb[py * GBA_SCREEN_W + px] = (dx > 0) ? leaf_col : (leaf_col + RGB15(2, 4, 1));
                    }
                }
            }
        }
    }
}

/* 3. Golfer */
static void draw_gba_golfer(uint16_t *fb, int sx, int sy, int state, int swing_pct) {
    /* Shadow */
    for (int dx = -4; dx <= 4; ++dx) {
        int px = sx + dx;
        if (px >= 0 && px < GBA_SCREEN_W && sy >= 0 && sy < GBA_SCREEN_H) {
            fb[sy * GBA_SCREEN_W + px] = GBA_BLACK;
        }
    }
    
    /* White Tour Cap */
    gba_draw_rect(fb, sx - 2, sy - 18, 5, 2, GBA_WHITE);
    /* Face */
    gba_draw_rect(fb, sx - 2, sy - 16, 4, 3, RGB15(28, 22, 17));
    /* Red Athletic Polo */
    gba_draw_rect(fb, sx - 3, sy - 13, 6, 6, RGB15(28, 5, 5));
    /* Khaki Shorts */
    gba_draw_rect(fb, sx - 2, sy - 7, 5, 4, RGB15(24, 20, 14));
    /* Shoes */
    gba_draw_rect(fb, sx - 3, sy - 3, 2, 3, GBA_WHITE);
    gba_draw_rect(fb, sx + 1, sy - 3, 2, 3, GBA_WHITE);
    
    /* Golf Club */
    if (state == GOLFER_STATE_SWINGING) {
        int club_angle = (swing_pct * 180) / 10000;
        int cx = sx + 4 + (club_angle < 90 ? (club_angle / 15) : (12 - club_angle / 15));
        int cy = sy - 10 - (club_angle / 15);
        gba_draw_rect(fb, cx, cy, 2, 8, RGB15(26, 26, 28));
    } else {
        gba_draw_rect(fb, sx + 3, sy - 12, 1, 11, RGB15(26, 26, 28));
        gba_draw_rect(fb, sx + 3, sy - 2, 3, 2, GBA_DARK_GRAY);
    }
}

/* 4. Caddy with Golf Bag */
static void draw_gba_caddy(uint16_t *fb, int sx, int sy) {
    /* Shadow */
    gba_draw_rect(fb, sx - 3, sy, 6, 1, GBA_BLACK);
    /* White Caddy Bib */
    gba_draw_rect(fb, sx - 2, sy - 16, 4, 2, GBA_WHITE);
    gba_draw_rect(fb, sx - 2, sy - 14, 4, 3, RGB15(28, 22, 17));
    gba_draw_rect(fb, sx - 3, sy - 11, 6, 6, GBA_WHITE);
    /* Green Number "1" on Caddy Bib */
    gba_draw_rect(fb, sx, sy - 9, 1, 3, GBA_GREEN);
    /* Trousers */
    gba_draw_rect(fb, sx - 2, sy - 5, 5, 4, RGB15(6, 8, 12));
    /* Heavy Canvas Golf Bag slung on shoulder */
    gba_draw_rect(fb, sx + 3, sy - 12, 4, 10, RGB15(18, 10, 5));
    /* Club Heads poking out of bag */
    gba_draw_rect(fb, sx + 4, sy - 15, 2, 3, RGB15(26, 26, 28));
}

/* 5. Golf Ball */
static void draw_gba_ball(uint16_t *fb, int sx, int sy, int ball_z_px) {
    /* Drop shadow on turf */
    if (sx >= 0 && sx < GBA_SCREEN_W && sy >= 0 && sy < GBA_SCREEN_H) {
        fb[sy * GBA_SCREEN_W + sx] = GBA_BLACK;
    }
    /* Elevated Ball */
    int by = sy - ball_z_px;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int px = sx + dx;
            int py = by + dy;
            if (px >= 0 && px < GBA_SCREEN_W && py >= 0 && py < GBA_SCREEN_H) {
                fb[py * GBA_SCREEN_W + px] = (dx == -1 && dy == -1) ? GBA_WHITE : RGB15(26, 26, 28);
            }
        }
    }
}

/* 6. Caddyshack Gopher */
static void draw_gba_gopher(uint16_t *fb, int sx, int sy, int anim_f) {
    /* Dirt Mound */
    gba_draw_rect(fb, sx - 6, sy - 2, 12, 4, RGB15(14, 9, 5));
    gba_draw_rect(fb, sx - 3, sy - 1, 6, 2, GBA_BLACK); /* Hole pit */
    
    int gy = sy - 6;
    if (anim_f == 2) gy -= 2; /* Dancing hop */
    
    /* Furry Brown Head */
    gba_draw_rect(fb, sx - 3, gy - 6, 6, 6, RGB15(20, 12, 6));
    /* Pink Ears */
    gba_draw_rect(fb, sx - 4, gy - 7, 2, 2, RGB15(30, 20, 22));
    gba_draw_rect(fb, sx + 2, gy - 7, 2, 2, RGB15(30, 20, 22));
    /* Black Beady Eyes */
    gba_draw_rect(fb, sx - 2, gy - 4, 1, 1, GBA_BLACK);
    gba_draw_rect(fb, sx + 1, gy - 4, 1, 1, GBA_BLACK);
    /* White Buck Teeth! */
    gba_draw_rect(fb, sx - 1, gy - 2, 2, 2, GBA_WHITE);
    
    /* Waving Paws */
    if (anim_f == 2) {
        gba_draw_rect(fb, sx - 5, gy - 5, 2, 2, RGB15(26, 18, 12));
        gba_draw_rect(fb, sx + 3, gy - 3, 2, 2, RGB15(26, 18, 12));
    }
}

/* 7. Carl Spackler (Floppy Bucket Hat, Work Vest, TNT Plunger / Hose) */
static void draw_gba_carl(uint16_t *fb, int sx, int sy, const CarlEntity *carl) {
    /* Shadow */
    gba_draw_rect(fb, sx - 4, sy, 8, 1, GBA_BLACK);
    
    /* Floppy Olive Bucket Hat */
    gba_draw_rect(fb, sx - 4, sy - 18, 8, 2, RGB15(12, 14, 9));
    gba_draw_rect(fb, sx - 3, sy - 20, 6, 3, RGB15(14, 16, 10));
    
    /* Scruffy Face & Stubble */
    gba_draw_rect(fb, sx - 2, sy - 16, 5, 3, RGB15(26, 19, 14));
    gba_draw_rect(fb, sx - 2, sy - 14, 5, 2, RGB15(18, 15, 12)); /* 5 o'clock shadow */
    
    /* Utility Work Vest */
    gba_draw_rect(fb, sx - 3, sy - 12, 7, 6, RGB15(10, 13, 8));
    gba_draw_rect(fb, sx - 1, sy - 12, 3, 6, RGB15(24, 23, 21)); /* Grubby tee */
    
    /* Muddy Work Pants */
    gba_draw_rect(fb, sx - 3, sy - 6, 6, 5, RGB15(12, 10, 8));
    /* Heavy Work Boots */
    gba_draw_rect(fb, sx - 3, sy - 1, 3, 2, RGB15(6, 5, 4));
    gba_draw_rect(fb, sx + 1, sy - 1, 3, 2, RGB15(6, 5, 4));
    
    /* State Details */
    if (carl->state == CARL_STATE_CELEBRATING) {
        /* Raised arms in victory: Cinderella Story! */
        gba_draw_rect(fb, sx - 5, sy - 19, 2, 6, RGB15(10, 13, 8));
        gba_draw_rect(fb, sx + 4, sy - 19, 2, 6, RGB15(10, 13, 8));
        gba_draw_text(fb, sx - 20, sy - 30, "CINDERELLA!", GBA_YELLOW, true);
    } else if (carl->state == CARL_STATE_DYNAMITE_PLUNGE) {
        /* Pushing plunger */
        gba_draw_text(fb, sx - 16, sy - 28, "DIE GOPHER!", GBA_RED, true);
    } else if (carl->state == CARL_STATE_HOSE_SPRAY) {
        /* Green Garden Hose & Brass Nozzle */
        gba_draw_rect(fb, sx + 4, sy - 10, 4, 3, RGB15(26, 22, 6)); /* Brass Nozzle */
        gba_draw_rect(fb, sx - 5, sy - 6, 3, 5, RGB15(4, 16, 6));   /* Green Hose */
    }
}

/* 8. Carl's TNT Detonator Box */
static void draw_gba_tnt_box(uint16_t *fb, int sx, int sy, bool plunged) {
    /* Wooden Blasting Machine */
    gba_draw_rect(fb, sx - 3, sy - 6, 6, 6, RGB15(18, 10, 5));
    /* Red TNT label */
    gba_draw_rect(fb, sx - 2, sy - 4, 4, 2, GBA_RED);
    /* T-Handle */
    if (plunged) {
        gba_draw_rect(fb, sx - 2, sy - 7, 4, 1, RGB15(10, 6, 3));
    } else {
        gba_draw_rect(fb, sx, sy - 9, 1, 3, RGB15(26, 22, 8)); /* Brass rod */
        gba_draw_rect(fb, sx - 2, sy - 10, 4, 1, RGB15(10, 6, 3)); /* T-bar up */
    }
}

/* ========================================================================= */
/* MAIN GBA FRAME RENDERER                                                   */
/* ========================================================================= */

void gba_render_draw_frame(GbaRenderContext *grc, const GolfState *gs) {
    uint16_t *fb = gba_get_backbuffer();
    grc->tick_count++;
    
    /* 1. Camera Tracking (Focus smoothly on ball or look-mode target) */
    int32_t ball_gx = FROM_FIXED(gs->ball.x);
    int32_t ball_gy = FROM_FIXED(gs->ball.y);
    int32_t b_elev = gs->grid[ball_gx][ball_gy].elevation;
    
    if (grc->look_mode) {
        grc->target_cam_x = grc->look_cam_x;
        grc->target_cam_y = grc->look_cam_y;
    } else {
        int32_t target_sx = (ball_gx - ball_gy) * GBA_TILE_HALF_W;
        int32_t target_sy = (ball_gx + ball_gy) * GBA_TILE_HALF_H - (b_elev * GBA_HEIGHT_STEP);
        grc->target_cam_x = (GBA_SCREEN_W / 2) - target_sx;
        grc->target_cam_y = (GBA_SCREEN_H / 2) - target_sy;
    }
    
    /* Smooth camera interpolation or instant snap on initial frame */
    if (grc->tick_count <= 1 || (grc->cam_x == 0 && grc->cam_y == 0)) {
        grc->cam_x = grc->target_cam_x;
        grc->cam_y = grc->target_cam_y;
    } else {
        grc->cam_x += (grc->target_cam_x - grc->cam_x) / 4;
        grc->cam_y += (grc->target_cam_y - grc->cam_y) / 4;
    }

    /* In Look Mode: Compute tile at screen center (120, 80) and yards from ball */
    if (grc->look_mode) {
        int32_t u = (104 - grc->cam_x) / 16;
        int32_t v = (72 - grc->cam_y) / 8;
        int32_t cgx = (u + v) / 2;
        int32_t cgy = (v - u) / 2;
        if (cgx < 0) cgx = 0; if (cgx >= GRID_W) cgx = GRID_W - 1;
        if (cgy < 0) cgy = 0; if (cgy >= GRID_H) cgy = GRID_H - 1;
        grc->look_center_gx = cgx;
        grc->look_center_gy = cgy;
        
        int32_t dx = cgx - ball_gx;
        int32_t dy = cgy - ball_gy;
        grc->look_dist_yds = (int32_t)(fixed_sqrt(TO_FIXED(dx * dx + dy * dy)) / 65536 * YARDS_PER_TILE);
        if (grc->look_dist_yds < 1) grc->look_dist_yds = 1;
    }
    
    /* Clear Screen with Fairway Green / Sky Gradient */
    for (int y = 0; y < GBA_SCREEN_H; ++y) {
        uint16_t bg_col = (y < 20) ? RGB15(2, 6, 12) : RGB15(3, 10, 4);
        for (int x = 0; x < GBA_SCREEN_W; ++x) {
            fb[y * GBA_SCREEN_W + x] = bg_col;
        }
    }
    
    /* 2. Render Isometric Diamond Terrain */
    s_drawable_count = 0;
    
    for (int gy = 0; gy < GRID_H; ++gy) {
        for (int gx = 0; gx < GRID_W; ++gx) {
            int elev = gs->grid[gx][gy].elevation;
            int sx = (gx - gy) * GBA_TILE_HALF_W + grc->cam_x;
            int sy = (gx + gy) * GBA_TILE_HALF_H - (elev * GBA_HEIGHT_STEP) + grc->cam_y;
            
            /* Viewport Culling (32x16 tile) */
            if (sx < -32 || sx > GBA_SCREEN_W + 32 || sy < -32 || sy > GBA_SCREEN_H + 32) {
                continue;
            }
            
            SurfaceType surf = gs->grid[gx][gy].surface;
            draw_gba_diamond(fb, sx, sy, surf, gx, gy, grc->tick_count);
            
            /* Elevation cliff face */
            if (gx + 1 < GRID_W && gy + 1 < GRID_H) {
                int adj_elev = gs->grid[gx + 1][gy + 1].elevation;
                if (elev > adj_elev) {
                    draw_gba_cliff(fb, sx, sy + GBA_TILE_H, elev - adj_elev);
                }
            }
            
            /* Collect Drawables on this tile */
            int center_x = sx + GBA_TILE_HALF_W;
            int center_y = sy + GBA_TILE_HALF_H;
            int depth_key = (gx + gy) * 32;
            
            /* Flag */
            if (gx == gs->pin_gx && gy == gs->pin_gy) {
                add_drawable(GBA_ENT_FLAG, center_x, center_y, depth_key + 12, 0);
            }
            
            /* Trees / Scenery */
            FeatureType feat = gs->grid[gx][gy].feature;
            if (feat != FEAT_NONE && feat != FEAT_CLUBHOUSE) {
                add_drawable(GBA_ENT_TREE, center_x, center_y, depth_key + 14, (int)feat);
            }
        }
    }
    
    /* 3. Add Dynamic Entities (Golfer, Caddy, Ball, Gopher, Carl) */
    int golfer_gx = FROM_FIXED(gs->golfer.x);
    int golfer_gy = FROM_FIXED(gs->golfer.y);
    int g_sx = (golfer_gx - golfer_gy) * GBA_TILE_HALF_W + grc->cam_x + GBA_TILE_HALF_W;
    int g_sy = (golfer_gx + golfer_gy) * GBA_TILE_HALF_H - (gs->grid[golfer_gx][golfer_gy].elevation * GBA_HEIGHT_STEP) + grc->cam_y + GBA_TILE_HALF_H;
    add_drawable(GBA_ENT_GOLFER, g_sx, g_sy, (golfer_gx + golfer_gy) * 32 + 16, gs->golfer.state);
    
    /* Caddy stands just behind golfer */
    add_drawable(GBA_ENT_CADDY, g_sx - 10, g_sy - 3, (golfer_gx + golfer_gy) * 32 + 15, 0);
    
    /* Ball */
    int b_sx = (ball_gx - ball_gy) * GBA_TILE_HALF_W + grc->cam_x + GBA_TILE_HALF_W;
    int b_sy = (ball_gx + ball_gy) * GBA_TILE_HALF_H - (b_elev * GBA_HEIGHT_STEP) + grc->cam_y + GBA_TILE_HALF_H;
    int ball_z_px = FROM_FIXED(gs->ball.z - gs->ball.ground_z);
    add_drawable(GBA_ENT_BALL, b_sx, b_sy, (ball_gx + ball_gy) * 32 + 18, ball_z_px);
    
    /* Gopher */
    if (gs->gopher.visible) {
        int goph_gx = gs->gopher.gx;
        int goph_gy = gs->gopher.gy;
        int gp_sx = (goph_gx - goph_gy) * GBA_TILE_HALF_W + grc->cam_x + GBA_TILE_HALF_W;
        int gp_sy = (goph_gx + goph_gy) * GBA_TILE_HALF_H - (gs->grid[goph_gx][goph_gy].elevation * GBA_HEIGHT_STEP) + grc->cam_y + GBA_TILE_HALF_H;
        add_drawable(GBA_ENT_GOPHER, gp_sx, gp_sy, (goph_gx + goph_gy) * 32 + 13, gs->gopher.anim_frame);
    }
    
    /* Carl Spackler */
    if (gs->carl.active) {
        int cgx = FROM_FIXED(gs->carl.x);
        int cgy = FROM_FIXED(gs->carl.y);
        int c_sx = (cgx - cgy) * GBA_TILE_HALF_W + grc->cam_x + GBA_TILE_HALF_W;
        int c_sy = (cgx + cgy) * GBA_TILE_HALF_H - (gs->grid[cgx][cgy].elevation * GBA_HEIGHT_STEP) + grc->cam_y + GBA_TILE_HALF_H;
        add_drawable(GBA_ENT_CARL, c_sx, c_sy, (cgx + cgy) * 32 + 17, 0);
        
        /* TNT Plunger box */
        if (gs->carl.attack_type == CARL_ATTACK_DYNAMITE &&
            (gs->carl.state == CARL_STATE_DYNAMITE_PLANT || gs->carl.state == CARL_STATE_DYNAMITE_PLUNGE || gs->carl.state == CARL_STATE_CELEBRATING)) {
            int pgx = FROM_FIXED(gs->carl.plunger_x);
            int pgy = FROM_FIXED(gs->carl.plunger_y);
            int p_sx = (pgx - pgy) * GBA_TILE_HALF_W + grc->cam_x + GBA_TILE_HALF_W;
            int p_sy = (pgx + pgy) * GBA_TILE_HALF_H - (gs->grid[pgx][pgy].elevation * GBA_HEIGHT_STEP) + grc->cam_y + GBA_TILE_HALF_H;
            bool plunged = (gs->carl.state == CARL_STATE_DYNAMITE_PLUNGE && gs->carl.timer >= 14) || gs->carl.state == CARL_STATE_CELEBRATING;
            add_drawable(GBA_ENT_TNT_BOX, p_sx, p_sy, (pgx + pgy) * 32 + 15, plunged ? 1 : 0);
        }
    }
    
    /* 4. Depth-Sort & Render Drawables */
    sort_drawables();
    for (int i = 0; i < s_drawable_count; ++i) {
        const GbaDrawable *d = &s_drawables[i];
        switch (d->type) {
            case GBA_ENT_FLAG:
                draw_gba_flag(fb, d->sx, d->sy);
                break;
            case GBA_ENT_TREE:
                draw_gba_tree(fb, d->sx, d->sy, d->data);
                break;
            case GBA_ENT_GOLFER:
                draw_gba_golfer(fb, d->sx, d->sy, d->data, gs->swing.cursor_pos);
                break;
            case GBA_ENT_CADDY:
                draw_gba_caddy(fb, d->sx, d->sy);
                break;
            case GBA_ENT_BALL:
                draw_gba_ball(fb, d->sx, d->sy, d->data);
                break;
            case GBA_ENT_GOPHER:
                draw_gba_gopher(fb, d->sx, d->sy, d->data);
                break;
            case GBA_ENT_CARL:
                draw_gba_carl(fb, d->sx, d->sy, &gs->carl);
                break;
            case GBA_ENT_TNT_BOX:
                draw_gba_tnt_box(fb, d->sx, d->sy, d->data != 0);
                break;
            default:
                break;
        }
    }
    
    /* 5. GBA Visual Effects (Explosion Blast or Water Jet Stream) */
    if (gs->carl.active && gs->carl.state == CARL_STATE_DYNAMITE_PLUNGE && gs->carl.timer >= 14 && gs->carl.timer <= 38) {
        /* Detonation Fireball over hole */
        int tgx = gs->carl.target_gx;
        int tgy = gs->carl.target_gy;
        int h_sx = (tgx - tgy) * GBA_TILE_HALF_W + grc->cam_x + GBA_TILE_HALF_W;
        int h_sy = (tgx + tgy) * GBA_TILE_HALF_H - (gs->grid[tgx][tgy].elevation * GBA_HEIGHT_STEP) + grc->cam_y + GBA_TILE_HALF_H - 8;
        int rad = (gs->carl.timer - 14) * 2;
        if (rad > 16) rad = 16;
        for (int dy = -rad; dy <= rad; ++dy) {
            for (int dx = -rad; dx <= rad; ++dx) {
                if (dx * dx + dy * dy <= rad * rad) {
                    int px = h_sx + dx;
                    int py = h_sy + dy;
                    if (px >= 0 && px < GBA_SCREEN_W && py >= 0 && py < GBA_SCREEN_H) {
                        fb[py * GBA_SCREEN_W + px] = ((dx * dx + dy * dy) < (rad * rad) / 3) ? GBA_YELLOW : GBA_ORANGE;
                    }
                }
            }
        }
    } else if (gs->carl.active && gs->carl.state == CARL_STATE_HOSE_SPRAY) {
        /* Water Jet stream from Carl to Burrow */
        int cgx = FROM_FIXED(gs->carl.x);
        int cgy = FROM_FIXED(gs->carl.y);
        int c_sx = (cgx - cgy) * GBA_TILE_HALF_W + grc->cam_x + GBA_TILE_HALF_W + 4;
        int c_sy = (cgx + cgy) * GBA_TILE_HALF_H - (gs->grid[cgx][cgy].elevation * GBA_HEIGHT_STEP) + grc->cam_y + GBA_TILE_HALF_H - 10;
        int tgx = gs->carl.target_gx;
        int tgy = gs->carl.target_gy;
        int h_sx = (tgx - tgy) * GBA_TILE_HALF_W + grc->cam_x + GBA_TILE_HALF_W;
        int h_sy = (tgx + tgy) * GBA_TILE_HALF_H - (gs->grid[tgx][tgy].elevation * GBA_HEIGHT_STEP) + grc->cam_y + GBA_TILE_HALF_H;
        
        for (int s = 0; s <= 16; ++s) {
            int wx = c_sx + (s * (h_sx - c_sx)) / 16;
            int wy = c_sy + (s * (h_sy - c_sy)) / 16 - (s * (16 - s)) / 16;
            if (wx >= 0 && wx < GBA_SCREEN_W && wy >= 0 && wy < GBA_SCREEN_H) {
                fb[wy * GBA_SCREEN_W + wx] = (s & 1) ? GBA_WHITE : GBA_SKY_BLUE;
            }
        }
    }
    
    /* 5.5 Render Aim Reticle & Dotted Trajectory */
    if (grc->look_mode) {
        int cx = GBA_SCREEN_W / 2;
        int cy = GBA_SCREEN_H / 2;
        
        /* Dotted flight line from ball to screen center */
        for (int step = 0; step <= 16; ++step) {
            if ((step & 1) == 0) {
                int lx = b_sx + (step * (cx - b_sx)) / 16;
                int ly = b_sy + (step * (cy - b_sy)) / 16;
                if (lx >= 0 && lx < GBA_SCREEN_W && ly >= 14 && ly < 138) {
                    fb[ly * GBA_SCREEN_W + lx] = GBA_GOLD;
                }
            }
        }
        
        /* Pulsing Crosshair Reticle at Screen Center */
        uint16_t reticle_col = (grc->tick_count & 8) ? GBA_YELLOW : GBA_WHITE;
        gba_draw_rect(fb, cx - 6, cy, 4, 1, reticle_col);
        gba_draw_rect(fb, cx + 3, cy, 4, 1, reticle_col);
        gba_draw_rect(fb, cx, cy - 6, 1, 4, reticle_col);
        gba_draw_rect(fb, cx, cy + 3, 1, 4, reticle_col);
        fb[cy * GBA_SCREEN_W + cx] = GBA_RED;
        fb[(cy - 3) * GBA_SCREEN_W + (cx - 3)] = reticle_col;
        fb[(cy - 3) * GBA_SCREEN_W + (cx + 3)] = reticle_col;
        fb[(cy + 3) * GBA_SCREEN_W + (cx - 3)] = reticle_col;
        fb[(cy + 3) * GBA_SCREEN_W + (cx + 3)] = reticle_col;
    } else if (gs->has_custom_aim) {
        /* Draw locked aim ring on terrain */
        int tgx = FROM_FIXED(gs->aim_target_x);
        int tgy = FROM_FIXED(gs->aim_target_y);
        if (tgx >= 0 && tgx < GRID_W && tgy >= 0 && tgy < GRID_H) {
            int t_elev = gs->grid[tgx][tgy].elevation;
            int tx = (tgx - tgy) * GBA_TILE_HALF_W + grc->cam_x + GBA_TILE_HALF_W;
            int ty = (tgx + tgy) * GBA_TILE_HALF_H - (t_elev * GBA_HEIGHT_STEP) + grc->cam_y + GBA_TILE_HALF_H;
            if (tx >= 4 && tx < GBA_SCREEN_W - 4 && ty >= 16 && ty < 136) {
                gba_draw_rect(fb, tx - 3, ty, 7, 1, GBA_GOLD);
                gba_draw_rect(fb, tx, ty - 3, 1, 7, GBA_GOLD);
                fb[ty * GBA_SCREEN_W + tx] = GBA_RED;
            }
        }
    }
    
    /* 6. In-Game HUD: Top Status Bar (240x14) */
    gba_draw_rect(fb, 0, 0, GBA_SCREEN_W, 13, RGB15(2, 4, 10));
    const char *club_name = "1W";
    if (gs->current_club == CLUB_3W) club_name = "3W";
    else if (gs->current_club == CLUB_5I) club_name = "5I";
    else if (gs->current_club == CLUB_9I) club_name = "9I";
    else if (gs->current_club == CLUB_PW) club_name = "PW";
    else if (gs->current_club == CLUB_PT) club_name = "PT";
    
    /* Header Mode / Hole Info */
    if (grc->look_mode) {
        gba_draw_text(fb, 4, 3, "LOOK", GBA_GOLD, false);
    } else {
        gba_draw_text(fb, 4, 3, "H1 PAR4", GBA_WHITE, false);
    }
    
    /* Club */
    gba_draw_text(fb, 68, 3, club_name, GBA_GOLD, false);
    
    /* Distance (Aim Target Distance in Look Mode, or Pin Distance) */
    int pin_dist = grc->look_mode ? grc->look_dist_yds :
        (int)(fixed_sqrt(FMUL(gs->ball.x - TO_FIXED(gs->pin_gx), gs->ball.x - TO_FIXED(gs->pin_gx)) +
                         FMUL(gs->ball.y - TO_FIXED(gs->pin_gy), gs->ball.y - TO_FIXED(gs->pin_gy))) / 65536 * 3);
    if (pin_dist < 1) pin_dist = 1;
    
    char dist_str[16];
    dist_str[0] = '0' + (pin_dist / 100) % 10;
    dist_str[1] = '0' + (pin_dist / 10) % 10;
    dist_str[2] = '0' + (pin_dist % 10);
    dist_str[3] = 'Y'; dist_str[4] = 'D'; dist_str[5] = '\0';
    gba_draw_text(fb, 100, 3, dist_str, GBA_WHITE, false);
    
    /* Right indicator: Surface in Look mode, or Wind */
    if (grc->look_mode) {
        SurfaceType st = gs->grid[grc->look_center_gx][grc->look_center_gy].surface;
        const char *st_name = "ROUGH  ";
        uint16_t st_col = GBA_LIGHT_GREEN;
        if (st == SURF_FAIRWAY) { st_name = "FAIRWAY"; st_col = GBA_GREEN; }
        else if (st == SURF_GREEN) { st_name = "GREEN  "; st_col = GBA_LIGHT_GREEN; }
        else if (st == SURF_SAND) { st_name = "SAND   "; st_col = GBA_SAND_YELLOW; }
        else if (st == SURF_WATER) { st_name = "WATER  "; st_col = GBA_SKY_BLUE; }
        gba_draw_text(fb, 160, 3, st_name, st_col, false);
    } else {
        gba_draw_text(fb, 160, 3, "WIND 12M", GBA_SKY_BLUE, false);
    }
    
    /* 7. Bottom Swing Meter / Look HUD (240x22) */
    gba_draw_rect(fb, 0, 138, GBA_SCREEN_W, 22, RGB15(2, 4, 10));
    
    if (grc->look_mode) {
        gba_draw_text(fb, 10, 141, "[A] SET AIM   [D-PAD] MOVE", GBA_GOLD, false);
        gba_draw_text(fb, 10, 151, "[SELECT] RETURN TO GOLFER", GBA_WHITE, false);
    } else if (gs->banner_timer > 0 && gs->banner_text[0]) {
        /* Comic Event Banner (e.g. "CINDERELLA STORY!") */
        gba_draw_text(fb, 8, 145, gs->banner_text, GBA_YELLOW, true);
    } else {
        /* Classic 3-Click Swing Gauge */
        int meter_x = 24;
        int meter_y = 143;
        int meter_w = 192;
        int meter_h = 8;
        
        /* Gauge Border & Track */
        gba_draw_rect(fb, meter_x - 1, meter_y - 1, meter_w + 2, meter_h + 2, RGB15(16, 18, 22));
        gba_draw_rect(fb, meter_x, meter_y, meter_w, meter_h, RGB15(4, 6, 8));
        
        /* Sweetspot Tick */
        int sweet_x = meter_x + (gs->swing.sweetspot_center * meter_w) / 65536;
        gba_draw_rect(fb, sweet_x - 2, meter_y, 4, meter_h, GBA_GOLD);
        
        /* Cursor Position */
        int cur_x = meter_x + (gs->swing.cursor_pos * meter_w) / 65536;
        if (cur_x < meter_x) cur_x = meter_x;
        if (cur_x > meter_x + meter_w - 2) cur_x = meter_x + meter_w - 2;
        
        /* Power Fill */
        if (gs->swing.state >= SWING_STATE_POWER) {
            int fill_w = cur_x - meter_x;
            if (fill_w > 0) gba_draw_rect(fb, meter_x, meter_y + 1, fill_w, meter_h - 2, RGB15(28, 12, 4));
        }
        gba_draw_rect(fb, cur_x - 1, meter_y - 2, 3, meter_h + 4, GBA_WHITE);
        
        /* Key Prompts */
        gba_draw_text(fb, 12, 152, "[A]SWING  [SEL]LOOK  [L/R]CLUB", GBA_GRAY, false);
    }
}

/* ========================================================================= */
/* GBA TITLE / SPLASH SCREEN (CADDYSHACK EDITION)                            */
/* ========================================================================= */

/* Draw Weathered Caddyshack Building with Caddy Kids */
static void draw_gba_caddyshack_building(uint16_t *fb, int bx, int by) {
    /* 1. Ground Drop Shadow under building */
    gba_draw_rect(fb, bx + 2, by + 46, 80, 5, RGB15(2, 6, 2));

    /* 2. Weathered Cedar Wood Walls (70 x 36 px) */
    for (int y = 14; y <= 45; ++y) {
        int py = by + y;
        if (py < 0 || py >= GBA_SCREEN_H) continue;
        
        bool is_groove = (y % 4 == 0);
        uint16_t plank_col = is_groove ? RGB15(10, 7, 4) : (((y % 8) < 4) ? RGB15(18, 12, 7) : RGB15(15, 10, 6));

        for (int x = 6; x <= 74; ++x) {
            int px = bx + x;
            if (px < 0 || px >= GBA_SCREEN_W) continue;
            
            /* Corner support timber beams */
            if (x <= 10 || x >= 70) {
                fb[py * GBA_SCREEN_W + px] = RGB15(12, 8, 4);
            } else {
                fb[py * GBA_SCREEN_W + px] = plank_col;
            }
        }
    }

    /* 3. Corrugated Galvanized Tin Roof */
    for (int y = 0; y <= 14; ++y) {
        int py = by + y;
        if (py < 0 || py >= GBA_SCREEN_H) continue;

        int x_start = bx + 2 + (14 - y);
        int x_end   = bx + 78 - (14 - y);

        for (int px = x_start; px <= x_end; ++px) {
            if (px < 0 || px >= GBA_SCREEN_W) continue;
            
            /* Vertical corrugated ribs */
            int rib = px % 3;
            uint16_t metal_col = (rib == 0) ? RGB15(22, 23, 25) : ((rib == 1) ? RGB15(17, 18, 20) : RGB15(12, 13, 15));
            fb[py * GBA_SCREEN_W + px] = metal_col;
        }
    }

    /* Roof Overhang Eaves Rafter Shadow */
    gba_draw_rect(fb, bx + 5, by + 14, 70, 2, RGB15(6, 4, 2));

    /* 4. Stacked "CADDY" and "SHACK" Wooden Signs above Doorway */
    gba_draw_rect(fb, bx + 13, by + 16, 42, 8, RGB15(7, 4, 2));
    gba_draw_rect(fb, bx + 14, by + 17, 40, 6, RGB15(16, 11, 6));
    gba_draw_text(fb, bx + 14, by + 16, "CADDY", GBA_GOLD, false);

    gba_draw_rect(fb, bx + 13, by + 24, 42, 8, RGB15(7, 4, 2));
    gba_draw_rect(fb, bx + 14, by + 25, 40, 6, RGB15(16, 11, 6));
    gba_draw_text(fb, bx + 14, by + 24, "SHACK", GBA_GOLD, false);

    /* 5. Open Rustic Doorway & Counter */
    gba_draw_rect(fb, bx + 25, by + 32, 18, 14, RGB15(3, 2, 2)); /* Dark interior */
    gba_draw_rect(fb, bx + 25, by + 37, 18, 2, RGB15(15, 10, 5)); /* Wooden Counter Shelf */
    gba_draw_rect(fb, bx + 27, by + 39, 14, 7, RGB15(12, 8, 4));  /* Lower half-door */

    /* 6. Window with Wooden Shutters (shifted right so zero text overlap) */
    gba_draw_rect(fb, bx + 58, by + 22, 13, 12, RGB15(4, 5, 8));  /* Window glass */
    gba_draw_rect(fb, bx + 57, by + 21, 15, 1, RGB15(16, 11, 6)); /* Frame */
    gba_draw_rect(fb, bx + 57, by + 33, 15, 1, RGB15(16, 11, 6));
    gba_draw_rect(fb, bx + 57, by + 21, 1, 13, RGB15(16, 11, 6));
    gba_draw_rect(fb, bx + 71, by + 21, 1, 13, RGB15(16, 11, 6));
    gba_draw_rect(fb, bx + 63, by + 22, 1, 11, RGB15(14, 9, 5));  /* Mullion */
    gba_draw_rect(fb, bx + 58, by + 27, 13, 1, RGB15(14, 9, 5));

    /* 7. Caddy Kids hanging around in white pants & red/black shirts */
    /* Kid 1: Leaning against doorframe in red shirt & white slacks */
    gba_draw_rect(fb, bx + 18, by + 25, 5, 3, RGB15(28, 4, 4));    /* Red cap */
    gba_draw_rect(fb, bx + 18, by + 28, 5, 4, RGB15(28, 20, 16));  /* Face */
    gba_draw_rect(fb, bx + 17, by + 32, 7, 7, RGB15(26, 4, 4));    /* Red shirt */
    gba_draw_rect(fb, bx + 18, by + 39, 5, 7, GBA_WHITE);           /* White trousers */
    gba_draw_rect(fb, bx + 17, by + 45, 6, 2, RGB15(5, 5, 5));     /* Shoes */

    /* Kid 2: Sitting by wall in black shirt & white slacks with golf bag */
    gba_draw_rect(fb, bx + 72, by + 26, 5, 3, GBA_WHITE);           /* White cap */
    gba_draw_rect(fb, bx + 72, by + 29, 5, 4, RGB15(28, 20, 16));  /* Face */
    gba_draw_rect(fb, bx + 71, by + 33, 7, 7, RGB15(3, 3, 5));     /* Black shirt */
    gba_draw_rect(fb, bx + 72, by + 40, 6, 6, GBA_WHITE);           /* White slacks */
    /* Leather Golf Bag with Clubs */
    gba_draw_rect(fb, bx + 78, by + 32, 5, 14, RGB15(15, 9, 4));   /* Leather bag */
    gba_draw_rect(fb, bx + 79, by + 27, 3, 5, RGB15(28, 28, 30));  /* Irons */
    gba_draw_rect(fb, bx + 78, by + 25, 4, 3, RGB15(28, 4, 4));    /* Red headcover */

    /* 8. Wooden "Bushwood" Sign on Posts */
    gba_draw_rect(fb, bx + 87, by + 34, 2, 14, RGB15(11, 7, 3));   /* Left post */
    gba_draw_rect(fb, bx + 152, by + 34, 2, 14, RGB15(11, 7, 3));  /* Right post */
    gba_draw_rect(fb, bx + 84, by + 24, 73, 11, RGB15(7, 4, 2));   /* Sign frame */
    gba_draw_rect(fb, bx + 85, by + 25, 71, 9, RGB15(17, 12, 7));  /* Timber board */
    gba_draw_text(fb, bx + 88, by + 26, "BUSHWOOD", GBA_GOLD, true);
}

/* Draw Big Animated Dancing Caddyshack Gopher */
static void draw_gba_giant_gopher(uint16_t *fb, int gx, int gy, int anim_f) {
    /* 4-Frame Dancing Hop Beat */
    int hop_y = 0;
    if (anim_f == 1) hop_y = -2;
    else if (anim_f == 3) hop_y = -5; /* Joyful leap in the air! */

    /* 1. Dirt Burrow Mound & Pit */
    for (int dy = -4; dy <= 4; ++dy) {
        int py = gy + 32 + dy;
        if (py < 0 || py >= GBA_SCREEN_H) continue;
        int mw = 18 - dy * dy;
        if (mw < 4) mw = 4;
        for (int dx = -mw; dx <= mw; ++dx) {
            int px = gx + dx;
            if (px < 0 || px >= GBA_SCREEN_W) continue;
            uint16_t d_col = ((dx ^ dy) & 2) ? RGB15(14, 9, 4) : RGB15(11, 7, 3);
            fb[py * GBA_SCREEN_W + px] = d_col;
        }
    }
    gba_draw_rect(fb, gx - 7, gy + 31, 14, 4, GBA_BLACK); /* Burrow hole entrance */

    /* 2. Plump Furry Body */
    int body_top = gy + 14 + hop_y;
    int body_bot = gy + 31 + hop_y;
    for (int py = body_top; py <= body_bot; ++py) {
        if (py < 0 || py >= GBA_SCREEN_H) continue;
        int bw = 9 - (body_bot - py) / 4;
        if (bw < 6) bw = 6;
        for (int px = gx - bw; px <= gx + bw; ++px) {
            if (px < 0 || px >= GBA_SCREEN_W) continue;
            fb[py * GBA_SCREEN_W + px] = RGB15(20, 13, 6);
        }
    }
    /* Cream-colored belly patch */
    gba_draw_rect(fb, gx - 4, body_top + 4, 8, 10, RGB15(28, 23, 17));

    /* 3. Furry Chubby Head */
    int head_top = gy + 2 + hop_y;
    int head_bot = gy + 16 + hop_y;
    for (int py = head_top; py <= head_bot; ++py) {
        if (py < 0 || py >= GBA_SCREEN_H) continue;
        int hw = (py < head_top + 5) ? 7 : 10; /* Chubby cheeks */
        for (int px = gx - hw; px <= gx + hw; ++px) {
            if (px < 0 || px >= GBA_SCREEN_W) continue;
            fb[py * GBA_SCREEN_W + px] = RGB15(22, 14, 7);
        }
    }

    /* 4. Pink Inner Ears */
    /* Left Ear */
    gba_draw_rect(fb, gx - 10, head_top - 4, 4, 6, RGB15(18, 11, 5));
    gba_draw_rect(fb, gx - 9,  head_top - 3, 2, 4, RGB15(31, 18, 20));
    /* Right Ear */
    gba_draw_rect(fb, gx + 7,  head_top - 4, 4, 6, RGB15(18, 11, 5));
    gba_draw_rect(fb, gx + 8,  head_top - 3, 2, 4, RGB15(31, 18, 20));

    /* 5. Glossy Black Eyes with Catchlight */
    /* Left Eye */
    gba_draw_rect(fb, gx - 6, head_top + 4, 3, 4, GBA_BLACK);
    gba_draw_rect(fb, gx - 5, head_top + 4, 1, 1, GBA_WHITE); /* Twinkle */
    /* Right Eye */
    gba_draw_rect(fb, gx + 4, head_top + 4, 3, 4, GBA_BLACK);
    gba_draw_rect(fb, gx + 5, head_top + 4, 1, 1, GBA_WHITE);

    /* 6. Tan Snout & Black Button Nose */
    gba_draw_rect(fb, gx - 5, head_top + 7, 10, 6, RGB15(26, 19, 13)); /* Muzzle */
    gba_draw_rect(fb, gx - 2, head_top + 7, 4, 2, GBA_BLACK);          /* Nose */

    /* Cheerful Whiskers */
    gba_draw_rect(fb, gx - 15, head_top + 8, 5, 1, RGB15(12, 8, 4));
    gba_draw_rect(fb, gx - 15, head_top + 10, 5, 1, RGB15(12, 8, 4));
    gba_draw_rect(fb, gx + 11, head_top + 8, 5, 1, RGB15(12, 8, 4));
    gba_draw_rect(fb, gx + 11, head_top + 10, 5, 1, RGB15(12, 8, 4));

    /* 7. Iconic Big White Buck Teeth! */
    gba_draw_rect(fb, gx - 3, head_top + 12, 3, 4, GBA_WHITE);
    gba_draw_rect(fb, gx + 1, head_top + 12, 3, 4, GBA_WHITE);
    gba_draw_rect(fb, gx,     head_top + 12, 1, 4, GBA_BLACK); /* Gap */

    /* 8. Expressive Dancing Paws (Outstretched and Waving) */
    if (anim_f == 0) {
        /* Left paw raised high waving out */
        gba_draw_rect(fb, gx - 19, head_top + 2, 7, 5, RGB15(22, 14, 7));
        gba_draw_rect(fb, gx - 21, head_top, 4, 4, RGB15(31, 20, 18)); /* Pink paw pad */
        /* Right paw on hip */
        gba_draw_rect(fb, gx + 9,  body_top + 6, 6, 5, RGB15(22, 14, 7));
    } else if (anim_f == 1) {
        /* Both paws extended outward in dance groove */
        gba_draw_rect(fb, gx - 18, body_top + 4, 8, 5, RGB15(22, 14, 7));
        gba_draw_rect(fb, gx - 20, body_top + 3, 4, 4, RGB15(31, 20, 18));
        gba_draw_rect(fb, gx + 10, body_top + 4, 8, 5, RGB15(22, 14, 7));
        gba_draw_rect(fb, gx + 16, body_top + 3, 4, 4, RGB15(31, 20, 18));
    } else if (anim_f == 2) {
        /* Right paw raised high waving out */
        gba_draw_rect(fb, gx + 12, head_top + 2, 7, 5, RGB15(22, 14, 7));
        gba_draw_rect(fb, gx + 17, head_top, 4, 4, RGB15(31, 20, 18)); /* Pink paw pad */
        /* Left paw on hip */
        gba_draw_rect(fb, gx - 15, body_top + 6, 6, 5, RGB15(22, 14, 7));
    } else {
        /* Joyful jump: both paws thrown high up into the air! */
        gba_draw_rect(fb, gx - 19, head_top - 5, 8, 6, RGB15(22, 14, 7));
        gba_draw_rect(fb, gx - 21, head_top - 8, 5, 5, RGB15(31, 20, 18));
        gba_draw_rect(fb, gx + 11, head_top - 5, 8, 6, RGB15(22, 14, 7));
        gba_draw_rect(fb, gx + 16, head_top - 8, 5, 5, RGB15(31, 20, 18));
    }
}


void gba_render_splash(GbaRenderContext *grc, int anim_tick) {
    uint16_t *fb = gba_get_backbuffer();
    grc->tick_count = anim_tick;
    
    /* 1. Sunset Sky Gradient (Dusk Purple -> Peach -> Gold Horizon) */
    for (int y = 0; y < 65; ++y) {
        int r = 16 + (y * 15) / 65;
        int g = 6  + (y * 18) / 65;
        int b = 14 + (y * 4)  / 65;
        uint16_t sky_col = RGB15(r, g, b);
        for (int x = 0; x < GBA_SCREEN_W; ++x) {
            fb[y * GBA_SCREEN_W + x] = sky_col;
        }
    }

    /* 2. Setting Sun Orb on Horizon */
    for (int dy = -14; dy <= 14; ++dy) {
        int py = 52 + dy;
        if (py < 0 || py >= 65) continue;
        for (int dx = -14; dx <= 14; ++dx) {
            if (dx * dx + dy * dy <= 14 * 14) {
                int px = 195 + dx;
                if (px >= 0 && px < GBA_SCREEN_W) {
                    fb[py * GBA_SCREEN_W + px] = RGB15(31, 28, 18);
                }
            }
        }
    }

    /* 3. Distant Slate Mountain Silhouette */
    for (int x = 0; x < GBA_SCREEN_W; ++x) {
        int my = 50 + (((x * 7) ^ (x / 13)) % 5);
        for (int y = my; y < 65; ++y) {
            fb[y * GBA_SCREEN_W + x] = RGB15(9, 7, 12);
        }
    }

    /* 4. Rolling Emerald Fairway Turf */
    for (int y = 65; y < GBA_SCREEN_H; ++y) {
        int t = y - 65;
        for (int x = 0; x < GBA_SCREEN_W; ++x) {
            /* Mowed diagonal fairway stripes */
            int stripe = ((x + y) / 10) & 1;
            uint16_t turf = stripe ? RGB15(4 + t / 32, 18 + t / 16, 5 + t / 32) : RGB15(3 + t / 32, 15 + t / 16, 4 + t / 32);
            fb[y * GBA_SCREEN_W + x] = turf;
        }
    }

    /* 5. Putting Green & White Sand Bunker on Right */
    /* Sand trap */
    for (int dy = -7; dy <= 7; ++dy) {
        int py = 76 + dy;
        if (py >= 0 && py < GBA_SCREEN_H) {
            for (int dx = -16; dx <= 16; ++dx) {
                if (dx * dx * 2 + dy * dy * 10 <= 16 * 16 * 2) {
                    int px = 205 + dx;
                    if (px >= 0 && px < GBA_SCREEN_W) {
                        fb[py * GBA_SCREEN_W + px] = (dy < -3) ? RGB15(22, 19, 11) : RGB15(29, 26, 17);
                    }
                }
            }
        }
    }
    /* Putting Green surface */
    for (int dy = -8; dy <= 8; ++dy) {
        int py = 66 + dy;
        if (py >= 0 && py < GBA_SCREEN_H) {
            for (int dx = -14; dx <= 14; ++dx) {
                if (dx * dx * 2 + dy * dy * 8 <= 14 * 14 * 2) {
                    int px = 216 + dx;
                    if (px >= 0 && px < GBA_SCREEN_W) {
                        fb[py * GBA_SCREEN_W + px] = RGB15(8, 28, 10);
                    }
                }
            }
        }
    }
    /* Flagstick & Fluttering Red Pennant */
    gba_draw_rect(fb, 218, 50, 1, 20, GBA_WHITE);
    gba_draw_rect(fb, 219, 50, 7, 5, GBA_RED);

    /* 6. The Caddyshack Building with Caddy Kids & Bushwood Sign */
    draw_gba_caddyshack_building(fb, 4, 44);

    /* 7. Big Dancing Caddyshack Gopher */
    int goph_frame = (anim_tick / 8) % 4;
    draw_gba_giant_gopher(fb, 184, 82, goph_frame);

    /* 8. Title Banner at Top */
    gba_draw_text(fb, 44, 7, "DUFFER PRO GBA", GBA_BLACK, false);
    gba_draw_text(fb, 42, 6, "DUFFER PRO GBA", GBA_GOLD, true);
    gba_draw_text(fb, 36, 18, "CHAMPIONSHIP SIMGOLF", RGB15(29, 29, 31), true);

    /* 9. Pulsing "PRESS START OR A" */
    if ((anim_tick / 16) & 1) {
        gba_draw_text(fb, 34, 135, ">> PRESS START OR A <<", GBA_WHITE, true);
    }

    /* 10. Version Stamp */
    gba_draw_text(fb, 132, 147, "v0.7.6-noonan", RGB15(20, 24, 20), true);
}


