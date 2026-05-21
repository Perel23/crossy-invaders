# Crossy Invaders — Session Tracker

## Status: Step 10 complete — ready for Step 11

---

## Completed steps

### ✅ Step 1 — Project scaffold (2026-05-20)
- GitHub repo created: https://github.com/Perel23/crossy-invaders
- `bagel.h` copied from moshesu/bagel26 (single header, no full clone needed)
- `lib/SDL` and `lib/SDL_image` added as git submodules
- `CMakeLists.txt` set up (SDL3 + SDL3_image, no Box2D — discrete movement only)
- `Game.h` / `Game.cpp` skeleton: opens window at 60 FPS, exits on ESC
- `Components.h` placeholder created
- `main.cpp` written

---

### ✅ Step 2 — Components + player entity (2026-05-20)
- Bumped `MaxComponents` to 16 in `bagel.h`
- Defined `Transform`, `Drawable`, `LanePos`, `PlayerTag` in `Components.h`
- `PackedStorage` for `LanePos` and `PlayerTag`
- Player entity spawned at bottom-center
- `draw_system` iterates `Transform`+`Drawable` mask, draws colored rectangle placeholder

---

### ✅ Step 3 — Input + player movement (2026-05-21)
- `InputState` component: up/down/left/right/shoot/activate + debounce flags
- `input_system`: reads SDL keyboard state → writes `InputState`
- `player_move_system`: one tile hop per keypress, clamped to grid bounds

---

### ✅ Step 4 — Enemy formation (2026-05-21)
- `EnemyTag` + `Health` components
- Enemies spawned in a grid (2 rows × 8 cols) near the top
- `enemy_move_system`: Space Invaders side-step + drop-a-lane on edge hit (600 ms timer)

---

### ✅ Step 5 — Shooting (2026-05-21)
- `BulletTag` + `Velocity` components
- `shoot_system`: player fires up on Space; lowest-lane enemy fires down on 1200 ms timer
- `bullet_system`: moves bullets by velocity each frame, despawns off-screen

---

### ✅ Step 6 — Collision + end states (2026-05-21)
- `collision_system`: bullet×enemy, bullet×player (shield-aware), enemy×player
- Post-hit invincibility frames (90 frames) prevent rapid damage stacking
- GAME OVER / YOU WIN overlay using `SDL_RenderDebugText` at 4× scale
- R key restarts the game

---

### ✅ Step 7 — Lane hazards + HUD (2026-05-21)
- `Hazard` component; 2 hazard lanes (lanes 4 and 6) with 2 cars each, screen-wrap
- `hazard_move_system`: continuous pixel movement
- HUD: 3 HP icons + 2 shield-charge icons in top-left corner

---

### ✅ Step 8 — Iron Dome shield (2026-05-21)
- `Shield` component: timer (frames) + charges remaining
- `shield_system`: I key activates, 5-second timer, 2 charges per life
- Blue halo drawn around player while shield is active
- Shield absorbs enemy bullets with no HP loss

---

### ✅ Step 9 — Shelters (2026-05-21)
- `Shelter` component; 3 shelters on lane 3, HP = 5 each
- Collision pass checks bullet×shelter before bullet×player — shelters actually provide cover
- Shelter color shades from light grey (full HP) to dark grey (nearly destroyed)
- Layout overhauled: 1024×768, 64px tiles, spacious lane spacing

---

### ✅ Step 10 — Character select screen (2026-05-21)
- `GameState` enum: `Select` → `Playing`
- Select screen: two colored boxes (Trump = orange, Bibi = blue), arrow keys to choose, ENTER to confirm
- Player's in-game color matches chosen character
- R after game over returns to character select

---

## Next step to do → Step 11: Level Progression

**Goal**: Multiple levels with increasing difficulty. Beating a wave starts the next level rather than ending the game.

**What to do**:
1. Add `_level` counter (starts at 1) and a brief "LEVEL X" splash between waves
2. On win condition (no enemies left), increment `_level`, re-spawn the enemy formation — don't go back to select
3. Scale difficulty per level:
   - `ENEMY_MOVE_MS` decreases (enemies step faster)
   - `ENEMY_SHOOT_MS` decreases (enemies shoot more often)
   - Optionally add a third enemy row at higher levels
4. Add procedural/random variety to hazard lane positions and car counts so each run feels different (deferred from layout step)

**Why**: This turns a one-shot demo into a proper arcade loop. Difficulty scaling is the core of the Space Invaders feel — the creeping dread as the formation speeds up.

**Definition of done**: Player can complete wave 1 and be presented with a harder wave 2; game only ends on player death, not wave clear.

---

## Upcoming steps

| # | Step | Summary |
|---|------|---------|
| 11 | Level progression | Multiple levels, difficulty scaling, random hazard layout |
| 12 | Polish | Sprites, sounds, score display, proper game over/win screen |
