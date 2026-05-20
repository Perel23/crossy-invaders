# Crossy Invaders — Session Tracker

## Status: NOT STARTED — setup pending

---

## Completed steps
_(none yet)_

---

## Next step to do → Step 1: Project scaffold

**Goal**: Get a window open with the bagel26 engine — nothing more.

**What to do**:
1. Create GitHub repo `crossy-invaders`
2. Copy `bagel.h` from moshesu/bagel26
3. Add `lib/SDL` and `lib/SDL_image` as git submodules (same URLs as bagel26)
4. Write `CMakeLists.txt` (SDL3 + SDL3_image only — no Box2D needed)
5. Write `main.cpp` that opens a black 800×600 window and exits on ESC
6. Write minimal `Game.h` / `Game.cpp` skeleton with empty systems

**Why**: Before writing any ECS code, we need to confirm the build works end-to-end on all team members' machines.

**Definition of done**: `cmake .. && make && ./crossy-invaders` opens a window.

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
