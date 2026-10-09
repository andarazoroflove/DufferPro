import math
from PIL import Image, ImageDraw, ImageFont

W = 640
H = 480
img = Image.new("RGBA", (W, H), (0, 0, 0, 255))
draw = ImageDraw.Draw(img)

# 1. Scenic Morning Sky (Smooth Golden Hour Gradient)
for y in range(230):
    t = y / 230.0
    # From deep royal dawn blue down to glowing sunrise amber/peach
    r = int(35 * (1 - t) + 248 * t)
    g = int(82 * (1 - t) + 205 * t)
    b = int(145 * (1 - t) + 130 * t)
    draw.line([(0, y), (W, y)], fill=(r, g, b, 255))

# Soft, wispy morning clouds with peach undertones
for cy, cx, cw, ch in [(45, 130, 180, 14), (75, 480, 220, 18), (110, 280, 160, 12), (30, 360, 120, 10)]:
    for i in range(ch):
        t = i / float(ch)
        span = int((cw // 2) * math.sin(math.pi * t))
        alpha = int(75 * math.sin(math.pi * t))
        # Warm morning cloud color
        cr = int(255 * (1 - t * 0.2))
        cg = int(240 * (1 - t * 0.3) + 210 * (t * 0.3))
        cb = int(230 * (1 - t * 0.5) + 180 * (t * 0.5))
        draw.line([(cx - span, cy - ch//2 + i), (cx + span, cy - ch//2 + i)], fill=(cr, cg, cb, alpha))

# 2. Distant Misty Mountain Ridge
m_points = [(0, 220)]
for x in range(0, W + 16, 16):
    my = 182 - 28 * math.sin(x * 0.007) - 14 * math.cos(x * 0.018)
    m_points.append((x, int(my)))
m_points.extend([(W, 260), (0, 260)])
draw.polygon(m_points, fill=(62, 98, 118, 255))
# Mountain ridge sunlit rim
for i in range(1, len(m_points) - 2):
    draw.line([m_points[i-1], m_points[i]], fill=(95, 140, 158, 255), width=2)

# 3. Rolling Distant Hill with Fairway & Pin
h1_points = [(0, 260)]
for x in range(0, W + 10, 10):
    hy = 205 + 32 * math.sin((x + 80) * 0.008)
    h1_points.append((x, int(hy)))
h1_points.extend([(W, 310), (0, 310)])
draw.polygon(h1_points, fill=(52, 110, 58, 255))
# Hill crest highlight
for i in range(1, len(h1_points) - 2):
    draw.line([h1_points[i-1], h1_points[i]], fill=(75, 148, 82, 255), width=2)

# Distant Pin Flag on elevated knoll
flag_x, flag_y = 535, 204
draw.line([(flag_x, flag_y), (flag_x, flag_y - 30)], fill=(245, 245, 245, 255), width=2) # flagpole
draw.polygon([(flag_x, flag_y - 30), (flag_x + 16, flag_y - 22), (flag_x, flag_y - 14)], fill=(230, 42, 42, 255)) # flag
draw.polygon([(flag_x + 4, flag_y - 26), (flag_x + 12, flag_y - 22), (flag_x + 4, flag_y - 18)], fill=(255, 215, 45, 255)) # yellow stripe
draw.ellipse([flag_x - 4, flag_y - 2, flag_x + 4, flag_y + 2], fill=(18, 25, 18, 255)) # cup hole

# Distant Tree Stand
def draw_dist_pine(px, py, h):
    draw.polygon([(px, py - h), (px - h//3, py), (px + h//3, py)], fill=(24, 60, 30, 255))
    draw.polygon([(px, py - h), (px - h//3, py), (px, py)], fill=(34, 78, 40, 255)) # lit left
    draw.line([(px, py), (px, py + 3)], fill=(55, 38, 24, 255), width=2)

for tx in [455, 475, 498, 565, 585, 605, 622, 20, 50, 85]:
    ty = int(210 + 26 * math.sin((tx + 80) * 0.008))
    draw_dist_pine(tx, ty, 18)

# Distant White Sand Trap
bunker_pts = [(430, 254), (458, 246), (488, 250), (510, 262), (485, 270), (446, 266)]
draw.polygon(bunker_pts, fill=(222, 200, 138, 255))
draw.line([(430, 254), (458, 246), (488, 250), (510, 262)], fill=(160, 135, 82, 255), width=2)

# 4. Foreground Rolling Fairway Turf (y=245 to 480)
for y in range(240, H):
    t = (y - 240) / 240.0
    r = int(48 * (1 - t) + 82 * t)
    g = int(108 * (1 - t) + 165 * t)
    b = int(32 * (1 - t) + 48 * t)
    # Distinct 2:1 isometric diagonal mower striping!
    stripe = ((y * 2) // 16) & 1
    if stripe:
        r += 10; g += 18; b += 7
    draw.line([(0, y), (W, y)], fill=(r, g, b, 255))

# Subtle fairway cross-stipple texture
for y in range(250, H, 2):
    for x in range(0, W, 4):
        p = (x * 17 + y * 23) % 19
        if p == 0:
            draw.point((x, y), fill=(38, 88, 24, 255))
        elif p == 5:
            draw.point((x, y), fill=(110, 195, 68, 255))

# 5. Foreground Pine Trees on Right Side (Rich 4-Tier Shading)
def draw_fg_pine(px, py, scale):
    draw.ellipse([px - 28*scale, py - 6*scale, px + 28*scale, py + 8*scale], fill=(16, 38, 14, 140)) # shadow
    tw = max(2, int(4 * scale))
    th = int(18 * scale)
    draw.rectangle([px - tw, py - th, px + tw, py], fill=(68, 42, 26, 255))
    for i in range(4):
        top_y = py - int((th + (3 - i) * 19 + 16) * scale)
        bot_y = top_y + int(26 * scale)
        hw = int((11 + i * 8) * scale)
        # Shadow side (right)
        draw.polygon([(px, top_y), (px + hw, bot_y), (px, bot_y)], fill=(22 + i*5, 58 + i*8, 26 + i*4, 255))
        # Lit side (left)
        draw.polygon([(px, top_y), (px - hw, bot_y), (px, bot_y)], fill=(36 + i*8, 88 + i*12, 42 + i*6, 255))
        # Scalloped bottom needles
        for dx in range(-hw, hw, int(4 * scale)):
            draw.line([(px + dx, bot_y), (px + dx + int(2 * scale), bot_y + int(3 * scale))], fill=(25 + i*6, 65 + i*9, 30 + i*5, 255), width=1)

draw_fg_pine(575, 365, 1.25)
draw_fg_pine(618, 415, 1.45)
draw_fg_pine(530, 395, 0.95)

# 6. Shadow under Golfer & Leather Bag
draw.ellipse([70, 425, 245, 462], fill=(16, 38, 14, 155))

# 7. Vintage Leather Golf Bag
bag_base_x = 108
bag_base_y = 442
# Bag Body (rich tan cognac leather)
draw.polygon([(bag_base_x - 18, bag_base_y), (bag_base_x + 18, bag_base_y),
              (bag_base_x + 28, bag_base_y - 105), (bag_base_x - 10, bag_base_y - 105)],
             fill=(128, 72, 38, 255))
# Lit side highlight
draw.polygon([(bag_base_x - 18, bag_base_y), (bag_base_x, bag_base_y),
              (bag_base_x + 8, bag_base_y - 105), (bag_base_x - 10, bag_base_y - 105)],
             fill=(155, 90, 48, 255))
# Leather collar & rim
draw.ellipse([bag_base_x - 12, bag_base_y - 110, bag_base_x + 30, bag_base_y - 98], fill=(78, 40, 20, 255))
draw.ellipse([bag_base_x - 10, bag_base_y - 108, bag_base_x + 28, bag_base_y - 100], fill=(42, 22, 10, 255)) # bag opening
# Front zippered accessory pouch
draw.polygon([(bag_base_x - 14, bag_base_y - 25), (bag_base_x + 10, bag_base_y - 22),
              (bag_base_x + 12, bag_base_y - 78), (bag_base_x - 10, bag_base_y - 78)], fill=(115, 62, 32, 255))
draw.line([(bag_base_x - 1, bag_base_y - 24), (bag_base_x + 1, bag_base_y - 76)], fill=(220, 190, 85, 255), width=2) # brass zip
# White golf towel with eyelet
draw.polygon([(bag_base_x - 16, bag_base_y - 85), (bag_base_x - 30, bag_base_y - 48),
              (bag_base_x - 18, bag_base_y - 36), (bag_base_x - 10, bag_base_y - 78)], fill=(240, 240, 245, 255))
draw.line([(bag_base_x - 24, bag_base_y - 60), (bag_base_x - 20, bag_base_y - 40)], fill=(185, 185, 195, 255), width=1)
# Padded leather shoulder sling
draw.line([(bag_base_x + 24, bag_base_y - 95), (bag_base_x + 34, bag_base_y - 60), (bag_base_x + 18, bag_base_y - 32)], fill=(85, 45, 22, 255), width=4)

# Clubs in the bag
# Knit pom-pom woods
# 1. Driver (Navy & Crimson knit pom-pom)
draw.line([(bag_base_x, bag_base_y - 105), (bag_base_x - 12, bag_base_y - 140)], fill=(175, 180, 185, 255), width=3)
draw.ellipse([bag_base_x - 24, bag_base_y - 158, bag_base_x - 4, bag_base_y - 132], fill=(28, 55, 115, 255))
draw.ellipse([bag_base_x - 22, bag_base_y - 152, bag_base_x - 6, bag_base_y - 138], fill=(210, 42, 42, 255))
draw.ellipse([bag_base_x - 20, bag_base_y - 170, bag_base_x - 8, bag_base_y - 154], fill=(245, 245, 245, 255)) # pom pom!
# 2. 3-Wood (British Racing Green & Gold)
draw.line([(bag_base_x + 8, bag_base_y - 105), (bag_base_x + 4, bag_base_y - 134)], fill=(175, 180, 185, 255), width=3)
draw.ellipse([bag_base_x - 4, bag_base_y - 150, bag_base_x + 14, bag_base_y - 126], fill=(22, 75, 40, 255))
draw.ellipse([bag_base_x, bag_base_y - 160, bag_base_x + 10, bag_base_y - 148], fill=(245, 215, 60, 255))
# Steel Irons fanned out in bag
for ix, iy, ang in [(12, -128, -25), (20, -122, 8), (26, -115, 30)]:
    sx = bag_base_x + 10
    sy = bag_base_y - 105
    ex = bag_base_x + ix
    ey = bag_base_y + iy
    draw.line([(sx, sy), (ex, ey)], fill=(185, 190, 200, 255), width=3)
    hx = ex + int(9 * math.cos(math.radians(ang)))
    hy = ey + int(9 * math.sin(math.radians(ang)))
    draw.polygon([(ex, ey), (hx, hy), (hx + 3, hy - 4), (ex + 2, ey - 5)], fill=(225, 230, 240, 255))

# 8. The Golfer Futzing With His Clubs!
gx = 175
gy = 442

# Two-Tone Saddle Oxford Golf Shoes
# Left Shoe
draw.polygon([(gx - 22, gy), (gx + 4, gy), (gx + 2, gy - 13), (gx - 18, gy - 13)], fill=(242, 242, 248, 255))
draw.polygon([(gx - 14, gy), (gx - 3, gy), (gx - 5, gy - 13), (gx - 12, gy - 13)], fill=(32, 32, 36, 255)) # saddle
draw.rectangle([gx - 22, gy - 3, gx + 4, gy], fill=(26, 26, 28, 255)) # sole
# Right Shoe
draw.polygon([(gx + 16, gy - 6), (gx + 38, gy - 6), (gx + 36, gy - 17), (gx + 20, gy - 17)], fill=(242, 242, 248, 255))
draw.polygon([(gx + 24, gy - 6), (gx + 32, gy - 6), (gx + 31, gy - 17), (gx + 25, gy - 17)], fill=(32, 32, 36, 255))
draw.rectangle([gx + 16, gy - 9, gx + 38, gy - 6], fill=(26, 26, 28, 255))

# Folded Crew Socks with stripe
draw.rectangle([gx - 18, gy - 26, gx - 6, gy - 13], fill=(238, 238, 244, 255))
draw.line([(gx - 18, gy - 23), (gx - 6, gy - 23)], fill=(195, 38, 38, 255), width=2)
draw.rectangle([gx + 20, gy - 30, gx + 32, gy - 17], fill=(238, 238, 244, 255))
draw.line([(gx + 20, gy - 27), (gx + 32, gy - 27)], fill=(195, 38, 38, 255), width=2)

# Sun-Tanned Athletic Legs
draw.polygon([(gx - 18, gy - 26), (gx - 6, gy - 26), (gx - 4, gy - 68), (gx - 20, gy - 68)], fill=(215, 155, 105, 255))
draw.polygon([(gx - 18, gy - 26), (gx - 12, gy - 26), (gx - 12, gy - 68), (gx - 20, gy - 68)], fill=(232, 175, 125, 255)) # highlight
draw.polygon([(gx + 20, gy - 30), (gx + 32, gy - 30), (gx + 28, gy - 72), (gx + 14, gy - 72)], fill=(195, 138, 92, 255))
draw.polygon([(gx + 20, gy - 30), (gx + 26, gy - 30), (gx + 23, gy - 72), (gx + 14, gy - 72)], fill=(215, 155, 105, 255))

# Pleated Khaki Bermuda Golf Shorts
shorts_pts = [
    (gx - 26, gy - 68),
    (gx, gy - 68),
    (gx + 6, gy - 88),   # crotch
    (gx + 12, gy - 72),
    (gx + 34, gy - 72),
    (gx + 38, gy - 128), # right waist
    (gx - 22, gy - 128), # left waist
]
draw.polygon(shorts_pts, fill=(196, 172, 130, 255))
draw.polygon([(gx - 26, gy - 68), (gx - 12, gy - 68), (gx - 10, gy - 128), (gx - 22, gy - 128)], fill=(218, 194, 150, 255)) # lit side
draw.polygon([(gx + 6, gy - 78), (gx + 34, gy - 72), (gx + 38, gy - 128), (gx + 8, gy - 128)], fill=(168, 146, 106, 255)) # shadow
draw.line([(gx - 10, gy - 128), (gx - 13, gy - 68)], fill=(168, 144, 104, 255), width=1) # pleat
# Brown leather belt & gold buckle
draw.rectangle([gx - 21, gy - 129, gx + 37, gy - 124], fill=(82, 44, 22, 255))
draw.rectangle([gx + 4, gy - 130, gx + 14, gy - 123], fill=(235, 198, 72, 255))

# Torso - Classic Red Polo Shirt (leaning slightly towards bag)
shirt_pts = [
    (gx - 22, gy - 128),
    (gx + 37, gy - 128),
    (gx + 32, gy - 192), # right shoulder
    (gx - 4, gy - 200),  # neck
    (gx - 34, gy - 188), # left shoulder
]
draw.polygon(shirt_pts, fill=(195, 34, 34, 255))
draw.polygon([(gx - 22, gy - 128), (gx + 8, gy - 128), (gx + 2, gy - 196), (gx - 34, gy - 188)], fill=(226, 48, 48, 255)) # lit front
draw.polygon([(gx + 8, gy - 128), (gx + 37, gy - 128), (gx + 32, gy - 192), (gx + 2, gy - 196)], fill=(158, 24, 24, 255)) # shadow back
# Crisp white collar & buttons
draw.polygon([(gx - 10, gy - 198), (gx - 4, gy - 200), (gx + 6, gy - 198), (gx + 8, gy - 190), (gx - 14, gy - 190)], fill=(245, 245, 250, 255))
draw.line([(gx - 3, gy - 190), (gx - 3, gy - 168)], fill=(145, 18, 18, 255), width=2)
draw.ellipse([gx - 4, gy - 184, gx - 1, gy - 181], fill=(245, 245, 245, 255))
draw.ellipse([gx - 4, gy - 176, gx - 1, gy - 173], fill=(245, 245, 245, 255))

# Left Arm (bent, gloved hand resting casually on bag rim)
draw.polygon([(gx - 34, gy - 188), (gx - 20, gy - 188), (gx - 32, gy - 146), (gx - 42, gy - 146)], fill=(205, 38, 38, 255)) # sleeve
draw.line([(gx - 40, gy - 146), (gx - 30, gy - 146)], fill=(245, 245, 250, 255), width=2) # white sleeve stripe
draw.polygon([(gx - 40, gy - 146), (gx - 30, gy - 146), (gx - 22, gy - 106), (gx - 30, gy - 106)], fill=(215, 155, 105, 255)) # forearm
# White Cabretta leather golf glove on bag
draw.ellipse([gx - 34, gy - 110, gx - 18, gy - 96], fill=(245, 245, 250, 255))
draw.line([(gx - 30, gy - 104), (gx - 22, gy - 104)], fill=(38, 38, 42, 255), width=1) # black velcro closure

# Head & Visor
head_x = gx - 2
head_y = gy - 210
draw.rectangle([head_x - 8, head_y + 4, head_x + 8, head_y + 14], fill=(205, 145, 95, 255)) # neck
draw.polygon([(head_x - 12, head_y), (head_x + 12, head_y), (head_x + 10, head_y + 12),
              (head_x + 4, head_y + 16), (head_x - 10, head_y + 14)], fill=(230, 168, 118, 255))
# Thoughtful expression looking at the club
draw.line([(head_x + 4, head_y + 2), (head_x + 9, head_y + 2)], fill=(42, 28, 18, 255), width=2)
draw.ellipse([head_x + 2, head_y + 4, head_x + 8, head_y + 9], fill=(245, 185, 135, 255))
draw.ellipse([head_x - 12, head_y + 2, head_x - 7, head_y + 10], fill=(215, 150, 100, 255)) # ear
# White visor
draw.ellipse([head_x - 14, head_y - 12, head_x + 14, head_y], fill=(245, 245, 250, 255))
draw.polygon([(head_x - 4, head_y - 2), (head_x + 24, head_y + 1),
              (head_x + 22, head_y - 3), (head_x + 8, head_y - 6)], fill=(235, 235, 240, 255))
draw.line([(head_x - 2, head_y + 1), (head_x + 22, head_y + 2)], fill=(28, 78, 42, 255), width=2)

# Right Arm & The Club He's Futzing With!
draw.polygon([(gx + 32, gy - 192), (gx + 16, gy - 196), (gx + 40, gy - 152), (gx + 52, gy - 154)], fill=(175, 26, 26, 255))
draw.line([(gx + 40, gy - 152), (gx + 52, gy - 154)], fill=(245, 245, 250, 255), width=2)
draw.polygon([(gx + 40, gy - 152), (gx + 52, gy - 154), (gx + 42, gy - 112), (gx + 32, gy - 114)], fill=(215, 155, 105, 255))
draw.ellipse([gx + 30, gy - 116, gx + 44, gy - 102], fill=(225, 162, 112, 255)) # hand

# The 7-Iron Club being examined!
club_grip_x = gx + 36
club_grip_y = gy - 110
club_head_x = gx + 50
club_head_y = gy - 238
draw.line([(club_grip_x, club_grip_y), (club_head_x, club_head_y)], fill=(198, 204, 214, 255), width=3)
draw.line([(club_grip_x, club_grip_y), (club_head_x, club_head_y)], fill=(250, 252, 255, 255), width=1) # glint
draw.line([(club_grip_x - 4, club_grip_y + 15), (club_grip_x + 2, club_grip_y - 8)], fill=(32, 32, 36, 255), width=5) # grip
# Iron Head
head_poly = [
    (club_head_x - 4, club_head_y),
    (club_head_x + 20, club_head_y - 8),
    (club_head_x + 24, club_head_y - 20),
    (club_head_x + 3, club_head_y - 14)
]
draw.polygon(head_poly, fill=(238, 242, 250, 255))
draw.line([(club_head_x + 5, club_head_y - 6), (club_head_x + 18, club_head_y - 11)], fill=(135, 140, 155, 255), width=1)
draw.line([(club_head_x + 6, club_head_y - 9), (club_head_x + 19, club_head_y - 14)], fill=(135, 140, 155, 255), width=1)
draw.line([(club_head_x + 7, club_head_y - 12), (club_head_x + 20, club_head_y - 17)], fill=(135, 140, 155, 255), width=1)
draw.line([(club_head_x + 20, club_head_y - 8), (club_head_x + 24, club_head_y - 20)], fill=(255, 255, 255, 255), width=2) # toe shine

# 9. STYLIZED 3D 'DUFFER' TITLE LOGO
def draw_3d_letter_gradient(pts, depth=8):
    # Deep drop shadow
    for d in range(depth, 0, -1):
        sh_pts = [(x + d, y + d) for (x, y) in pts]
        draw.polygon(sh_pts, fill=(10, 24, 14, 255))
    # Bevel under-layer (rich dark bronze)
    draw.polygon([(x + 2, y + 2) for (x, y) in pts], fill=(130, 85, 18, 255))
    # Golden face
    draw.polygon(pts, fill=(248, 218, 92, 255))
    # Top edge highlight
    draw.line(pts[:len(pts)//2], fill=(255, 255, 230, 255), width=2)

def letter_D(lx, ly):
    return [(lx, ly), (lx+26, ly), (lx+36, ly+10), (lx+36, ly+42), (lx+26, ly+52), (lx, ly+52), (lx, ly),
            (lx+12, ly+12), (lx+22, ly+12), (lx+25, ly+18), (lx+25, ly+34), (lx+22, ly+40), (lx+12, ly+40), (lx+12, ly+12)]
def letter_U(lx, ly):
    return [(lx, ly), (lx+12, ly), (lx+12, ly+38), (lx+24, ly+38), (lx+24, ly), (lx+36, ly), (lx+36, ly+42), (lx+26, ly+52), (lx+10, ly+52), (lx, ly+42), (lx, ly)]
def letter_F(lx, ly):
    return [(lx, ly), (lx+34, ly), (lx+34, ly+12), (lx+12, ly+12), (lx+12, ly+22), (lx+28, ly+22), (lx+28, ly+32), (lx+12, ly+32), (lx+12, ly+52), (lx, ly+52), (lx, ly)]
def letter_E(lx, ly):
    return [(lx, ly), (lx+34, ly), (lx+34, ly+12), (lx+12, ly+12), (lx+12, ly+20), (lx+28, ly+20), (lx+28, ly+30), (lx+12, ly+30), (lx+12, ly+40), (lx+34, ly+40), (lx+34, ly+52), (lx, ly+52), (lx, ly)]
def letter_R(lx, ly):
    return [(lx, ly), (lx+26, ly), (lx+36, ly+10), (lx+36, ly+24), (lx+26, ly+32), (lx+36, ly+52), (lx+22, ly+52), (lx+16, ly+32), (lx+12, ly+32), (lx+12, ly+52), (lx, ly+52), (lx, ly)]

logo_base_x = 270
logo_base_y = 48
draw_3d_letter_gradient(letter_D(logo_base_x, logo_base_y))
draw_3d_letter_gradient(letter_U(logo_base_x + 44, logo_base_y))
draw_3d_letter_gradient(letter_F(logo_base_x + 88, logo_base_y))
draw_3d_letter_gradient(letter_F(logo_base_x + 130, logo_base_y))
draw_3d_letter_gradient(letter_E(logo_base_x + 172, logo_base_y))
draw_3d_letter_gradient(letter_R(logo_base_x + 214, logo_base_y))

# Dimpled Golf Ball next to logo
gb_x = logo_base_x + 272
gb_y = logo_base_y + 35
# Drop shadow
draw.ellipse([gb_x - 7, gb_y - 2, gb_x + 11, gb_y + 12], fill=(12, 28, 15, 180))
draw.ellipse([gb_x - 9, gb_y - 9, gb_x + 9, gb_y + 9], fill=(245, 245, 252, 255))
draw.ellipse([gb_x - 7, gb_y - 7, gb_x - 3, gb_y - 3], fill=(255, 255, 255, 255)) # highlight
draw.arc([gb_x - 9, gb_y - 9, gb_x + 9, gb_y + 9], 45, 225, fill=(175, 180, 195, 255), width=2)
# Dimple dots
for ddx, ddy in [(-2, 0), (2, 2), (-1, 3), (3, -2), (0, -3), (4, 1)]:
    draw.point((gb_x + ddx, gb_y + ddy), fill=(205, 210, 220, 255))

# Subtitle Ribbon Banner beneath logo
rib_x1 = logo_base_x - 14
rib_y1 = logo_base_y + 66
rib_x2 = logo_base_x + 296
rib_y2 = logo_base_y + 92
# Drop shadow
draw.rectangle([rib_x1 + 4, rib_y1 + 4, rib_x2 + 4, rib_y2 + 4], fill=(12, 26, 15, 210))
# Deep forest green ribbon
draw.rectangle([rib_x1, rib_y1, rib_x2, rib_y2], fill=(20, 62, 34, 255))
draw.rectangle([rib_x1 + 2, rib_y1 + 2, rib_x2 - 2, rib_y2 - 2], outline=(235, 198, 75, 255), width=2)

img.save("docs/screenshots/splash_preview.png")
print("Refined splash_preview.png saved successfully!")

