# Crossy Invaders — Session Tracker

## Status: Step 2 complete — ready for Step 3

---

## Completed steps

### ✅ Step 1 — Project scaffold (2026-05-20)
- GitHub repo created: https://github.com/Perel23/crossy-invaders
- `bagel.h` copied from moshesu/bagel26 (single header, no full clone needed)
- `lib/SDL` and `lib/SDL_image` added as git submodules
- `CMakeLists.txt` set up (SDL3 + SDL3_image, no Box2D — discrete movement only)
- `Game.h` / `Game.cpp` skeleton: opens 800×600 black window at 60 FPS, exits on ESC
- `Components.h` placeholder created
- `main.cpp` written

**To build on your machine (first time):**
```bash
git clone --recurse-submodules https://github.com/Perel23/crossy-invaders
cd crossy-invaders
mkdir build && cd build
cmake ..
make
./crossy_invaders
```

---

### ✅ Step 2 — Components + player entity (2026-05-20)
- Bumped `MaxComponents` to 16 in `bagel.h` (was 6)
- Defined `Transform`, `Drawable`, `LanePos`, `PlayerTag` in `Components.h`
- `PackedStorage` for `LanePos` and `PlayerTag`
- Player entity spawned at bottom-center with all four components
- `draw_system` iterates `Transform`+`Drawable` mask, draws green rectangle placeholder

**Definition of done**: ✅ Green rectangle appears at bottom-center of window

---

## Next step to do → Step 3: Input + player movement

**Goal**: Arrow keys move the player between discrete lanes. One keypress = one tile hop (Crossy Road style).

**What to do**:
1. Add `InputState` component: booleans for up/down/left/right, plus a `moved` flag to prevent key-hold sliding
2. Implement `input_system`: poll SDL keyboard state → write into `InputState`
3. Implement `player_move_system`: read `InputState`, update `LanePos` (clamped to grid bounds), recalculate `Transform.p` from lane+col
4. Define the grid constants: `TILE = 48`, `COLS = 800/48`, `LANES = 600/48`

**Why**: This introduces a second inter-system data flow — `input_system` writes data that `player_move_system` reads. That's the ECS way to pass information between systems without coupling them.

**Definition of done**: The green rectangle hops one tile per arrow key press, stops at screen edges.

---

## Upcoming steps (in order)

| # | Step | Summary |
|---|------|---------|
| 2 | Components + player entity | Define all components in `Components.h`, spawn player at bottom-center |
| 3 | Input + player movement | Arrow keys move player between discrete lanes |
| 4 | Enemy formation | Spawn enemy grid at top, Space Invaders side-to-side movement |
| 5 | Shooting | Enemies fire bullets down; player fires bullets up |
| 6 | Collision | Bullet×player, bullet×enemy, player×enemy |
| 7 | Lane hazards | Moving cars/obstacles on road lanes |
| 8 | Iron Dome power-up | Timed shield blocks incoming bullets |
| 9 | Shelters | Static cover entities player can hide behind |
| 10 | Character select screen | Trump / Bibi choice before game starts |
| 11 | Level progression | Multiple levels, difficulty scaling, upgrades |
| 12 | Polish | Sprites, sounds, score display, game over/win screen |

---

## Notes / decisions
- Language: C++ (C++20) with bagel26 ECS engine
- No Box2D — movement is discrete (tile/lane-based), no physics needed
- Window size: 800×600 (same as Pong example)
- Namespace: `ci` for our game code, `bagel` for engine
