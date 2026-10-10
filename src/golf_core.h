#ifndef GOLF_CORE_H
#define GOLF_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include "fixed_math.h"

/* Version Identification
 * Versioning Protocol:
 * - Current Release: 0.75-noonan (SemVer base: 0.7.5)
 * - Step Changes: Increment 0.0.xx for each step (0.7.5 -> 0.7.6 -> 0.7.7...)
 * - Rollover: When reaching x.x.99, increment middle step (e.g., 0.7.99 -> 0.8.0)
 * - Formal Releases: Published strictly on multiples of 5 (0.7.5, 0.7.10, 0.7.15...)
 */
#define DUFFER_VERSION      "0.75-noonan"
#define DUFFER_VERSION_NAME "0.75-noonan"
#define DUFFER_SEMVER       "0.7.5"

/* Grid Dimensions */
#define GRID_W              32
#define GRID_H              64
#define YARDS_PER_TILE      7      /* ~7 yards per tile (32x64 hole spans ~150-450 yds) */
#define HEIGHT_STEP_PX      12     /* 12 pixels per discrete elevation tier */
#define MAX_ELEVATION_TIER  6      /* 0..6 discrete tiers */

/* Elevation Tiers */
typedef enum {
    ELEV_WATER      = 0,
    ELEV_LOWLAND    = 1,
    ELEV_VALLEY     = 2,
    ELEV_RIDGE      = 3,
    ELEV_PLATEAU    = 4,
    ELEV_HILLTOP    = 5,
    ELEV_PEAK       = 6
} ElevationTier;

/* Surface Materials */
typedef enum {
    SURF_WATER    = 0,
    SURF_ROUGH    = 1,
    SURF_FAIRWAY  = 2,
    SURF_GREEN    = 3,
    SURF_SAND     = 4,
    SURF_TEE      = 5,
    SURF_CARTPATH = 6,
    SURF_BRIDGE   = 7,
    SURF_COUNT
} SurfaceType;

/* Natural Features / Buildings / Obstacles */
typedef enum {
    FEAT_NONE = 0,
    FEAT_PINE = 1,
    FEAT_OAK = 2,
    FEAT_PALM = 3,
    FEAT_FLOWERING = 4,
    FEAT_BUSH = 5,
    FEAT_FLOWERS = 6,
    FEAT_ROCK = 7,
    FEAT_BENCH = 8,
    FEAT_CART = 9,
    FEAT_CLUBHOUSE = 10,  /* Occupied by clubhouse */
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
    uint8_t elevation;      /* 0..MAX_ELEVATION_TIER */
    uint8_t surface;        /* SurfaceType */
    uint8_t feature;        /* FeatureType */
    uint8_t autotile_mask;  /* 0..15 edge transition mask */
    uint8_t variant;        /* Subtle aesthetic variation (0..3) */
} GolfTile;

/* Course Themes / Color Schemes */
typedef enum {
    THEME_BUSHWOOD = 0,     /* Classic country club greens & whites */
    THEME_LA_PLAYA,         /* Desert beige, cactus, tumbleweeds, Burning Man */
    THEME_NOCTURNAL_GREEN,  /* Transylvania purples & greys, fall leaves, coffins, zombies */
    THEME_COUNT
} CourseTheme;

/* Game Modes */
typedef enum {
    MODE_SPLASH = 0,
    MODE_SETUP,
    MODE_PLAY,
    MODE_BUILD
} GameMode;

/* Build Category Tabs for Design Mode */
typedef enum {
    BUILD_CAT_TERRAIN = 0,
    BUILD_CAT_COURSE,
    BUILD_CAT_SCENERY,
    BUILD_CAT_COUNT
} BuildCategory;

/* Build Tools */
typedef enum {
    /* Terrain Tab */
    TOOL_FAIRWAY = 0,
    TOOL_GREEN,
    TOOL_SAND,
    TOOL_ROUGH,
    TOOL_WATER,
    TOOL_CARTPATH,
    TOOL_RAISE,
    TOOL_LOWER,
    TOOL_BULLDOZE,
    
    /* Course Tab */
    TOOL_TEE,
    TOOL_PIN,
    TOOL_BRIDGE,
    TOOL_BENCH,
    TOOL_CART,
    
    /* Scenery Tab */
    TOOL_PINE,
    TOOL_OAK,
    TOOL_PALM,
    TOOL_FLOWERING,
    TOOL_BUSH,
    TOOL_FLOWERS,
    TOOL_ROCK,
    TOOL_CLUBHOUSE,
    
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

/* Golfer Character State */
typedef enum {
    GOLFER_STATE_READY = 0, /* Standing beside ball ready to address & swing */
    GOLFER_STATE_SWINGING,  /* In swing motion */
    GOLFER_STATE_WATCHING,  /* Watching shot flight / roll */
    GOLFER_STATE_WALKING    /* Walking to the ball */
} GolferState;

typedef struct {
    fixed x, y;             /* Current position in grid coordinates (16.16 fixed) */
    fixed current_z;        /* Smoothed ground elevation (16.16 fixed) */
    fixed target_x, target_y;
    GolferState state;
    uint8_t facing_angle;   /* 0..255 */
    int32_t walk_anim_frame;/* 0..3 */
    int32_t walk_ticks;
    int32_t step_timer;
} GolferEntity;

/* Caddy Character Entity */
typedef enum {
    CADDY_STATE_READY = 0,
    CADDY_STATE_FOLLOWING,
    CADDY_STATE_CELEBRATING,
    CADDY_STATE_DISAPPOINTED
} CaddyState;

typedef struct {
    fixed x, y;
    fixed current_z;
    CaddyState state;
    uint8_t facing_angle;
    int32_t walk_anim_frame;
    int32_t anim_timer;
    int32_t mood_timer;
} CaddyEntity;

/* Spectator / Gallery Entity */
#define MAX_SPECTATORS 24

typedef enum {
    SPEC_STATE_IDLE = 0,
    SPEC_STATE_WATCHING,
    SPEC_STATE_CLAPPING,
    SPEC_STATE_CHEERING
} SpectatorState;

typedef struct {
    int32_t gx, gy;
    fixed current_z;
    SpectatorState state;
    uint8_t shirt_color_idx; /* 0..5 */
    uint8_t facing_angle;
    int32_t anim_frame;
    int32_t timer;
    bool active;
} SpectatorEntity;

/* Beverage Cart Entity */
typedef enum {
    BEV_STATE_DRIVING = 0,
    BEV_STATE_SERVICING,
    BEV_STATE_PAUSED
} BeverageCartState;

typedef struct {
    fixed x, y;             /* Continuous grid position (16.16 fixed) */
    fixed current_z;
    fixed target_x, target_y;
    BeverageCartState state;
    uint8_t facing_angle;
    fixed speed;
    int32_t whistle_timer;  /* Ticks until next cheerful whistle */
    int32_t speech_timer;   /* Speech bubble "COLD DRINKS!" */
    int32_t wait_timer;
    bool active;
} BeverageCartEntity;

/* Gopher Entity & Hole Hazards (Caddyshack Style) */
#define MAX_GOPHER_HOLES 16

typedef enum {
    GOPHER_STATE_HIDDEN = 0,    /* Underground waiting for timer */
    GOPHER_STATE_POPPING_UP,   /* Rising from dirt mound (frames 0..1) */
    GOPHER_STATE_TAUNTING,     /* Dancing / laughing / head bopping (frames 2..3) */
    GOPHER_STATE_RETREATING    /* Ducking back down */
} GopherState;

typedef struct {
    int32_t gx, gy;            /* Tile coordinates of gopher hole */
    fixed current_z;           /* Ground elevation */
    bool active;
} GopherHole;

typedef struct {
    fixed x, y;                /* Continuous course position (16.16 fixed) */
    fixed current_z;
    int32_t gx, gy;            /* Current tile */
    GopherState state;
    int32_t anim_frame;        /* Frame index: 0..3 */
    int32_t timer;             /* Ticks in current state */
    int32_t pop_cooldown;      /* Ticks until next random pop-up */
    bool visible;              /* True if gopher body is above ground */
    GopherHole holes[MAX_GOPHER_HOLES];
    int32_t hole_count;
} GopherEntity;

/* Carl Spackler Greenskeeper Entity (Caddyshack Nemesis) */
typedef enum {
    CARL_STATE_INACTIVE = 0,
    CARL_STATE_APPROACHING,     /* Sneaking toward gopher hole */
    CARL_STATE_DYNAMITE_PLANT,  /* Placing TNT plunger box & wire */
    CARL_STATE_DYNAMITE_PLUNGE, /* Pushing plunger handle down -> BOOM! */
    CARL_STATE_HOSE_SPRAY,      /* Blasting water hose into burrow -> FLOOD! */
    CARL_STATE_CELEBRATING,     /* Taunting/admiring damage ("Cinderella story!") */
    CARL_STATE_LEAVING          /* Walking off */
} CarlState;

typedef enum {
    CARL_ATTACK_DYNAMITE = 0,
    CARL_ATTACK_HOSE
} CarlAttackType;

typedef struct {
    fixed x, y;                 /* Continuous course position (16.16 fixed) */
    fixed current_z;
    fixed start_x, start_y;     /* Entry and exit position */
    fixed plunger_x, plunger_y; /* Plunger detonator box position */
    int32_t target_gx, target_gy;/* Target gopher hole */
    CarlState state;
    CarlAttackType attack_type;
    int32_t timer;              /* Ticks in current phase */
    int32_t anim_frame;         /* Animation frame */
    bool active;
    bool triggered_effect;      /* True once crater or flood has occurred */
} CarlEntity;

/* Audio Events Triggered by Physics & Actions */
typedef enum {
    AUDIO_EVT_NONE = 0,
    AUDIO_EVT_SWING_DRIVER = 1,
    AUDIO_EVT_SWING_IRON = 2,
    AUDIO_EVT_SWING_WEDGE = 3,
    AUDIO_EVT_SWING_PUTTER = 4,
    AUDIO_EVT_BALL_BOUNCE = 5,
    AUDIO_EVT_TREE_HIT = 6,
    AUDIO_EVT_ROCK_HIT = 7,
    AUDIO_EVT_WATER_SPLASH = 8,
    AUDIO_EVT_FOOTSTEP_GRASS = 9,
    AUDIO_EVT_FOOTSTEP_SAND = 10,
    AUDIO_EVT_FOOTSTEP_ROUGH = 11,
    AUDIO_EVT_FOOTSTEP_WOOD = 12,
    AUDIO_EVT_CUP_SINK = 13,
    AUDIO_EVT_SWEETSPOT = 14,
    AUDIO_EVT_APPLAUSE_GOLF = 15,
    AUDIO_EVT_CROWD_CHEER = 16,
    AUDIO_EVT_CROWD_OOH = 17,
    AUDIO_EVT_BUILD_PLACE = 18,
    AUDIO_EVT_TERRAIN_SCULPT = 19,
    AUDIO_EVT_BEV_WHISTLE = 20,
    AUDIO_EVT_GOPHER_CHUCKLE = 21,
    AUDIO_EVT_GOPHER_GULP = 22,
    AUDIO_EVT_CARL_EXPLOSION = 23,
    AUDIO_EVT_CARL_HOSE = 24,
    AUDIO_EVT_SIREN = 25,
    AUDIO_EVT_SHOTGUN = 26,
    AUDIO_EVT_CHAINSAW = 27,
    AUDIO_EVT_ZOMBIE = 28,
    AUDIO_EVT_SEAMONSTER = 29
} GolfAudioEvent;

/* Tumbleweeds for La Playa Desert Theme */
#define MAX_TUMBLEWEEDS 8
typedef struct {
    fixed x, y;
    fixed vx, vy;
    fixed current_z;
    int32_t rotation;
    bool active;
    bool stuck;
} TumbleweedEntity;

/* Sea Monster for Nocturnal Green Theme */
typedef struct {
    fixed x, y;
    fixed current_z;
    int32_t state;       /* 0: hidden, 1: eyes/head peeking, 2: tentacle swiping ball */
    int32_t timer;
    int32_t anim_frame;
    bool active;
} SeaMonsterEntity;

/* Course Ranger Entity for Sneak On Mode */
typedef enum {
    RANGER_STATE_INACTIVE = 0,
    RANGER_STATE_PATROLLING,
    RANGER_STATE_CHASING,
    RANGER_STATE_SCOLDING
} RangerState;

typedef struct {
    fixed x, y;
    fixed current_z;
    fixed target_x, target_y;
    RangerState state;
    uint8_t facing_angle;
    fixed speed;
    int32_t timer;
    int32_t speech_timer;
    int32_t cooldown;
    char speech_msg[64];
    bool active;
} RangerEntity;

/* Wind system */
typedef struct {
    uint8_t angle;          /* 0..255 direction */
    fixed speed;            /* 0..30 mph in fixed units */
} Wind;

/* Full Game Simulation State */
typedef struct {
    GolfTile grid[GRID_W][GRID_H];
    
    /* Routing coordinates */
    int32_t tee_gx, tee_gy;
    int32_t pin_gx, pin_gy;
    bool has_tee;
    bool has_pin;
    
    /* Clubhouse Footprint */
    int32_t clubhouse_gx, clubhouse_gy;
    bool has_clubhouse;
    
    /* Score & Course Stats */
    int32_t hole_yards;
    int32_t par;
    int32_t strokes;
    int32_t score_rel_par;  /* strokes - par */
    bool hole_completed;
    
    /* SimGolf Hole Rating & Economics */
    int32_t fun_rating;     /* 0..100 (maps to 0..5 stars) */
    int32_t difficulty_rating; /* 1..10 */
    int32_t club_funds;     /* Dollars */
    
    /* Course Theme & Generation Scales (1..5) */
    CourseTheme theme;
    int32_t tree_scale;     /* 1..5, default 3 */
    int32_t bush_scale;     /* 1..5, default 3 */
    int32_t rock_scale;     /* 1..5, default 3 */
    int32_t water_scale;    /* 1..5, default 3 */
    bool is_sneak_on;       /* True if sneak-on mode */
    uint32_t current_seed;

    /* Replay Hole Snapshot */
    GolfTile initial_grid[GRID_W][GRID_H];
    int32_t initial_tee_gx, initial_tee_gy;
    int32_t initial_pin_gx, initial_pin_gy;
    int32_t initial_ch_gx, initial_ch_gy;
    bool initial_has_ch;

    /* Game State */
    GameMode mode;
    BuildCategory build_category;
    BuildTool current_tool;
    int32_t brush_size;     /* 1 or 2 */
    
    /* Player in Play Mode */
    ClubType current_club;
    uint8_t aim_angle;      /* 0..255 */
    fixed aim_target_x;     /* Aim landing spot in grid units (fixed 16.16) */
    fixed aim_target_y;
    int32_t aim_distance_yds; /* Distance in yards to aim target */
    bool has_custom_aim;    /* True if player explicitly placed aim spot */
    int32_t shot_origin_gx; /* Launch tile coordinates to allow hitting out of obstacles */
    int32_t shot_origin_gy;
    SwingGauge swing;
    BallEntity ball;
    GolferEntity golfer;
    CaddyEntity caddy;
    SpectatorEntity spectators[MAX_SPECTATORS];
    int32_t spectator_count;
    BeverageCartEntity bev_cart;
    GopherEntity gopher;
    CarlEntity carl;
    int32_t gopher_encounter_count;
    int32_t carl_cooldown;

    /* Theme Entities & Event Timers */
    TumbleweedEntity tumbleweeds[MAX_TUMBLEWEEDS];
    SeaMonsterEntity sea_monster;
    RangerEntity ranger;
    bool caddy_eaten;
    int32_t caddy_eaten_timer;
    int32_t raver_passed_out_timer;
    
    Wind wind;
    GolfAudioEvent audio_event; /* Audio event generated this tick */
    
    /* Lie at ball starting position */
    int32_t prev_ball_gx, prev_ball_gy;
    
    /* Notification banner */
    char banner_text[64];
    int32_t banner_timer;   /* in ticks */
} GolfState;

/* Core Function Prototypes */
void golf_init(GolfState *gs);
void golf_generate_terrain(GolfState *gs, uint32_t seed);
void golf_generate_terrain_scaled(GolfState *gs, uint32_t seed, int trees, int bushes, int rocks, int water);
void golf_place_default_routing(GolfState *gs);
void golf_randomize_hole(GolfState *gs, uint32_t seed);
void golf_replay_hole(GolfState *gs);
void golf_start_sneak_on(GolfState *gs);
void golf_set_theme(GolfState *gs, CourseTheme theme);
void golf_update_autotile_masks(GolfState *gs);
void golf_recalc_hole_stats(GolfState *gs);
void golf_recalc_course_rating(GolfState *gs);

/* Build Mode Operations */
bool golf_build_apply_tool(GolfState *gs, int32_t gx, int32_t gy);
void golf_set_tool(GolfState *gs, BuildTool tool);
void golf_set_build_category(GolfState *gs, BuildCategory cat);
bool golf_raise_terrain(GolfState *gs, int32_t gx, int32_t gy, int32_t brush_size);
bool golf_lower_terrain(GolfState *gs, int32_t gx, int32_t gy, int32_t brush_size);
bool golf_place_clubhouse(GolfState *gs, int32_t gx, int32_t gy);
void golf_remove_clubhouse(GolfState *gs);

/* Play Mode Operations */
void golf_set_mode(GolfState *gs, GameMode mode);
void golf_select_club(GolfState *gs, ClubType club);
void golf_change_club(GolfState *gs, int delta);
void golf_adjust_aim(GolfState *gs, int delta_angle);
void golf_set_aim_target(GolfState *gs, fixed target_x, fixed target_y);
void golf_swing_click(GolfState *gs);
void golf_reset_ball_to_tee(GolfState *gs);

/* Physics & Simulation Tick (60 Hz deterministic fixed-point) */
void golf_tick(GolfState *gs);

/* Helper queries */
fixed golf_get_ground_z_fixed(const GolfState *gs, fixed fx, fixed fy);
SurfaceType golf_get_surface_at(const GolfState *gs, fixed fx, fixed fy);
int32_t golf_distance_to_pin_yards(const GolfState *gs);
const ClubData* golf_get_club_data(ClubType club);
int golf_get_water_proximity(const GolfState *gs, fixed view_x, fixed view_y);
int golf_get_clubhouse_proximity(const GolfState *gs, fixed view_x, fixed view_y);
void golf_golfer_arrive_at_ball(GolfState *gs);

#endif /* GOLF_CORE_H */
