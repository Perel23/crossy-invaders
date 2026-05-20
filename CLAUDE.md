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
