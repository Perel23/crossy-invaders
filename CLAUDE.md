# Crossy Invaders — CLAUDE.md

## What this project is
A 2D arcade game combining **Crossy Road** (lane-dodging movement) and **Space Invaders** (enemy wave combat).
Built as an ECS course assignment using the **bagel26** engine.

## Language & Engine
- **Language**: C++ (C++20), using the [bagel26](https://github.com/moshesu/bagel26) ECS engine
- **Renderer**: SDL3 + SDL3_image (no Box2D — movement is discrete, not physics-based)
- **Build**: CMake

## ECS Architecture (bagel26 patterns)
- Components are plain structs: `using Transform = struct { SDL_FPoint p; float a; };`
- Override storage via template specialization: `template <> struct bagel::Storage<MyComp> final : NoInstance { using type = PackedStorage<MyComp>; };`
- Systems are `const` methods in the game class — build a `MaskBuilder` mask, iterate with `Entity::first()` / `e.eof()` / `e.next()`, filter with `e.test(mask)`
- Entities created in constructor with `Entity::create().addAll(...)`
- Namespace: `bagel` for engine, `ci` (crossy invaders) for our code

## ECS Rules (from lecturer feedback)

**No global or specific member variables** — variables like `leftHeld`, `gravityTimer`, `invincFrames` must NOT be class members. In ECS everything lives in components and is processed by systems.

**No stored entity handles** — do not keep `activeEnt` or similar handles as class members. Systems must iterate all entities with `MaskBuilder` + `Entity::first()` / `e.next()` / `e.test(mask)` and filter by component mask. At construction ensure only one entity per unique role exists.

**Timers are components, not members** — any timer (movement cooldown, invincibility, shield duration, shoot interval) must be a component on the relevant entity with a system that decrements it. The ideal pattern:
1. Timer component counts down each frame
2. When it reaches 0, the system adds a tag to the entity
3. A separate system handles that tag (single responsibility)

**Events/flags are components or tags** — game-over, win, and similar state flags belong as components (`GameStatus`) or tags (`GameOverTag`) on entities, not as class-level booleans.

## Visual Style

### Camera / Perspective
- **Diagonal top-down view** — like the real Crossy Road: the camera looks down at roughly 45° from above and slightly in front, giving a pseudo-3D feel without true 3D
- Tiles and sprites are drawn as if seen from this angle: slightly squashed vertically, with a visible "top face" and a short "front face" on raised objects (cars, shelters, enemies)
- Depth is faked with draw order: entities with a higher lane index (farther from the player) are drawn first so nearer entities overlap them
- No actual 3D math — all positions remain on the 2D grid; the isometric look comes entirely from the sprite artwork and draw order

### Camera follows the player
- The player is always rendered at the **bottom-center of the screen** — the camera scrolls to keep them there
- All other entities (enemies, hazards, bullets, shelters) are rendered at `world_pos - camera_offset`
- `Transform.p` stores **world-space position** for all entities; the camera offset is subtracted only at render time in `draw_system`
- The camera tracks the player's world Y: `cameraY = player.Transform.p.y - (WIN_H - TILE * 1.5f)` — so the player sits near the bottom row on screen regardless of how far they've advanced
- This gives the Crossy Road feel of always moving "forward" into danger — the screen never feels static

### Implementation note
When we add the camera, `draw_system` will calculate `screenPos = worldPos - {0, cameraY}` before drawing each entity. No component changes needed — `Transform.p` already holds world positions consistently. The current placeholder code uses screen == world (camera fixed at origin), which will be replaced when sprites are added.

### Art direction
- Sprites should be pre-rendered or drawn to match the diagonal perspective (not flat top-down)
- When we add a spritesheet, tiles are roughly 2:1 width-to-height ratio (standard isometric tile shape)

## Game Design

### Characters (player choice at start)
- Trump or Bibi — visual difference only at this stage

### Gameplay loop
1. Player moves between **lanes** using arrow keys (discrete tile steps — Crossy Road style)
2. **Enemy wave** (Revolutionary Guards fighters) occupies the top of the screen, moves Space Invaders style, shoots downward
3. Player can shoot upward to eliminate enemies
4. Enemies advance over time; if they reach the player's zone → game over

### Special mechanics
- **Iron Dome** power-up: temporary shield blocking incoming missiles (timer-based)
- **Shelters** (מקלטים): static entities player can hide behind for cover
- **Lane hazards**: moving obstacles on road lanes (like Crossy Road cars)

### Level structure
- Fixed number of levels (not infinite)
- Each level: faster enemies, more accurate, more aggressive
- Player receives upgrades between levels

## Component Plan
| Component | Storage | Purpose |
|---|---|---|
| `Transform` | Sparse | Position + angle on screen |
| `Drawable` | Sparse | Sprite rect + size |
| `Velocity` | Packed | For enemies, bullets |
| `PlayerTag` | Packed | Marks the player entity |
| `EnemyTag` | Packed | Marks enemy entities |
| `BulletTag` | Packed | Marks bullet entities |
| `Health` | Packed | HP for player/enemies/shelters |
| `Shield` | Packed | Iron Dome — timer remaining |
| `LanePos` | Packed | Discrete lane + column position |
| `InputState` | Sparse | Keyboard state for player |
| `Hazard` | Packed | Moving lane obstacle (cars etc.) |

## Systems Plan
1. `input_system` — read keyboard → update `InputState`
2. `player_move_system` — `InputState` → update `LanePos` + `Transform`
3. `enemy_move_system` — Space Invaders formation movement
4. `bullet_system` — move bullets, despawn off-screen
5. `shoot_system` — enemy shoot timers + player shoot
6. `collision_system` — bullet×player, bullet×enemy, player×hazard, bullet×shelter
7. `shield_system` — decrement Iron Dome timer, remove when expired
8. `draw_system` — render all `Transform`+`Drawable` entities

## File Structure (to be created)
```
crossy-invaders/
├── CMakeLists.txt
├── bagel.h             (copied from bagel26)
├── main.cpp
├── Game.h / Game.cpp   (game class, run loop)
├── Components.h        (all component definitions)
├── res/                (sprites/assets)
└── lib/
    ├── SDL/            (git submodule)
    └── SDL_image/      (git submodule)
```

## Working style
- Build incrementally, one step at a time
- Each step must compile and produce visible progress
- Follow bagel26 Pong coding style exactly
- `next_step.md` is updated at the end of every session
- **Teach every step**: before writing code, explain what we're doing and why — cover the ECS concept involved, the design decision, and what the code will do. The team is learning while building.
