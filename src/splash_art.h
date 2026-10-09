/* Procedural Retro Splash Art for Duffer - 100% ANSI C99 */
#ifndef SPLASH_ART_H
#define SPLASH_ART_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define SPLASH_W 640
#define SPLASH_H 480

#ifndef COLOR_ARGB
#define COLOR_ARGB(a, r, g, b) (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))
#define COLOR_RGB(r, g, b)     COLOR_ARGB(255, r, g, b)
#endif

typedef struct { int x, y; } SplashPoint;

/* Scanline Polygon Fill */
static void splash_fill_polygon(uint32_t *dst, const SplashPoint *pts, int n, uint32_t color) {
    if (n < 3) return;
    int min_y = pts[0].y, max_y = pts[0].y;
    for (int i = 1; i < n; ++i) {
        if (pts[i].y < min_y) min_y = pts[i].y;
        if (pts[i].y > max_y) max_y = pts[i].y;
    }
    if (min_y < 0) min_y = 0;
    if (max_y >= SPLASH_H) max_y = SPLASH_H - 1;

    int nodes[64];
    for (int y = min_y; y <= max_y; ++y) {
        int node_count = 0;
        int j = n - 1;
        for (int i = 0; i < n; ++i) {
            int yi = pts[i].y, yj = pts[j].y;
            if ((yi < y && yj >= y) || (yj < y && yi >= y)) {
                if (node_count < 64) {
                    int xi = pts[i].x, xj = pts[j].x;
                    int x = xi + (int)(((int64_t)(y - yi) * (xj - xi)) / (yj - yi));
                    nodes[node_count++] = x;
                }
            }
            j = i;
        }
        for (int a = 0; a < node_count - 1; ++a) {
            for (int b = a + 1; b < node_count; ++b) {
                if (nodes[a] > nodes[b]) {
                    int tmp = nodes[a]; nodes[a] = nodes[b]; nodes[b] = tmp;
                }
            }
        }
        for (int k = 0; k < node_count - 1; k += 2) {
            int x1 = nodes[k];
            int x2 = nodes[k + 1];
            if (x1 < 0) x1 = 0;
            if (x2 >= SPLASH_W) x2 = SPLASH_W - 1;
            if (x2 >= x1) {
                uint32_t *row = &dst[y * SPLASH_W + x1];
                for (int x = x1; x <= x2; ++x) {
                    *row++ = color;
                }
            }
        }
    }
}

/* Filled Ellipse with Alpha Blending */
static void splash_fill_ellipse(uint32_t *dst, int cx, int cy, int rx, int ry, uint32_t color) {
    int min_y = cy - ry; if (min_y < 0) min_y = 0;
    int max_y = cy + ry; if (max_y >= SPLASH_H) max_y = SPLASH_H - 1;
    int rx2 = rx * rx;
    int ry2 = ry * ry;
    if (rx2 == 0 || ry2 == 0) return;
    int a = (color >> 24) & 0xFF;

    for (int y = min_y; y <= max_y; ++y) {
        int dy = y - cy;
        int dy2 = dy * dy;
        if (dy2 > ry2) continue;
        int span = (int)sqrt((double)(rx2 * (ry2 - dy2)) / (double)ry2);
        int x1 = cx - span; if (x1 < 0) x1 = 0;
        int x2 = cx + span; if (x2 >= SPLASH_W) x2 = SPLASH_W - 1;
        uint32_t *row = &dst[y * SPLASH_W];
        for (int x = x1; x <= x2; ++x) {
            if (a == 255) {
                row[x] = color;
            } else if (a > 0) {
                uint32_t bg = row[x];
                int bg_r = (bg >> 16) & 0xFF, bg_g = (bg >> 8) & 0xFF, bg_b = bg & 0xFF;
                int fg_r = (color >> 16) & 0xFF, fg_g = (color >> 8) & 0xFF, fg_b = color & 0xFF;
                int r = bg_r + (((fg_r - bg_r) * a) >> 8);
                int g = bg_g + (((fg_g - bg_g) * a) >> 8);
                int b = bg_b + (((fg_b - bg_b) * a) >> 8);
                row[x] = COLOR_RGB(r, g, b);
            }
        }
    }
}

/* Thick Line */
static void splash_draw_line(uint32_t *dst, int x0, int y0, int x1, int y1, uint32_t color, int width) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    int hw = width / 2;
    while (1) {
        for (int wy = -hw; wy <= hw; ++wy) {
            for (int wx = -hw; wx <= hw; ++wx) {
                int px = x0 + wx, py = y0 + wy;
                if (px >= 0 && px < SPLASH_W && py >= 0 && py < SPLASH_H) {
                    dst[py * SPLASH_W + px] = color;
                }
            }
        }
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void draw_3d_letter(uint32_t *dst, const SplashPoint *pts, int n, int depth) {
    SplashPoint shifted[32];
    for (int d = depth; d >= 1; --d) {
        for (int i = 0; i < n; ++i) { shifted[i].x = pts[i].x + d; shifted[i].y = pts[i].y + d; }
        splash_fill_polygon(dst, shifted, n, COLOR_RGB(10, 24, 14));
    }
    for (int i = 0; i < n; ++i) { shifted[i].x = pts[i].x + 2; shifted[i].y = pts[i].y + 2; }
    splash_fill_polygon(dst, shifted, n, COLOR_RGB(130, 85, 18));
    splash_fill_polygon(dst, pts, n, COLOR_RGB(248, 218, 92));
    for (int i = 1; i < n / 2; ++i) {
        splash_draw_line(dst, pts[i - 1].x, pts[i - 1].y, pts[i].x, pts[i].y, COLOR_RGB(255, 255, 230), 2);
    }
}

/* Procedural Scene Generator */
static void generate_splash_background(uint32_t *dst) {
    /* 1. Sky Gradient */
    for (int y = 0; y < 230; ++y) {
        double t = y / 230.0;
        int r = (int)(35 * (1.0 - t) + 248 * t);
        int g = (int)(82 * (1.0 - t) + 205 * t);
        int b = (int)(145 * (1.0 - t) + 130 * t);
        uint32_t col = COLOR_RGB(r, g, b);
        for (int x = 0; x < SPLASH_W; ++x) { dst[y * SPLASH_W + x] = col; }
    }
    /* Cirrus Clouds */
    splash_fill_ellipse(dst, 130, 45, 90, 7, COLOR_ARGB(70, 255, 235, 220));
    splash_fill_ellipse(dst, 480, 75, 110, 9, COLOR_ARGB(65, 255, 235, 220));
    splash_fill_ellipse(dst, 280, 110, 80, 6, COLOR_ARGB(60, 255, 235, 220));
    splash_fill_ellipse(dst, 360, 30, 60, 5, COLOR_ARGB(50, 255, 235, 220));

    /* 2. Mountain Ridge */
    SplashPoint m_pts[64];
    int m_count = 0;
    m_pts[m_count++] = (SplashPoint){ 0, 260 };
    m_pts[m_count++] = (SplashPoint){ 0, 220 };
    for (int x = 0; x <= SPLASH_W + 16; x += 32) {
        int my = 182 - (int)(28.0 * sin(x * 0.007)) - (int)(14.0 * cos(x * 0.018));
        if (m_count < 60) m_pts[m_count++] = (SplashPoint){ x, my };
    }
    m_pts[m_count++] = (SplashPoint){ SPLASH_W, 260 };
    splash_fill_polygon(dst, m_pts, m_count, COLOR_RGB(62, 98, 118));
    for (int i = 2; i < m_count - 1; ++i) {
        splash_draw_line(dst, m_pts[i - 1].x, m_pts[i - 1].y, m_pts[i].x, m_pts[i].y, COLOR_RGB(95, 140, 158), 2);
    }

    /* 3. Rolling Hill */
    SplashPoint h1_pts[64];
    int h1_count = 0;
    h1_pts[h1_count++] = (SplashPoint){ 0, 310 };
    h1_pts[h1_count++] = (SplashPoint){ 0, 260 };
    for (int x = 0; x <= SPLASH_W + 10; x += 20) {
        int hy = 205 + (int)(32.0 * sin((x + 80) * 0.008));
        if (h1_count < 60) h1_pts[h1_count++] = (SplashPoint){ x, hy };
    }
    h1_pts[h1_count++] = (SplashPoint){ SPLASH_W, 310 };
    splash_fill_polygon(dst, h1_pts, h1_count, COLOR_RGB(52, 110, 58));
    for (int i = 2; i < h1_count - 1; ++i) {
        splash_draw_line(dst, h1_pts[i - 1].x, h1_pts[i - 1].y, h1_pts[i].x, h1_pts[i].y, COLOR_RGB(75, 148, 82), 2);
    }

    /* Distant Pin Flag */
    splash_draw_line(dst, 535, 204, 535, 174, COLOR_RGB(245, 245, 245), 2);
    SplashPoint flag_pts[3] = { { 535, 174 }, { 551, 182 }, { 535, 190 } };
    splash_fill_polygon(dst, flag_pts, 3, COLOR_RGB(230, 42, 42));
    SplashPoint flag_sub[3] = { { 539, 178 }, { 547, 182 }, { 539, 186 } };
    splash_fill_polygon(dst, flag_sub, 3, COLOR_RGB(255, 215, 45));
    splash_fill_ellipse(dst, 535, 204, 4, 2, COLOR_RGB(18, 25, 18));

    /* Distant Trees */
    static const int dist_trees[10] = { 455, 475, 498, 565, 585, 605, 622, 20, 50, 85 };
    for (int t = 0; t < 10; ++t) {
        int tx = dist_trees[t];
        int ty = 210 + (int)(26.0 * sin((tx + 80) * 0.008));
        SplashPoint tp[3] = { { tx, ty - 18 }, { tx - 6, ty }, { tx + 6, ty } };
        splash_fill_polygon(dst, tp, 3, COLOR_RGB(24, 60, 30));
        SplashPoint tlit[3] = { { tx, ty - 18 }, { tx - 6, ty }, { tx, ty } };
        splash_fill_polygon(dst, tlit, 3, COLOR_RGB(34, 78, 40));
        splash_draw_line(dst, tx, ty, tx, ty + 3, COLOR_RGB(55, 38, 24), 2);
    }

    /* Distant Sand Bunker */
    SplashPoint b_pts[6] = { { 430, 254 }, { 458, 246 }, { 488, 250 }, { 510, 262 }, { 485, 270 }, { 446, 266 } };
    splash_fill_polygon(dst, b_pts, 6, COLOR_RGB(222, 200, 138));
    splash_draw_line(dst, 430, 254, 458, 246, COLOR_RGB(160, 135, 82), 2);
    splash_draw_line(dst, 458, 246, 488, 250, COLOR_RGB(160, 135, 82), 2);
    splash_draw_line(dst, 488, 250, 510, 262, COLOR_RGB(160, 135, 82), 2);

    /* 4. Foreground Fairway Turf */
    for (int y = 240; y < SPLASH_H; ++y) {
        double t = (y - 240) / 240.0;
        int r = (int)(48 * (1.0 - t) + 82 * t);
        int g = (int)(108 * (1.0 - t) + 165 * t);
        int b = (int)(32 * (1.0 - t) + 48 * t);
        int stripe = ((y * 2) / 16) & 1;
        if (stripe) { r += 10; g += 18; b += 7; }
        uint32_t col = COLOR_RGB(r, g, b);
        for (int x = 0; x < SPLASH_W; ++x) { dst[y * SPLASH_W + x] = col; }
    }
    /* Grass Texture */
    for (int y = 250; y < SPLASH_H; y += 2) {
        for (int x = 0; x < SPLASH_W; x += 4) {
            int p = (x * 17 + y * 23) % 19;
            if (p == 0) dst[y * SPLASH_W + x] = COLOR_RGB(38, 88, 24);
            else if (p == 5) dst[y * SPLASH_W + x] = COLOR_RGB(110, 195, 68);
        }
    }

    /* 5. Foreground Trees */
    static const struct { int x, y; double s; } fg_pines[3] = { { 575, 365, 1.25 }, { 618, 415, 1.45 }, { 530, 395, 0.95 } };
    for (int p = 0; p < 3; ++p) {
        int px = fg_pines[p].x, py = fg_pines[p].y;
        double s = fg_pines[p].s;
        splash_fill_ellipse(dst, px, py + 1, (int)(28 * s), (int)(7 * s), COLOR_ARGB(140, 16, 38, 14));
        int tw = (int)(4 * s); if (tw < 2) tw = 2;
        int th = (int)(18 * s);
        SplashPoint trk[4] = { { px - tw, py - th }, { px + tw, py - th }, { px + tw, py }, { px - tw, py } };
        splash_fill_polygon(dst, trk, 4, COLOR_RGB(68, 42, 26));
        for (int i = 0; i < 4; ++i) {
            int top_y = py - (int)((th + (3 - i) * 19 + 16) * s);
            int bot_y = top_y + (int)(26 * s);
            int hw = (int)((11 + i * 8) * s);
            SplashPoint sh[3] = { { px, top_y }, { px + hw, bot_y }, { px, bot_y } };
            splash_fill_polygon(dst, sh, 3, COLOR_RGB(22 + i * 5, 58 + i * 8, 26 + i * 4));
            SplashPoint lit[3] = { { px, top_y }, { px - hw, bot_y }, { px, bot_y } };
            splash_fill_polygon(dst, lit, 3, COLOR_RGB(36 + i * 8, 88 + i * 12, 42 + i * 6));
        }
    }

    /* 6. Golfer & Bag Ground Shadow */
    splash_fill_ellipse(dst, 155, 442, 85, 18, COLOR_ARGB(155, 16, 38, 14));

    /* 7. Vintage Leather Golf Bag */
    int bx = 108, by = 442;
    SplashPoint bag_pts[4] = { { bx - 18, by }, { bx + 18, by }, { bx + 28, by - 105 }, { bx - 10, by - 105 } };
    splash_fill_polygon(dst, bag_pts, 4, COLOR_RGB(128, 72, 38));
    SplashPoint bag_lit[4] = { { bx - 18, by }, { bx, by }, { bx + 8, by - 105 }, { bx - 10, by - 105 } };
    splash_fill_polygon(dst, bag_lit, 4, COLOR_RGB(155, 90, 48));
    splash_fill_ellipse(dst, bx + 9, by - 104, 21, 6, COLOR_RGB(78, 40, 20));
    splash_fill_ellipse(dst, bx + 9, by - 104, 19, 4, COLOR_RGB(42, 22, 10));
    SplashPoint pouch[4] = { { bx - 14, by - 25 }, { bx + 10, by - 22 }, { bx + 12, by - 78 }, { bx - 10, by - 78 } };
    splash_fill_polygon(dst, pouch, 4, COLOR_RGB(115, 62, 32));
    splash_draw_line(dst, bx - 1, by - 24, bx + 1, by - 76, COLOR_RGB(220, 190, 85), 2);
    SplashPoint towel[4] = { { bx - 16, by - 85 }, { bx - 30, by - 48 }, { bx - 18, by - 36 }, { bx - 10, by - 78 } };
    splash_fill_polygon(dst, towel, 4, COLOR_RGB(240, 240, 245));
    splash_draw_line(dst, bx - 24, by - 60, bx - 20, by - 40, COLOR_RGB(185, 185, 195), 1);
    splash_draw_line(dst, bx + 24, by - 95, bx + 34, by - 60, COLOR_RGB(85, 45, 22), 4);
    splash_draw_line(dst, bx + 34, by - 60, bx + 18, by - 32, COLOR_RGB(85, 45, 22), 4);

    /* Clubs in Bag */
    splash_draw_line(dst, bx, by - 105, bx - 12, by - 140, COLOR_RGB(175, 180, 185), 3);
    splash_fill_ellipse(dst, bx - 14, by - 145, 10, 13, COLOR_RGB(28, 55, 115));
    splash_fill_ellipse(dst, bx - 14, by - 145, 8, 7, COLOR_RGB(210, 42, 42));
    splash_fill_ellipse(dst, bx - 14, by - 162, 6, 8, COLOR_RGB(245, 245, 245));
    splash_draw_line(dst, bx + 8, by - 105, bx + 4, by - 134, COLOR_RGB(175, 180, 185), 3);
    splash_fill_ellipse(dst, bx + 5, by - 138, 9, 12, COLOR_RGB(22, 75, 40));
    splash_fill_ellipse(dst, bx + 5, by - 154, 5, 6, COLOR_RGB(245, 215, 60));
    for (int i = 0; i < 3; ++i) {
        int ex = bx + 10 + i * 7, ey = by - 125 + i * 5;
        splash_draw_line(dst, bx + 10, by - 105, ex, ey, COLOR_RGB(185, 190, 200), 3);
        SplashPoint ih[4] = { { ex, ey }, { ex + 8, ey - 2 }, { ex + 10, ey - 7 }, { ex + 2, ey - 5 } };
        splash_fill_polygon(dst, ih, 4, COLOR_RGB(225, 230, 240));
    }

    /* 8. The Golfer */
    int gx = 175, gy = 442;
    /* Left Shoe */
    SplashPoint lshoe[4] = { { gx - 22, gy }, { gx + 4, gy }, { gx + 2, gy - 13 }, { gx - 18, gy - 13 } };
    splash_fill_polygon(dst, lshoe, 4, COLOR_RGB(242, 242, 248));
    SplashPoint lsad[4] = { { gx - 14, gy }, { gx - 3, gy }, { gx - 5, gy - 13 }, { gx - 12, gy - 13 } };
    splash_fill_polygon(dst, lsad, 4, COLOR_RGB(32, 32, 36));
    SplashPoint lsole[4] = { { gx - 22, gy - 3 }, { gx + 4, gy - 3 }, { gx + 4, gy }, { gx - 22, gy } };
    splash_fill_polygon(dst, lsole, 4, COLOR_RGB(26, 26, 28));
    /* Right Shoe */
    SplashPoint rshoe[4] = { { gx + 16, gy - 6 }, { gx + 38, gy - 6 }, { gx + 36, gy - 17 }, { gx + 20, gy - 17 } };
    splash_fill_polygon(dst, rshoe, 4, COLOR_RGB(242, 242, 248));
    SplashPoint rsad[4] = { { gx + 24, gy - 6 }, { gx + 32, gy - 6 }, { gx + 31, gy - 17 }, { gx + 25, gy - 17 } };
    splash_fill_polygon(dst, rsad, 4, COLOR_RGB(32, 32, 36));
    SplashPoint rsole[4] = { { gx + 16, gy - 9 }, { gx + 38, gy - 9 }, { gx + 38, gy - 6 }, { gx + 16, gy - 6 } };
    splash_fill_polygon(dst, rsole, 4, COLOR_RGB(26, 26, 28));
    /* Socks */
    SplashPoint lsok[4] = { { gx - 18, gy - 26 }, { gx - 6, gy - 26 }, { gx - 6, gy - 13 }, { gx - 18, gy - 13 } };
    splash_fill_polygon(dst, lsok, 4, COLOR_RGB(238, 238, 244));
    splash_draw_line(dst, gx - 18, gy - 23, gx - 6, gy - 23, COLOR_RGB(195, 38, 38), 2);
    SplashPoint rsok[4] = { { gx + 20, gy - 30 }, { gx + 32, gy - 30 }, { gx + 32, gy - 17 }, { gx + 20, gy - 17 } };
    splash_fill_polygon(dst, rsok, 4, COLOR_RGB(238, 238, 244));
    splash_draw_line(dst, gx + 20, gy - 27, gx + 32, gy - 27, COLOR_RGB(195, 38, 38), 2);
    /* Legs */
    SplashPoint lleg[4] = { { gx - 18, gy - 26 }, { gx - 6, gy - 26 }, { gx - 4, gy - 68 }, { gx - 20, gy - 68 } };
    splash_fill_polygon(dst, lleg, 4, COLOR_RGB(215, 155, 105));
    SplashPoint lleg_lit[4] = { { gx - 18, gy - 26 }, { gx - 12, gy - 26 }, { gx - 12, gy - 68 }, { gx - 20, gy - 68 } };
    splash_fill_polygon(dst, lleg_lit, 4, COLOR_RGB(232, 175, 125));
    SplashPoint rleg[4] = { { gx + 20, gy - 30 }, { gx + 32, gy - 30 }, { gx + 28, gy - 72 }, { gx + 14, gy - 72 } };
    splash_fill_polygon(dst, rleg, 4, COLOR_RGB(195, 138, 92));
    /* Bermuda Shorts */
    SplashPoint shorts[7] = { { gx - 26, gy - 68 }, { gx, gy - 68 }, { gx + 6, gy - 88 }, { gx + 12, gy - 72 }, { gx + 34, gy - 72 }, { gx + 38, gy - 128 }, { gx - 22, gy - 128 } };
    splash_fill_polygon(dst, shorts, 7, COLOR_RGB(196, 172, 130));
    SplashPoint shorts_lit[4] = { { gx - 26, gy - 68 }, { gx - 12, gy - 68 }, { gx - 10, gy - 128 }, { gx - 22, gy - 128 } };
    splash_fill_polygon(dst, shorts_lit, 4, COLOR_RGB(218, 194, 150));
    SplashPoint shorts_sh[4] = { { gx + 6, gy - 78 }, { gx + 34, gy - 72 }, { gx + 38, gy - 128 }, { gx + 8, gy - 128 } };
    splash_fill_polygon(dst, shorts_sh, 4, COLOR_RGB(168, 146, 106));
    splash_draw_line(dst, gx - 10, gy - 128, gx - 13, gy - 68, COLOR_RGB(168, 144, 104), 1);
    SplashPoint belt[4] = { { gx - 21, gy - 129 }, { gx + 37, gy - 129 }, { gx + 37, gy - 124 }, { gx - 21, gy - 124 } };
    splash_fill_polygon(dst, belt, 4, COLOR_RGB(82, 44, 22));
    SplashPoint buckle[4] = { { gx + 4, gy - 130 }, { gx + 14, gy - 130 }, { gx + 14, gy - 123 }, { gx + 4, gy - 123 } };
    splash_fill_polygon(dst, buckle, 4, COLOR_RGB(235, 198, 72));
    /* Red Polo Shirt */
    SplashPoint shirt[5] = { { gx - 22, gy - 128 }, { gx + 37, gy - 128 }, { gx + 32, gy - 192 }, { gx - 4, gy - 200 }, { gx - 34, gy - 188 } };
    splash_fill_polygon(dst, shirt, 5, COLOR_RGB(195, 34, 34));
    SplashPoint shirt_lit[4] = { { gx - 22, gy - 128 }, { gx + 8, gy - 128 }, { gx + 2, gy - 196 }, { gx - 34, gy - 188 } };
    splash_fill_polygon(dst, shirt_lit, 4, COLOR_RGB(226, 48, 48));
    SplashPoint shirt_sh[4] = { { gx + 8, gy - 128 }, { gx + 37, gy - 128 }, { gx + 32, gy - 192 }, { gx + 2, gy - 196 } };
    splash_fill_polygon(dst, shirt_sh, 4, COLOR_RGB(158, 24, 24));
    SplashPoint collar[5] = { { gx - 10, gy - 198 }, { gx - 4, gy - 200 }, { gx + 6, gy - 198 }, { gx + 8, gy - 190 }, { gx - 14, gy - 190 } };
    splash_fill_polygon(dst, collar, 5, COLOR_RGB(245, 245, 250));
    splash_draw_line(dst, gx - 3, gy - 190, gx - 3, gy - 168, COLOR_RGB(145, 18, 18), 2);
    splash_fill_ellipse(dst, gx - 2, gy - 183, 2, 2, COLOR_RGB(245, 245, 245));
    splash_fill_ellipse(dst, gx - 2, gy - 175, 2, 2, COLOR_RGB(245, 245, 245));
    /* Left Arm & Glove */
    SplashPoint lsleeve[4] = { { gx - 34, gy - 188 }, { gx - 20, gy - 188 }, { gx - 32, gy - 146 }, { gx - 42, gy - 146 } };
    splash_fill_polygon(dst, lsleeve, 4, COLOR_RGB(205, 38, 38));
    splash_draw_line(dst, gx - 40, gy - 146, gx - 30, gy - 146, COLOR_RGB(245, 245, 250), 2);
    SplashPoint larm[4] = { { gx - 40, gy - 146 }, { gx - 30, gy - 146 }, { gx - 22, gy - 106 }, { gx - 30, gy - 106 } };
    splash_fill_polygon(dst, larm, 4, COLOR_RGB(215, 155, 105));
    splash_fill_ellipse(dst, gx - 26, gy - 103, 8, 7, COLOR_RGB(245, 245, 250));
    splash_draw_line(dst, gx - 30, gy - 104, gx - 22, gy - 104, COLOR_RGB(38, 38, 42), 1);
    /* Head & Visor */
    int hx = gx - 2, hy = gy - 210;
    SplashPoint neck[4] = { { hx - 8, hy + 4 }, { hx + 8, hy + 4 }, { hx + 8, hy + 14 }, { hx - 8, hy + 14 } };
    splash_fill_polygon(dst, neck, 4, COLOR_RGB(205, 145, 95));
    SplashPoint face[5] = { { hx - 12, hy }, { hx + 12, hy }, { hx + 10, hy + 12 }, { hx + 4, hy + 16 }, { hx - 10, hy + 14 } };
    splash_fill_polygon(dst, face, 5, COLOR_RGB(230, 168, 118));
    splash_draw_line(dst, hx + 4, hy + 2, hx + 9, hy + 2, COLOR_RGB(42, 28, 18), 2);
    splash_fill_ellipse(dst, hx + 5, hy + 6, 3, 2, COLOR_RGB(245, 185, 135));
    splash_fill_ellipse(dst, hx - 9, hy + 6, 2, 4, COLOR_RGB(215, 150, 100));
    splash_fill_ellipse(dst, hx, hy - 6, 14, 6, COLOR_RGB(245, 245, 250));
    SplashPoint brim[4] = { { hx - 4, hy - 2 }, { hx + 24, hy + 1 }, { hx + 22, hy - 3 }, { hx + 8, hy - 6 } };
    splash_fill_polygon(dst, brim, 4, COLOR_RGB(235, 235, 240));
    splash_draw_line(dst, hx - 2, hy + 1, hx + 22, hy + 2, COLOR_RGB(28, 78, 42), 2);
    /* Right Arm & Club */
    SplashPoint rsleeve[4] = { { gx + 32, gy - 192 }, { gx + 16, gy - 196 }, { gx + 40, gy - 152 }, { gx + 52, gy - 154 } };
    splash_fill_polygon(dst, rsleeve, 4, COLOR_RGB(175, 26, 26));
    splash_draw_line(dst, gx + 40, gy - 152, gx + 52, gy - 154, COLOR_RGB(245, 245, 250), 2);
    SplashPoint rarm[4] = { { gx + 40, gy - 152 }, { gx + 52, gy - 154 }, { gx + 42, gy - 112 }, { gx + 32, gy - 114 } };
    splash_fill_polygon(dst, rarm, 4, COLOR_RGB(215, 155, 105));
    splash_fill_ellipse(dst, gx + 37, gy - 109, 7, 7, COLOR_RGB(225, 162, 112));
    /* 7-Iron Club Being Inspected */
    int cgx = gx + 36, cgy = gy - 110;
    int chx = gx + 50, chy = gy - 238;
    splash_draw_line(dst, cgx, cgy, chx, chy, COLOR_RGB(198, 204, 214), 3);
    splash_draw_line(dst, cgx, cgy, chx, chy, COLOR_RGB(250, 252, 255), 1);
    splash_draw_line(dst, cgx - 4, cgy + 15, cgx + 2, cgy - 8, COLOR_RGB(32, 32, 36), 5);
    SplashPoint chp[4] = { { chx - 4, chy }, { chx + 20, chy - 8 }, { chx + 24, chy - 20 }, { chx + 3, chy - 14 } };
    splash_fill_polygon(dst, chp, 4, COLOR_RGB(238, 242, 250));
    splash_draw_line(dst, chx + 5, chy - 6, chx + 18, chy - 11, COLOR_RGB(135, 140, 155), 1);
    splash_draw_line(dst, chx + 6, chy - 9, chx + 19, chy - 14, COLOR_RGB(135, 140, 155), 1);
    splash_draw_line(dst, chx + 7, chy - 12, chx + 20, chy - 17, COLOR_RGB(135, 140, 155), 1);
    splash_draw_line(dst, chx + 20, chy - 8, chx + 24, chy - 20, COLOR_RGB(255, 255, 255), 2);

    /* 9. STYLIZED 3D "DUFFER" LOGO */
    int lx = 270, ly = 48;
    /* Letter D */
    SplashPoint lD[14] = { {lx, ly}, {lx+26, ly}, {lx+36, ly+10}, {lx+36, ly+42}, {lx+26, ly+52}, {lx, ly+52}, {lx, ly}, {lx+12, ly+12}, {lx+22, ly+12}, {lx+25, ly+18}, {lx+25, ly+34}, {lx+22, ly+40}, {lx+12, ly+40}, {lx+12, ly+12} };
    draw_3d_letter(dst, lD, 14, 8);
    /* Letter U */
    int u = lx + 44;
    SplashPoint lU[11] = { {u, ly}, {u+12, ly}, {u+12, ly+38}, {u+24, ly+38}, {u+24, ly}, {u+36, ly}, {u+36, ly+42}, {u+26, ly+52}, {u+10, ly+52}, {u, ly+42}, {u, ly} };
    draw_3d_letter(dst, lU, 11, 8);
    /* Letter F 1 */
    int f1 = lx + 88;
    SplashPoint lF1[11] = { {f1, ly}, {f1+34, ly}, {f1+34, ly+12}, {f1+12, ly+12}, {f1+12, ly+22}, {f1+28, ly+22}, {f1+28, ly+32}, {f1+12, ly+32}, {f1+12, ly+52}, {f1, ly+52}, {f1, ly} };
    draw_3d_letter(dst, lF1, 11, 8);
    /* Letter F 2 */
    int f2 = lx + 130;
    SplashPoint lF2[11] = { {f2, ly}, {f2+34, ly}, {f2+34, ly+12}, {f2+12, ly+12}, {f2+12, ly+22}, {f2+28, ly+22}, {f2+28, ly+32}, {f2+12, ly+32}, {f2+12, ly+52}, {f2, ly+52}, {f2, ly} };
    draw_3d_letter(dst, lF2, 11, 8);
    /* Letter E */
    int e = lx + 172;
    SplashPoint lE[13] = { {e, ly}, {e+34, ly}, {e+34, ly+12}, {e+12, ly+12}, {e+12, ly+20}, {e+28, ly+20}, {e+28, ly+30}, {e+12, ly+30}, {e+12, ly+40}, {e+34, ly+40}, {e+34, ly+52}, {e, ly+52}, {e, ly} };
    draw_3d_letter(dst, lE, 13, 8);
    /* Letter R */
    int r_l = lx + 214;
    SplashPoint lR[13] = { {r_l, ly}, {r_l+26, ly}, {r_l+36, ly+10}, {r_l+36, ly+24}, {r_l+26, ly+32}, {r_l+36, ly+52}, {r_l+22, ly+52}, {r_l+16, ly+32}, {r_l+12, ly+32}, {r_l+12, ly+52}, {r_l, ly+52}, {r_l, ly}, {r_l, ly} };
    draw_3d_letter(dst, lR, 12, 8);

    /* Golf Ball */
    int gbx = lx + 272, gby = ly + 35;
    splash_fill_ellipse(dst, gbx + 2, gby + 5, 9, 7, COLOR_ARGB(180, 12, 28, 15));
    splash_fill_ellipse(dst, gbx, gby, 9, 9, COLOR_RGB(245, 245, 252));
    splash_fill_ellipse(dst, gbx - 3, gby - 3, 3, 3, COLOR_RGB(255, 255, 255));

    /* Green Ribbon Banner Under Logo */
    int rx1 = lx - 14, ry1 = ly + 66, rx2 = lx + 296, ry2 = ly + 92;
    SplashPoint rb_sh[4] = { { rx1 + 4, ry1 + 4 }, { rx2 + 4, ry1 + 4 }, { rx2 + 4, ry2 + 4 }, { rx1 + 4, ry2 + 4 } };
    splash_fill_polygon(dst, rb_sh, 4, COLOR_RGB(12, 26, 15));
    SplashPoint rb_fg[4] = { { rx1, ry1 }, { rx2, ry1 }, { rx2, ry2 }, { rx1, ry2 } };
    splash_fill_polygon(dst, rb_fg, 4, COLOR_RGB(20, 62, 34));
    SplashPoint rb_bd[4] = { { rx1 + 2, ry1 + 2 }, { rx2 - 2, ry1 + 2 }, { rx2 - 2, ry2 - 2 }, { rx1 + 2, ry2 - 2 } };
    for (int i = 0; i < 4; ++i) {
        int j = (i + 1) % 4;
        splash_draw_line(dst, rb_bd[i].x, rb_bd[i].y, rb_bd[j].x, rb_bd[j].y, COLOR_RGB(235, 198, 75), 2);
    }
}

#endif /* SPLASH_ART_H */
