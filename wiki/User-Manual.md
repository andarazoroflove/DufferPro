# Duffer Pro: Championship Edition – User Manual
**Version:** `0.77-noonan` (SemVer `v0.7.7`)  
**Platforms:** Windows (x86/x64) · macOS Sonoma (Universal 2: Apple Silicon & Intel) · Game Boy Advance (Bare-Metal ROM)

---

## 📖 Welcome to Bushwood Country Club

**Duffer Pro** is a high-definition 2.5D isometric golf simulation and course architect crafted in pure ANSI C (C99). Inspired by mid-2000s classics such as *Sid Meier's SimGolf*, *RollerCoaster Tycoon 3*, and the timeless humor of *Caddyshack*, Duffer Pro delivers deterministic 16.16 fixed-point ball physics, procedural dynamic soundscapes, multi-tier terrain sculpting, and three unique course environments.

---

## 🕹️ Quick Start & Installation

### Windows
1. Unzip `Duffer_Pro_v0.75-noonan.zip` or download the `windows/` folder.
2. Ensure `SDL2.dll` resides in the same directory as `duffer_pro.exe`.
3. Double-click `duffer_pro.exe` to launch.

### macOS Sonoma (11.0+)
1. Extract `Duffer_Pro_macOS_Sonoma.zip`.
2. Drag `Duffer Pro.app` to your `/Applications` folder (or run in place).
3. If running via terminal or SSH, use the portable bundle: `./duffer_pro_mac_portable/run.sh`.

### Game Boy Advance
1. Flash `duffer_pro.gba` to your flash cartridge (SuperCard SD, EverDrive GBA, EZ-Flash Omega) or open in an emulator (mGBA, VisualBoyAdvance).
2. The bundled `duffer_pro.sav` provides pre-formatted 64KB SRAM support for instant hardware compatibility.

---

## ⛳ Game Modes

### 1. Sneak On
- **Instant Action:** Launches directly onto a randomized championship layout.
- **Randomized Elements:** Random obstacle densities (1–5 scale) and random environment theme each round.
- **The Course Ranger:** Keep an eye out for the ranger patrolling in his golf cart! If you or your ball strike him, there is a 25% chance he catches you and escorts you back to the tee with a stern lecture.

### 2. Play a Hole (Custom Championship)
- **Course Setup Dashboard:** Accessible by clicking **PLAY A HOLE** on the title screen.
- **Natural Density Scaling (1–5):**
  - **Trees:** Controls density of Scots pines, English oaks, palms, or fall foliage.
  - **Bushes:** Scales azaleas, tumbleweeds, and desert shrubs.
  - **Rocks:** Sets frequency of granite boulders, desert rocks, or cemetery coffins.
  - **Water:** Expands creeks, ponds, rapids, and monster lagoons.
  - *Safety Clamp:* Natural features will never exceed 70% total map coverage, and fairways/greens are always cleanly superimposed on top.
- **Theme Selection:** Choose between **Bushwood**, **La Playa**, or **Nocturnal Green**.

### 3. Replay Hole
- Click **REPLAY HOLE** (or press `R` during play) to immediately restart the current hole layout from the tee with identical terrain, pin placement, and obstacle distribution.

### 4. Course Architect (Design Mode)
- Press `Tab` at any time to toggle between **Play Mode** and **Design Mode**.
- Build custom fairways, carve sand traps, raise plateau greens, and lay down scenic cart paths and wooden footbridges.

---

## 🏌️ Swing & Aiming Mechanics

### 3-Click Precision Swing Gauge
1. **Click 1 (`Space` or Click Ball):** Starts the backswing. The power needle begins rising toward 100%.
2. **Click 2 (`Space` or Click Screen):** Locks swing power.
   - *Auto-Bounce:* If the needle hits 100% at the far right, it automatically bounces back without penalty.
3. **Click 3 (`Space` or Click Screen):** Locks swing accuracy.
   - **Sweet Spot:** Strike near the golden bracket for a dead-straight shot.
   - **Hook / Draw:** Early snap imparts counter-clockwise hook curve.
   - **Slice / Fade:** Late snap imparts clockwise slice curve.

### Sub-Tile Aiming
- **Mouse Aiming:** Left-click or drag anywhere on the course to place the landing reticle ring.
- **Continuous Alignment:** World-to-screen inversion calculates continuous sub-tile coordinates, so the reticle lands exactly under your mouse pointer.
- **Obstacle Launch Immunity:** If your ball lands in a tile occupied by a tree, boulder, or bush, the obstacle on that tile is ignored for your next stroke. You can always punch or loft your ball cleanly out of the square without being trapped.

---

## 🎒 The Bag (Clubs & Ranges)

| Club | Short | Nominal Distance | Carry / Roll Profile | Ideal Surface |
| :--- | :---: | :---: | :--- | :--- |
| **Driver** | **DR** | **260 yds** | Maximum distance, low piercing launch, ~220 yd carry | Tee Box |
| **1-Wood** | **1W** | **240 yds** | Penetrating flight, long rollout | Tee Box / Good Fairway Lie |
| **2-Wood** | **2W** | **225 yds** | Long fairway wood, stable wind penetration | Fairway |
| **3-Wood** | **3W** | **210 yds** | High sweeping flight, fairway approach | Fairway / Light Rough |
| **4-Iron** | **4I** | **195 yds** | Powerful long iron, medium rollout | Fairway |
| **5-Iron** | **5I** | **180 yds** | Mid-distance approach, responsive bite | Fairway |
| **6-Iron** | **6I** | **165 yds** | Controlled approach, standard trajectory | Fairway / Light Rough |
| **7-Iron** | **7I** | **150 yds** | Workhorse scoring iron, fast stopping power | Fairway / Rough |
| **8-Iron** | **8I** | **135 yds** | Steep descent, tight dispersion | Fairway / Rough |
| **9-Iron** | **9I** | **120 yds** | High loft, drop-and-stop landing angle | Fairway / Rough |
| **Pitching Wedge** | **PW** | **105 yds** | High trajectory, pin attack wedge | Fairway / Rough / Fringe |
| **Sand Wedge** | **SW** | **85 yds** | High spin pop-up, bunker escape specialist | Bunkers / Sand / Heavy Rough |
| **Lob Wedge** | **LW** | **65 yds** | Steep flop shot, zero rollout | Greenside Rough / Bunkers |
| **Putter** | **PT** | **25 yds** | Smooth ground roll, true green speed tracking | Green / Fringe |

*Note:* When aiming at a custom spot shorter than club maximum, swing power automatically calibrates so a sweet-spot stroke stops right at your reticle. Direct club cycling is available via `L` / `R` (GBA) or `C` (PC).

---

## 🎨 Environmental Themes

### 🌳 Bushwood
- **Atmosphere:** Classic American country club resort.
- **Clubhouse:** Georgian neoclassical estate with slate dormers, colonnade portico, brick chimneys, and patio veranda.
- **The Caddyshack:** Red-and-black shirt caddy crew with white slacks hanging out near the wooden Bushwood sign.
- **Groundskeeper & Pest:** Caddyshack Gopher pops out of burrows; Greenskeeper Carl Spackler retaliates with water hoses and TNT plunger boxes.
- **Flora & Hazards:** Scots pines, flowering cherries, green azalea bushes, granite boulders, blue water ponds.

### 🏜️ La Playa
- **Atmosphere:** Sun-drenched high desert plateau.
- **Centerpiece:** Towering Burning Man wooden effigy with festival celebrants dancing around the flames.
- **Flora & Hazards:** Giant desert saguaro cacti, rocky rapids creeks, and rolling tumbleweeds that roll across the course and pile up against obstacles.
- **Characters:** Dancing raver with headphones trampling turf; police golf cart patrol giving chase.

### 🏰 Nocturnal Green
- **Atmosphere:** Gothic Transylvania midnight tournament in moody purples and dark greys.
- **Estate:** Transylvanian castle with gothic towers and wrought-iron cresting.
- **Flora & Hazards:** Autumn fall-leaf trees, sickly greenish-yellow shrubs, small piles of weathered coffins, dark midnight waters.
- **Supernatural Encounters:** An ancient sea monster tentacles out from the water hazard; zombies dig out of graves, chased down by Ash from Evil Dead with his boomstick and chainsaw.

---

## ⌨️ Complete Controls Reference

| Control | Action | Details |
| :--- | :--- | :--- |
| `Tab` | Switch Modes | Toggle between **Play Mode** and **Design Mode** |
| `Left Click` | Aim Target / Swing | Click map to set reticle; click ball/screen to swing |
| `Space` | Swing Execution | Universal swing trigger (1: Start, 2: Power, 3: Accuracy) |
| `R` | Replay Hole | Restarts current hole layout from the tee box |
| `C` | Cycle Clubs | Cycles forward through available clubs in your bag |
| `Arrow Keys` | Pan Camera | Scrolls the isometric viewing window across the course |
| `Right Click + Drag` | Pan Camera | Smooth mouse drag camera navigation |
| `1`, `2`, `3` | Design Tabs | Tab 1: Elevation, Tab 2: Surfaces, Tab 3: Scenery props |
| `B` | Brush Size | Toggles between 1×1 tile brush and 3×3 broad brush |
| `Esc` | Title / Back | Returns to splash screen or deselects current tool |

---

## 🎮 Game Boy Advance Controls
 
| GBA Button | Aiming State | Locked State (`LOCKED` Badge) |
| :--- | :--- | :--- |
| `D-Pad` | Move Camera & Center Crosshair across Course | Unlock Aim to re-aim Crosshair |
| `B` Button (Hold) | Turbo Camera Scroll Speed | Unlock Aim to re-aim Crosshair |
| `A` Button | **Click 1:** Drop Pin Flag at Crosshair, Lock Aim, return to Ball | **Click 2:** Execute Untimed Shot |
| `L` / `R` | Cycle Clubs Backward / Forward (Full 14-Club Bag) | Cycle Clubs Backward / Forward |
| `START` | Randomize Hole Layout & Cycle Environment Themes | Randomize Hole Layout & Cycle Themes |

---

## 📜 Credits & License
- **Original Concept & Direction:** Adam & Pair Programming AI
- **Engine:** Pure C99, custom software isometric rasterizer, fixed-point math library
- **Current Version:** `v0.77-noonan` (SemVer `v0.7.7`)

