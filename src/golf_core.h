#ifndef GOLF_CORE_H
#define GOLF_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include "fixed_math.h"

/* Grid Dimensions */
#define GRID_W              32
#define GRID_H              64
#define YARDS_PER_TILE      7      /* ~7 yards per tile (32x64 hole spans ~150-450 yds) */
#define HEIGHT_STEP_PX      12     /* 12 pixels per discrete elevation tier */

/* Elevation Tiers */
typedef enum {
    ELEV_WATER   = 0,
    ELEV_LOWLAND = 1,
    ELEV_RIDGE   = 2,
    ELEV_PLATEAU = 3
} ElevationTier;

/* Surface Materials */
typedef enum {
    SURF_WATER   = 0,
    SURF_ROUGH   = 1,
    SURF_FAIRWAY = 2,
    SURF_GREEN   = 3,
    SURF_SAND    = 4,
    SURF_TEE     = 5,
    SURF_COUNT
} SurfaceType;

/* Natural Features / Obstacles */
typedef enum {
    FEAT_NONE = 0,
    FEAT_PINE,
    FEAT_OAK,
    FEAT_ROCK,
    FEAT_COUNT
} FeatureType;

/* Autotile Masks (4 cardinal diamond edges: NW, NE, SE, SW)
 * Bit 3: NW, Bit 2: NE, Bit 1: SE, Bit 0: SW */
#define MASK_NW (1 << 3)
#define MASK_NE (1 << 2)
#define MASK_SE (1 << 1)
#define MASK_SW (1 << 0)

/* Tile representation */
typedef struct {
    uint8_t elevation;      /* 0..3 */
    uint8_t surface;        /* SurfaceType */
    uint8_t feature;        /* FeatureType */
    uint8_t autotile_mask;  /* 0..15 edge transition mask */
    uint8_t variant;        /* Subtle aesthetic variation (0..3) */
} GolfTile;

/* Game Modes */
typedef enum {
    MODE_SPLASH = 0,
    MODE_PLAY,
    MODE_BUILD
} GameMode;

/* Build Tools */
typedef enum {
    TOOL_FAIRWAY = 0,
    TOOL_GREEN,
    TOOL_SAND,
    TOOL_ROUGH,
    TOOL_TEE,
    TOOL_PIN,
    TOOL_PINE,
    TOOL_OAK,
    TOOL_ROCK,
    TOOL_BULLDOZE,
    TOOL_COUNT
} BuildTool;

/* Golf Clubs */
typedef enum {
    CLUB_1W = 0,   /* Driver */
    CLUB_3W,       /* 3 Wood */
    CLUB_5I,       /* 5 Iron */
    CLUB_9I,       /* 9 Iron */
    CLUB_PW,       /* Pitching Wedge */
    CLUB_PT,       /* Putter */
    CLUB_COUNT
} ClubType;

typedef struct {
    const char *name;
    const char *short_name;
    int32_t nominal_yards;
    fixed launch_speed_h;     /* Horizontal speed in tiles/tick (fixed 16.16) */
    fixed launch_speed_v;     /* Vertical speed in pixels/tick (fixed 16.16) */
    fixed restitution;        /* Bounce damping factor (fixed 16.16) */
    fixed roll_friction;      /* Roll friction multiplier (fixed 16.16) */
    bool is_putter;
} ClubData;

/* Ball Flight Physics State */
typedef enum {
    BALL_IDLE = 0,
    BALL_IN_FLIGHT,
    BALL_BOUNCING,
    BALL_ROLLING,
    BALL_HOLED,
    BALL_WATER,
    BALL_OUT_OF_BOUNDS
} BallState;

typedef struct {
    fixed x, y, z;          /* Position: x,y in grid units (fixed); z in vertical px (fixed) */
    fixed vx, vy, vz;       /* Velocities per simulation tick */
    BallState state;
    fixed ground_z;         /* Ground height at current (x,y) */
    SurfaceType current_surf;
    int32_t air_ticks;
    int32_t bounce_count;
    fixed spin_rate;
} BallEntity;

/* 3-Click Swing Gauge */
typedef enum {
    SWING_STATE_READY = 0,
    SWING_STATE_POWER,       /* Moving up towards 100% */
    SWING_STATE_ACCURACY,    /* Moving down past sweetspot */
    SWING_STATE_RESOLVED     /* Shot triggered */
} SwingGaugeState;

typedef struct {
    SwingGaugeState state;
    fixed cursor_pos;       /* 0 to FIXED_ONE (0..100%) */
    fixed cursor_speed;     /* Speed per tick */
    fixed locked_power;     /* Locked power (0..FIXED_ONE) */
    fixed locked_accuracy;  /* Locked snap offset (-FIXED_ONE to +FIXED_ONE; 0 = sweetspot) */
    bool moving_up;
    int32_t sweetspot_center; /* Position in gauge (fixed) */
    int32_t sweetspot_width;  /* Snap tolerance (fixed) */
} SwingGauge;

/* Wind system */
typedef struct {
    uint8_t angle;          /* 0..255 direction */
    fixed speed;            /* 0..30 mph in fixed units */
} Wind;

/* Full Game Simulation State (Zero OS/Renderer dependencies) */
typedef struct {
    GolfTile grid[GRID_W][GRID_H];
    
    /* Routing coordinates */
    int32_t tee_gx, tee_gy;
    int32_t pin_gx, pin_gy;
    bool has_tee;
    bool has_pin;
    
    /* Score & Course Stats */
    int32_t hole_yards;
    int32_t par;
    int32_t strokes;
    int32_t score_rel_par;  /* strokes - par */
    bool hole_completed;
    
    /* Game State */
    GameMode mode;
    BuildTool current_tool;
    int32_t brush_size;     /* 1 or 2 */
    
    /* Player in Play Mode */
    ClubType current_club;
    uint8_t aim_angle;      /* 0..255 (0 = East, 64 = South, 128 = West, 192 = North) */
    SwingGauge swing;
    BallEntity ball;
    Wind wind;
    
    /* Lie at ball starting position */
    int32_t prev_ball_gx, prev_ball_gy;
    
    /* Notification banner */
    char banner_text[64];
    int32_t banner_timer;   /* in ticks */
} GolfState;

/* Core Function Prototypes */
void golf_init(GolfState *gs);
void golf_generate_terrain(GolfState *gs, uint32_t seed);
void golf_place_default_routing(GolfState *gs);
void golf_randomize_hole(GolfState *gs, uint32_t seed);
void golf_update_autotile_masks(GolfState *gs);
void golf_recalc_hole_stats(GolfState *gs);

/* Build Mode Operations */
bool golf_build_apply_tool(GolfState *gs, int32_t gx, int32_t gy);
void golf_set_tool(GolfState *gs, BuildTool tool);

/* Play Mode Operations */
void golf_set_mode(GolfState *gs, GameMode mode);
void golf_select_club(GolfState *gs, ClubType club);
void golf_change_club(GolfState *gs, int delta);
void golf_adjust_aim(GolfState *gs, int delta_angle);
void golf_swing_click(GolfState *gs);
void golf_reset_ball_to_tee(GolfState *gs);

/* Physics & Simulation Tick (60 Hz deterministic fixed-point) */
void golf_tick(GolfState *gs);

/* Helper queries */
fixed golf_get_ground_z_fixed(const GolfState *gs, fixed fx, fixed fy);
SurfaceType golf_get_surface_at(const GolfState *gs, fixed fx, fixed fy);
int32_t golf_distance_to_pin_yards(const GolfState *gs);
const ClubData* golf_get_club_data(ClubType club);

#endif /* GOLF_CORE_H */

