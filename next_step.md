# Crossy Invaders — Session Tracker

## Status: Step 13 complete — all 16 polish improvements implemented

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

### ✅ Step 11 — Polish & Gameplay Improvements (2026-05-26)
- **Lane background**: each lane now has a distinct colour (dark green safe zone, asphalt road lanes with dashed centre-lines, dark-red enemy zone, divider lines between every lane)
- **Level splash**: `GameState::LevelTransition` state; after clearing a wave a 150-frame "LEVEL X / Get ready…" overlay is shown before the next wave spawns — uses a `LevelSplash` ECS component with a `splash_system` that calls `spawn_entities()` when it expires
- **Score**: `_score` increments by `10 × level` per enemy kill; displayed as `SCORE 00000` centred at the top of the HUD each frame
- **Level number**: shown as `LVL X` in the HUD bottom-left area
- **Multiple enemy shooters**: instead of always shooting from one fixed enemy, the system now finds all enemies on the front row and picks one at random each volley — shots come from varied horizontal positions
- **Boss spread shot**: on level 3 the boss fires three bullets simultaneously at dx = −2.5, 0, +2.5 pixels/frame
- **Boss health bar**: a full-width red bar below the HUD icons shows remaining boss HP during level 3
- **Random hazard positions**: car starting X coordinates are randomised via `std::rand()` each time `spawn_entities()` runs; base speed scales with level (`+0.6 px/frame per level`)

---

## Next step to do → (game is complete — see upcoming steps for optional extras)

---

### ✅ Step 13 — 16-improvement mega-update (2026-05-27)

1. **Dodge/dash** — SHIFT+direction: 2-tile hop, 60-frame cooldown, 12-frame invincibility; `DashState` component on player; cyan refill bar above sprite
2. **Bullet-time (SlowMo)** — Q key: 3 s slow mode (enemy bullets at 35% speed, formation 3× slower), 10 s cooldown; blue overlay tint; bar + "SLOW" label top-right; `SlowMo` component on GameStatus entity; sfx case 6 (whoosh)
3. **Enemy row variety** — front row has 3× weight in random shooter selection; back rows tinted progressively cooler/bluer via `SDL_SetTextureColorMod`
4. **Spread-shot pickup** — purple type-3 pickup; `SpreadShot` component on player; 300-frame duration; purple timer bar under player; fires 3-bullet cone
5. **Pickup labels** — single letter (R/S/H/W) drawn over every pickup square
6. **Power-up timer bars** — RapidFire (yellow) and SpreadShot (purple) thin bars beneath player sprite; dash cooldown refill bar (cyan) above sprite
7. **Off-screen enemy warning arrows** — upward yellow triangles drawn at top edge via `SDL_RenderGeometry` for any enemy with screenY < 0
8. **End-of-level stats** — Kills / Accuracy (%) / Time (s) displayed on the level-transition splash; `waveStartTime`+`waveEndTime` in `GameStatus`
9. **Persistent high score** — `highscore.dat` written on every new best; `load_high_score()` in constructor; `save_high_score()` called on beat
10. **Player low-HP tinting** — `SDL_SetTextureColorMod(player_texture, 255, 80, 80)` when HP == 1
11. **Per-row enemy tinting** — depth computed from `LanePos.lane - minEnemyLane`; each step back reduces R/G, keeps B=255
12. **Floating score pop-ups** — `FloatingText` component; entities float upward 1.2 px/frame for 50 frames; `+N×M` text drawn in gold
13. **Animated select background** — parallax star-field (two layers, different scroll rates) driven by `_select_scroll` member
14. **Footstep dust** — small `Explosion{8,8,10}` entity spawned at old player position on every hop (including dash)
15. **Difficulty selection** — Easy/Normal/Hard on select screen (UP/DN keys); `SelectState.difficulty`; speed multipliers applied in enemy_move_system + shoot_system
16. **Level 4 role reversal** — player spawns at lane 9 (top); 2×8 enemies at lanes 2–3; enemies advance **upward** on edge hit; player bullets go down, enemy bullets go up; game over if enemy reaches lane ≥ 8; deep-space background; camera scroll disabled

---

## Previous next step (now complete)

**Goal**: Add audio feedback and tighten the overall feel.

**What to do**:
1. Add SDL_mixer (or SDL3 audio) as a submodule; load and play sound effects for: shoot, enemy hit, player hit, level clear, game over
2. Add a high-score display on the game-over / win screen (persist to a file or keep session-best in memory)
3. Animate the character select screen (show actual player sprites in the selection boxes)
4. Add a brief screen-flash effect when the player takes damage (red tint overlay for ~10 frames)

**Why**: Sound and tactile feedback are the last major gap between "functional prototype" and "feels like a real game".

**Definition of done**: Every major game event has a sound; the select screen shows sprites; damage flash is visible.

---

## Upcoming steps

| # | Step | Summary |
|---|------|---------|
| 12 | Sound & final polish | SDL_mixer SFX, high-score, damage flash, sprite select screen |
