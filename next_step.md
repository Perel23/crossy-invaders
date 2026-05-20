# Crossy Invaders — Session Tracker

## Status: Step 1 complete — ready for Step 2

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

## Next step to do → Step 2: Components + player entity

**Goal**: Define all components in `Components.h` and spawn a visible player entity at the bottom of the screen.

**What to do**:
1. Add a spritesheet to `res/` (or use colored rectangles as placeholder)
2. Define components: `Transform`, `Drawable`, `LanePos`, `PlayerTag`
3. Override storage for frequently-iterated components (`PackedStorage`)
4. In `Game` constructor: spawn one player entity with all four components
5. Implement `draw_system()` — iterate entities with `Transform`+`Drawable`, render them

**Why**: This is the first real ECS step — you'll see how components attach to entities and how systems filter them with a mask.

**Definition of done**: A player sprite (or colored rectangle) appears at the bottom-center of the window.

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
