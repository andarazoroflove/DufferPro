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

static void draw_gba_diamond(uint16_t *fb, int sx, int sy, SurfaceType surf, int gx, int gy, int anim_t, CourseTheme theme) {
    uint16_t base_col, alt_col;
    
    if (theme == THEME_LA_PLAYA) {
        /* LA PLAYA: Desert Sand Rough, Golden Tan Fairway, Oasis Green, Foaming Rapids */
        switch (surf) {
            case SURF_WATER: {
                int wave = ((anim_t / 10) + gx + gy) & 3;
                base_col = (wave == 0) ? RGB15(7, 22, 25) : RGB15(5, 18, 22);
                alt_col  = RGB15(31, 31, 31); /* Foaming rapids whitewater */
                break;
            }
            case SURF_ROUGH:
                base_col = ((gx + gy) & 1) ? RGB15(27, 23, 17) : RGB15(28, 24, 18);
                alt_col  = RGB15(25, 21, 15);
                break;
            case SURF_FAIRWAY:
                base_col = ((gx + gy) & 2) ? RGB15(23, 19, 13) : RGB15(21, 17, 11);
                alt_col  = RGB15(22, 18, 12);
                break;
            case SURF_GREEN:
                base_col = RGB15(9, 27, 11);
                alt_col  = RGB15(11, 29, 13);
                break;
            case SURF_SAND:
                base_col = RGB15(31, 30, 27); /* Chalk white playa dunes */
                alt_col  = RGB15(29, 28, 25);
                break;
            case SURF_TEE:
                base_col = RGB15(24, 20, 14);
                alt_col  = RGB15(31, 31, 31);
                break;
            case SURF_CARTPATH:
                base_col = RGB15(24, 18, 13);
                alt_col  = RGB15(20, 15, 10);
                break;
            case SURF_BRIDGE:
                base_col = RGB15(22, 18, 12);
                alt_col  = RGB15(17, 14, 9);
                break;
            default:
                base_col = RGB15(27, 23, 17);
                alt_col  = RGB15(25, 21, 15);
                break;
        }
    } else if (theme == THEME_NOCTURNAL_GREEN) {
        /* NOCTURNAL GREEN: Gothic Purples, Twilight Grass, Toxic Green, Volcanic Ash */
        switch (surf) {
            case SURF_WATER: {
                int wave = ((anim_t / 14) + gx + gy) & 3;
                base_col = (wave == 0) ? RGB15(3, 3, 7) : RGB15(2, 2, 5);
                alt_col  = RGB15(8, 8, 18); /* Deep indigo ripple */
                break;
            }
            case SURF_ROUGH:
                base_col = ((gx + gy) & 1) ? RGB15(6, 5, 8) : RGB15(7, 6, 9);
                alt_col  = RGB15(5, 4, 7);
                break;
            case SURF_FAIRWAY:
                base_col = ((gx + gy) & 2) ? RGB15(11, 8, 15) : RGB15(9, 6, 13);
                alt_col  = RGB15(10, 7, 14);
                break;
            case SURF_GREEN:
                base_col = RGB15(5, 22, 10); /* Glowing toxic green */
                alt_col  = RGB15(7, 25, 12);
                break;
            case SURF_SAND:
                base_col = RGB15(12, 11, 14); /* Volcanic dark ash */
                alt_col  = RGB15(10, 9, 12);
                break;
            case SURF_TEE:
                base_col = RGB15(8, 7, 11);
                alt_col  = RGB15(31, 31, 31);
                break;
            case SURF_CARTPATH:
                base_col = RGB15(9, 9, 10);
                alt_col  = RGB15(7, 7, 8);
                break;
            case SURF_BRIDGE:
                base_col = RGB15(10, 8, 7);
                alt_col  = RGB15(7, 5, 4);
                break;
            default:
                base_col = RGB15(6, 5, 8);
                alt_col  = RGB15(5, 4, 7);
                break;
        }
    } else {
        /* BUSHWOOD: Classic Country Club Emerald Greens & Golden Sand */
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
            if (surf == SURF_WATER) {
                if (theme == THEME_LA_PLAYA) {
                    if (dx >= span / 2 - 2 && dx <= span / 2 + 2 && ((dy + (anim_t / 6)) & 2) == 0) {
                        c = alt_col; /* Whitewater rapids froth */
                    }
                } else if (dx == span / 2 && ((dy + (anim_t / 8)) & 3) == 0) {
                    c = alt_col; /* Water shimmer */
                }
            } else if (surf == SURF_SAND && ((dx ^ dy) & 3) == 0) {
                c = alt_col; /* Grain texture */
            }
            fb[py * GBA_SCREEN_W + px] = c;
        }
    }
}

/* Vertical Cliff Facade for 3D Elevation Steps */
static void draw_gba_cliff(uint16_t *fb, int sx, int sy, int elev_drop, CourseTheme theme) {
    if (elev_drop <= 0) return;
    int h = elev_drop * GBA_HEIGHT_STEP;
    if (h > 40) h = 40;
    
    for (int dy = 0; dy < h; ++dy) {
        int py = sy + dy;
        if (py < 0 || py >= GBA_SCREEN_H) continue;
        
        for (int dx = 0; dx < GBA_TILE_W; ++dx) {
            int px = sx + dx;
            if (px < 0 || px >= GBA_SCREEN_W) continue;
            
            uint16_t cliff_col;
            if (theme == THEME_LA_PLAYA) {
                cliff_col = ((dx + dy) & 2) ? RGB15(21, 11, 7) : RGB15(17, 8, 5); /* Red sandstone */
            } else if (theme == THEME_NOCTURNAL_GREEN) {
                cliff_col = ((dx + dy) & 2) ? RGB15(6, 6, 8) : RGB15(4, 4, 6);   /* Basalt obsidian */
            } else {
                cliff_col = ((dx + dy) & 2) ? RGB15(11, 11, 12) : RGB15(8, 8, 9); /* Granite */
            }
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
    GBA_ENT_BUSH,
    GBA_ENT_ROCK,
    GBA_ENT_GOLFER,
    GBA_ENT_CADDY,
    GBA_ENT_BALL,
    GBA_ENT_BEV_CART,
    GBA_ENT_RANGER,
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

/* 2. Pine / Palm / Saguaro / Autumn Scenery Tree */
static void draw_gba_tree(uint16_t *fb, int sx, int sy, int tree_type, CourseTheme theme) {
    if (theme == THEME_LA_PLAYA) {
        /* Saguaro Cactus with branching arms */
        /* Trunk Shadow */
        for (int dx = -3; dx <= 3; ++dx) {
            int px = sx + dx;
            if (px >= 0 && px < GBA_SCREEN_W && sy >= 0 && sy < GBA_SCREEN_H) {
                fb[sy * GBA_SCREEN_W + px] = RGB15(16, 13, 9);
            }
        }
        /* Main Columnar Stem (4px wide, height 18) */
        for (int y = -18; y <= 0; ++y) {
            int py = sy + y;
            if (py < 0 || py >= GBA_SCREEN_H) continue;
            for (int x = -2; x <= 1; ++x) {
                int px = sx + x;
                if (px >= 0 && px < GBA_SCREEN_W) {
                    uint16_t col = (x == -1) ? RGB15(8, 22, 9) : RGB15(5, 17, 6);
                    fb[py * GBA_SCREEN_W + px] = col;
                }
            }
        }
        /* Left Branching Arm: horizontal branch at y = -10, then vertical up to y = -16 */
        for (int x = -6; x <= -2; ++x) {
            int py = sy - 10;
            int px = sx + x;
            if (px >= 0 && px < GBA_SCREEN_W && py >= 0 && py < GBA_SCREEN_H) {
                fb[py * GBA_SCREEN_W + px] = RGB15(6, 18, 7);
            }
        }
        for (int y = -16; y <= -10; ++y) {
            int py = sy + y;
            if (py >= 0 && py < GBA_SCREEN_H) {
                for (int x = -7; x <= -5; ++x) {
                    int px = sx + x;
                    if (px >= 0 && px < GBA_SCREEN_W) {
                        fb[py * GBA_SCREEN_W + px] = (x == -6) ? RGB15(8, 22, 9) : RGB15(5, 17, 6);
                    }
                }
            }
        }
        /* Right Branching Arm: horizontal branch at y = -7, then vertical up to y = -13 */
        for (int x = 1; x <= 5; ++x) {
            int py = sy - 7;
            int px = sx + x;
            if (px >= 0 && px < GBA_SCREEN_W && py >= 0 && py < GBA_SCREEN_H) {
                fb[py * GBA_SCREEN_W + px] = RGB15(6, 18, 7);
            }
        }
        for (int y = -13; y <= -7; ++y) {
            int py = sy + y;
            if (py >= 0 && py < GBA_SCREEN_H) {
                for (int x = 4; x <= 6; ++x) {
                    int px = sx + x;
                    if (px >= 0 && px < GBA_SCREEN_W) {
                        fb[py * GBA_SCREEN_W + px] = (x == 5) ? RGB15(8, 22, 9) : RGB15(5, 17, 6);
                    }
                }
            }
        }
    } else if (theme == THEME_NOCTURNAL_GREEN) {
        /* Spooky Autumn Foliage / Twisted Leafless Tree */
        for (int y = -10; y <= 0; ++y) {
            int py = sy + y;
            if (py >= 0 && py < GBA_SCREEN_H) {
                for (int x = -1; x <= 1; ++x) {
                    int px = sx + x + (y < -5 ? -1 : 0);
                    if (px >= 0 && px < GBA_SCREEN_W) fb[py * GBA_SCREEN_W + px] = RGB15(6, 4, 4);
                }
            }
        }
        if ((tree_type & 1) == 0) {
            /* Crimson / Burnt Orange Canopy */
            uint16_t leaf_col = (tree_type == 2) ? RGB15(28, 6, 2) : RGB15(26, 12, 3);
            for (int layer = 0; layer < 3; ++layer) {
                int ly = sy - 10 - layer * 6;
                int rad = 8 - layer * 2;
                for (int dy = -rad; dy <= 2; ++dy) {
                    int py = ly + dy;
                    if (py < 0 || py >= GBA_SCREEN_H) continue;
                    for (int dx = -rad; dx <= rad; ++dx) {
                        if (dx * dx + dy * dy <= rad * rad) {
                            int px = sx + dx;
                            if (px >= 0 && px < GBA_SCREEN_W) {
                                fb[py * GBA_SCREEN_W + px] = (dx > 0) ? leaf_col : (leaf_col - RGB15(4, 2, 0));
                            }
                        }
                    }
                }
            }
        } else {
            /* Bare Leafless Spooky Branches */
            for (int b = -16; b <= -8; ++b) {
                int py = sy + b;
                if (py < 0 || py >= GBA_SCREEN_H) continue;
                int bx1 = sx + (b + 11);
                int bx2 = sx - (b + 11);
                if (bx1 >= 0 && bx1 < GBA_SCREEN_W) fb[py * GBA_SCREEN_W + bx1] = RGB15(5, 4, 4);
                if (bx2 >= 0 && bx2 < GBA_SCREEN_W) fb[py * GBA_SCREEN_W + bx2] = RGB15(5, 4, 4);
            }
        }
    } else {
        /* Classic Bushwood Pines & Oaks */
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
}

/* 2.5 Bushes, Tumbleweeds, & Coffin Piles */
static void draw_gba_bush(uint16_t *fb, int sx, int sy, CourseTheme theme) {
    if (theme == THEME_LA_PLAYA) {
        /* Tumbleweed (spherical dried brush) */
        for (int dy = -5; dy <= 1; ++dy) {
            int py = sy + dy - 2;
            if (py < 0 || py >= GBA_SCREEN_H) continue;
            for (int dx = -5; dx <= 5; ++dx) {
                if (dx * dx + dy * dy <= 25) {
                    int px = sx + dx;
                    if (px >= 0 && px < GBA_SCREEN_W) {
                        uint16_t tw_col = ((dx ^ dy) & 1) ? RGB15(20, 16, 9) : RGB15(15, 11, 6);
                        fb[py * GBA_SCREEN_W + px] = tw_col;
                    }
                }
            }
        }
    } else if (theme == THEME_NOCTURNAL_GREEN) {
        /* Weathered Coffin Mound / Briar patch */
        for (int dy = -4; dy <= 0; ++dy) {
            int py = sy + dy;
            if (py < 0 || py >= GBA_SCREEN_H) continue;
            for (int dx = -7; dx <= 7; ++dx) {
                int px = sx + dx;
                if (px >= 0 && px < GBA_SCREEN_W) {
                    uint16_t c = (dy == -4 || dy == 0 || dx == -7 || dx == 7) ? RGB15(8, 5, 3) : RGB15(13, 9, 5);
                    fb[py * GBA_SCREEN_W + px] = c;
                }
            }
        }
    } else {
        /* Bushwood Flowering Shrub */
        for (int dy = -5; dy <= 1; ++dy) {
            int py = sy + dy - 2;
            if (py < 0 || py >= GBA_SCREEN_H) continue;
            for (int dx = -6; dx <= 6; ++dx) {
                if (dx * dx * 2 + dy * dy * 3 <= 72) {
                    int px = sx + dx;
                    if (px >= 0 && px < GBA_SCREEN_W) {
                        uint16_t bc = ((dx ^ dy) & 3) == 0 ? RGB15(6, 24, 7) : RGB15(3, 17, 4);
                        if (dx == 0 && dy == -2) bc = RGB15(31, 31, 31); /* Flower dot */
                        fb[py * GBA_SCREEN_W + px] = bc;
                    }
                }
            }
        }
    }
}

/* 2.6 Boulders, Mesa Rocks, & Cemetery Tombstones */
static void draw_gba_rock(uint16_t *fb, int sx, int sy, CourseTheme theme) {
    if (theme == THEME_LA_PLAYA) {
        /* Desert Red Sandstone Mesa Boulder */
        for (int dy = -7; dy <= 0; ++dy) {
            int py = sy + dy;
            if (py < 0 || py >= GBA_SCREEN_H) continue;
            int rw = 7 - (dy * dy) / 10;
            for (int dx = -rw; dx <= rw; ++dx) {
                int px = sx + dx;
                if (px >= 0 && px < GBA_SCREEN_W) {
                    uint16_t rc = (dy < -4) ? RGB15(24, 14, 8) : ((dx > 0) ? RGB15(19, 10, 6) : RGB15(14, 7, 4));
                    fb[py * GBA_SCREEN_W + px] = rc;
                }
            }
        }
    } else if (theme == THEME_NOCTURNAL_GREEN) {
        /* Cemetery Tombstone with Etched Cross */
        for (int dy = -10; dy <= 0; ++dy) {
            int py = sy + dy;
            if (py < 0 || py >= GBA_SCREEN_H) continue;
            for (int dx = -4; dx <= 4; ++dx) {
                int px = sx + dx;
                if (px >= 0 && px < GBA_SCREEN_W) {
                    uint16_t tc = RGB15(14, 13, 16);
                    if (dy >= -8 && dy <= -4 && dx == 0) tc = RGB15(6, 5, 8); /* Vertical cross beam */
                    if (dy == -6 && dx >= -2 && dx <= 2) tc = RGB15(6, 5, 8); /* Horizontal cross beam */
                    fb[py * GBA_SCREEN_W + px] = tc;
                }
            }
        }
    } else {
        /* Bushwood Granite Boulder */
        for (int dy = -6; dy <= 0; ++dy) {
            int py = sy + dy;
            if (py < 0 || py >= GBA_SCREEN_H) continue;
            int rw = 6 - (dy * dy) / 8;
            for (int dx = -rw; dx <= rw; ++dx) {
                int px = sx + dx;
                if (px >= 0 && px < GBA_SCREEN_W) {
                    uint16_t gc = (dy < -3) ? RGB15(16, 17, 18) : ((dx > 0) ? RGB15(12, 12, 13) : RGB15(8, 8, 9));
                    fb[py * GBA_SCREEN_W + px] = gc;
                }
            }
        }
    }
}

/* 2.7 Course Ranger / Marshall Patrol Cart */
static void draw_gba_ranger_cart(uint16_t *fb, int sx, int sy, int state, int anim_t) {
    /* Ground Shadow */
    for (int dx = -10; dx <= 10; ++dx) {
        int px = sx + dx;
        if (px >= 0 && px < GBA_SCREEN_W && sy >= 0 && sy < GBA_SCREEN_H) {
            fb[sy * GBA_SCREEN_W + px] = GBA_BLACK;
        }
    }
    /* 4 Black Rubber Wheels with Hubcaps */
    gba_draw_rect(fb, sx - 9, sy - 3, 4, 4, GBA_BLACK);
    gba_draw_rect(fb, sx + 5, sy - 3, 4, 4, GBA_BLACK);
    gba_draw_rect(fb, sx - 8, sy - 2, 2, 2, GBA_GRAY);
    gba_draw_rect(fb, sx + 6, sy - 2, 2, 2, GBA_GRAY);

    /* Cart Chassis (White body with patrol stripe) */
    gba_draw_rect(fb, sx - 10, sy - 8, 20, 6, RGB15(28, 28, 29));
    gba_draw_rect(fb, sx - 10, sy - 5, 20, 1, RGB15(22, 4, 4));

    /* Canopy Roof Support Posts */
    gba_draw_rect(fb, sx - 8, sy - 18, 1, 10, RGB15(16, 16, 18));
    gba_draw_rect(fb, sx + 7, sy - 18, 1, 10, RGB15(16, 16, 18));

    /* Canopy Roof */
    gba_draw_rect(fb, sx - 10, sy - 19, 20, 2, RGB15(26, 26, 28));

    /* Ranger Figure */
    /* Tan Campaign Hat (Smokey Bear brim & crown) */
    gba_draw_rect(fb, sx - 3, sy - 17, 7, 2, RGB15(18, 14, 8));
    gba_draw_rect(fb, sx - 2, sy - 19, 5, 2, RGB15(22, 17, 10));
    /* Face */
    gba_draw_rect(fb, sx - 2, sy - 15, 4, 3, RGB15(28, 21, 16));
    /* Khaki Uniform Shirt */
    gba_draw_rect(fb, sx - 3, sy - 12, 6, 5, RGB15(20, 17, 11));
    /* Gold Badge */
    gba_draw_rect(fb, sx - 1, sy - 11, 2, 2, GBA_GOLD);

    /* Flashing Emergency Beacon Light on Roof */
    bool flash = ((anim_t / 8) & 1) != 0;
    uint16_t beacon_col = flash ? GBA_YELLOW : RGB15(31, 14, 0);
    gba_draw_rect(fb, sx - 1, sy - 22, 3, 3, beacon_col);

    /* Scolding / Alert Indicator if Chasing/Scolding */
    if (state == RANGER_STATE_SCOLDING || state == RANGER_STATE_CHASING) {
        gba_draw_rect(fb, sx - 1, sy - 29, 3, 5, GBA_RED);
        gba_draw_rect(fb, sx - 1, sy - 23, 3, 2, GBA_RED);
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
    
    /* Clear Screen with Theme Backdrop */
    uint16_t bg_col;
    if (gs->theme == THEME_LA_PLAYA) {
        bg_col = RGB15(25, 21, 15); /* Desert beige */
    } else if (gs->theme == THEME_NOCTURNAL_GREEN) {
        bg_col = RGB15(4, 3, 6);   /* Gothic purple */
    } else {
        bg_col = RGB15(2, 8, 3);    /* Emerald green */
    }
    for (int y = 0; y < GBA_SCREEN_H; ++y) {
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
            draw_gba_diamond(fb, sx, sy, surf, gx, gy, grc->tick_count, gs->theme);
            
            /* Elevation cliff face */
            if (gx + 1 < GRID_W && gy + 1 < GRID_H) {
                int adj_elev = gs->grid[gx + 1][gy + 1].elevation;
                if (elev > adj_elev) {
                    draw_gba_cliff(fb, sx, sy + GBA_TILE_H, elev - adj_elev, gs->theme);
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
            
            /* Trees & Scenery Features */
            FeatureType feat = gs->grid[gx][gy].feature;
            if (feat == FEAT_PINE || feat == FEAT_OAK || feat == FEAT_PALM || feat == FEAT_FLOWERING) {
                add_drawable(GBA_ENT_TREE, center_x, center_y, depth_key + 14, (int)feat);
            } else if (feat == FEAT_BUSH || feat == FEAT_FLOWERS) {
                add_drawable(GBA_ENT_BUSH, center_x, center_y, depth_key + 14, (int)feat);
            } else if (feat == FEAT_ROCK) {
                add_drawable(GBA_ENT_ROCK, center_x, center_y, depth_key + 14, (int)feat);
            }
        }
    }
    
    /* 3. Add Dynamic Entities (Golfer, Caddy, Ball, Gopher, Carl, Ranger) */
    int golfer_gx = FROM_FIXED(gs->golfer.x);
    int golfer_gy = FROM_FIXED(gs->golfer.y);
    if (golfer_gx < 0) golfer_gx = 0; if (golfer_gx >= GRID_W) golfer_gx = GRID_W - 1;
    if (golfer_gy < 0) golfer_gy = 0; if (golfer_gy >= GRID_H) golfer_gy = GRID_H - 1;
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
        if (goph_gx < 0) goph_gx = 0; if (goph_gx >= GRID_W) goph_gx = GRID_W - 1;
        if (goph_gy < 0) goph_gy = 0; if (goph_gy >= GRID_H) goph_gy = GRID_H - 1;
        int gp_sx = (goph_gx - goph_gy) * GBA_TILE_HALF_W + grc->cam_x + GBA_TILE_HALF_W;
        int gp_sy = (goph_gx + goph_gy) * GBA_TILE_HALF_H - (gs->grid[goph_gx][goph_gy].elevation * GBA_HEIGHT_STEP) + grc->cam_y + GBA_TILE_HALF_H;
        add_drawable(GBA_ENT_GOPHER, gp_sx, gp_sy, (goph_gx + goph_gy) * 32 + 13, gs->gopher.anim_frame);
    }
    
    /* Carl Spackler */
    if (gs->carl.active) {
        int cgx = FROM_FIXED(gs->carl.x);
        int cgy = FROM_FIXED(gs->carl.y);
        if (cgx < 0) cgx = 0; if (cgx >= GRID_W) cgx = GRID_W - 1;
        if (cgy < 0) cgy = 0; if (cgy >= GRID_H) cgy = GRID_H - 1;
        int c_sx = (cgx - cgy) * GBA_TILE_HALF_W + grc->cam_x + GBA_TILE_HALF_W;
        int c_sy = (cgx + cgy) * GBA_TILE_HALF_H - (gs->grid[cgx][cgy].elevation * GBA_HEIGHT_STEP) + grc->cam_y + GBA_TILE_HALF_H;
        add_drawable(GBA_ENT_CARL, c_sx, c_sy, (cgx + cgy) * 32 + 17, 0);
        
        /* TNT Plunger box */
        if (gs->carl.attack_type == CARL_ATTACK_DYNAMITE &&
            (gs->carl.state == CARL_STATE_DYNAMITE_PLANT || gs->carl.state == CARL_STATE_DYNAMITE_PLUNGE || gs->carl.state == CARL_STATE_CELEBRATING)) {
            int pgx = FROM_FIXED(gs->carl.plunger_x);
            int pgy = FROM_FIXED(gs->carl.plunger_y);
            if (pgx < 0) pgx = 0; if (pgx >= GRID_W) pgx = GRID_W - 1;
            if (pgy < 0) pgy = 0; if (pgy >= GRID_H) pgy = GRID_H - 1;
            int p_sx = (pgx - pgy) * GBA_TILE_HALF_W + grc->cam_x + GBA_TILE_HALF_W;
            int p_sy = (pgx + pgy) * GBA_TILE_HALF_H - (gs->grid[pgx][pgy].elevation * GBA_HEIGHT_STEP) + grc->cam_y + GBA_TILE_HALF_H;
            bool plunged = (gs->carl.state == CARL_STATE_DYNAMITE_PLUNGE && gs->carl.timer >= 14) || gs->carl.state == CARL_STATE_CELEBRATING;
            add_drawable(GBA_ENT_TNT_BOX, p_sx, p_sy, (pgx + pgy) * 32 + 15, plunged ? 1 : 0);
        }
    }
    
    /* Course Ranger Patrol Cart for Sneak On Mode */
    if (gs->is_sneak_on && gs->ranger.active) {
        int rgx = FROM_FIXED(gs->ranger.x);
        int rgy = FROM_FIXED(gs->ranger.y);
        if (rgx < 0) rgx = 0; if (rgx >= GRID_W) rgx = GRID_W - 1;
        if (rgy < 0) rgy = 0; if (rgy >= GRID_H) rgy = GRID_H - 1;
        int r_sx = (rgx - rgy) * GBA_TILE_HALF_W + grc->cam_x + GBA_TILE_HALF_W;
        int r_sy = (rgx + rgy) * GBA_TILE_HALF_H - (gs->grid[rgx][rgy].elevation * GBA_HEIGHT_STEP) + grc->cam_y + GBA_TILE_HALF_H;
        add_drawable(GBA_ENT_RANGER, r_sx, r_sy, (rgx + rgy) * 32 + 16, gs->ranger.state);
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
                draw_gba_tree(fb, d->sx, d->sy, d->data, gs->theme);
                break;
            case GBA_ENT_BUSH:
                draw_gba_bush(fb, d->sx, d->sy, gs->theme);
                break;
            case GBA_ENT_ROCK:
                draw_gba_rock(fb, d->sx, d->sy, gs->theme);
                break;
            case GBA_ENT_RANGER:
                draw_gba_ranger_cart(fb, d->sx, d->sy, d->data, grc->tick_count);
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
    
    /* 7. Bottom Swing Meter / Look HUD (240x24) */
    gba_draw_rect(fb, 0, 136, GBA_SCREEN_W, 24, RGB15(2, 4, 8));
    gba_draw_rect(fb, 0, 136, GBA_SCREEN_W, 1, RGB15(12, 16, 22));
    
    if (grc->look_mode) {
        gba_draw_text(fb, 6, 139, "[A] SET AIM   [B] TURBO SPEED", GBA_GOLD, false);
        gba_draw_text(fb, 6, 149, "[D-PAD] MOVE CAMERA   [SEL] RETURN", GBA_WHITE, false);
    } else if (gs->banner_timer > 0 && gs->banner_text[0]) {
        /* Comic Event Banner */
        gba_draw_text(fb, 6, 144, gs->banner_text, GBA_YELLOW, true);
    } else {
        /* Full-Width 3-Click Swing Gauge spanning bottom bar */
        int meter_x = 4;
        int meter_y = 139;
        int meter_w = 232;
        int meter_h = 10;
        
        /* Gauge Outer Frame & Track */
        gba_draw_rect(fb, meter_x - 1, meter_y - 1, meter_w + 2, meter_h + 2, RGB15(18, 22, 28));
        gba_draw_rect(fb, meter_x, meter_y, meter_w, meter_h, RGB15(4, 6, 10));
        
        /* Sweetspot Zone */
        int sweet_x = meter_x + (gs->swing.sweetspot_center * meter_w) / 65536;
        int sweet_half = (gs->swing.sweetspot_width * meter_w) / 65536;
        if (sweet_half < 5) sweet_half = 5;
        gba_draw_rect(fb, sweet_x - sweet_half, meter_y, sweet_half * 2, meter_h, RGB15(4, 22, 10));
        gba_draw_rect(fb, sweet_x - 1, meter_y, 3, meter_h, GBA_GOLD);
        
        /* Cursor Position & Power Fill */
        int cur_x = meter_x + (gs->swing.cursor_pos * meter_w) / 65536;
        if (cur_x < meter_x) cur_x = meter_x;
        if (cur_x > meter_x + meter_w - 1) cur_x = meter_x + meter_w - 1;
        
        if (gs->swing.state >= SWING_STATE_POWER) {
            int fill_w = cur_x - meter_x;
            if (fill_w > 0) {
                for (int fx = 0; fx < fill_w; ++fx) {
                    int t = (fx * 31) / meter_w;
                    uint16_t grad_col = RGB15(16 + t / 2, 4 + t / 2, 4);
                    for (int my = meter_y; my < meter_y + meter_h; ++my) {
                        fb[my * GBA_SCREEN_W + (meter_x + fx)] = grad_col;
                    }
                }
            }
        }
        /* Needle Cursor (3px wide with dark outline) */
        gba_draw_rect(fb, cur_x - 2, meter_y - 2, 5, meter_h + 4, GBA_BLACK);
        gba_draw_rect(fb, cur_x - 1, meter_y - 2, 3, meter_h + 4, GBA_WHITE);
        
        /* Key Prompts */
        gba_draw_text(fb, 4, 151, "[A]SWING [B/U/D]CLUB [SEL]LOOK", GBA_WHITE, false);
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


void gba_render_splash(GbaRenderContext *grc, int anim_tick, int menu_choice) {
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

    /* 9. Interactive Splash Buttons: "JUMP ON" vs "PLAY HOLE" */
    int b0_x = 16, b0_y = 114, b0_w = 98, b0_h = 18;
    int b1_x = 126, b1_y = 114, b1_w = 98, b1_h = 18;

    /* Button 0: JUMP ON */
    if (menu_choice == 0) {
        gba_draw_rect(fb, b0_x - 1, b0_y - 1, b0_w + 2, b0_h + 2, GBA_GOLD);
        gba_draw_rect(fb, b0_x, b0_y, b0_w, b0_h, RGB15(3, 14, 5));
        gba_draw_text(fb, b0_x + 9, b0_y + 5, "> JUMP ON <", GBA_GOLD, true);
    } else {
        gba_draw_rect(fb, b0_x - 1, b0_y - 1, b0_w + 2, b0_h + 2, RGB15(8, 10, 12));
        gba_draw_rect(fb, b0_x, b0_y, b0_w, b0_h, RGB15(2, 3, 5));
        gba_draw_text(fb, b0_x + 9, b0_y + 5, "  JUMP ON  ", RGB15(15, 17, 19), false);
    }

    /* Button 1: PLAY HOLE */
    if (menu_choice == 1) {
        gba_draw_rect(fb, b1_x - 1, b1_y - 1, b1_w + 2, b1_h + 2, GBA_GOLD);
        gba_draw_rect(fb, b1_x, b1_y, b1_w, b1_h, RGB15(3, 14, 5));
        gba_draw_text(fb, b1_x + 5, b1_y + 5, "> PLAY HOLE <", GBA_GOLD, true);
    } else {
        gba_draw_rect(fb, b1_x - 1, b1_y - 1, b1_w + 2, b1_h + 2, RGB15(8, 10, 12));
        gba_draw_rect(fb, b1_x, b1_y, b1_w, b1_h, RGB15(2, 3, 5));
        gba_draw_text(fb, b1_x + 5, b1_y + 5, "  PLAY HOLE  ", RGB15(15, 17, 19), false);
    }

    /* 10. Navigation Hints & Mode Info */
    gba_draw_text(fb, 8, 136, "[D-PAD]SELECT  [A/START]PLAY", GBA_WHITE, true);

    if (menu_choice == 0) {
        gba_draw_text(fb, 12, 148, "* JUMP ON: AVOID RANGER! *", GBA_YELLOW, true);
    } else {
        gba_draw_text(fb, 20, 148, "* PLAY HOLE: NO RANGER *", GBA_LIGHT_GREEN, true);
    }
}


