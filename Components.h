#pragma once
#include <SDL3/SDL.h>
#include "bagel.h"

namespace ci
{
	// World-space position (x,y) and rotation angle in degrees.
	// Every visible entity has this.
	using Transform = struct { SDL_FPoint p; float a; };

	// Which part of the spritesheet to sample (part) and how large to draw it (size).
	// part is left at {0,0,0,0} until we load a real spritesheet in a later step.
	using Drawable  = struct { SDL_FRect part; SDL_FPoint size; };

	// Discrete grid position: lane = row index from bottom, col = column index from left.
	// This drives the Crossy Road-style tile movement.
	using LanePos   = struct { int lane; int col; };

	// Tag component — marks the player entity.
	// Tags carry no data; their presence in the entity's bitmask is what matters.
	struct PlayerTag {};

	// Keyboard state for the player.
	// moved/shotFired/activateFired prevent key-hold from repeating actions every frame.
	using InputState = struct { bool up, down, left, right, moved, shoot, shotFired, activate, activateFired; };

	// Tag component — marks enemy entities.
	struct EnemyTag {};

	// Tag component — marks bullet entities.
	// fromPlayer = true → fired by player (travels up), false → fired by enemy (travels down).
	using BulletTag = struct { bool fromPlayer; };

	// Continuous pixel velocity (pixels per frame at 60 fps).
	// Used for bullets; enemies move discretely via enemy_move_system.
	using Velocity = struct { float dx, dy; };

	// Hit points. Enemies have 1 (one-shot), player has 3, shelters have more.
	using Health = struct { int hp; };

	// Tag component — marks lane-hazard entities (moving cars/obstacles).
	struct Hazard {};

	// Iron Dome shield. timer > 0 means the shield is active (counts down in frames).
	// charges = how many activations remain this life.
	using Shield = struct { float timer; int charges; };

	// Tag component — marks shelter (bunker) entities.
	struct Shelter {};

	// Post-hit invincibility for the player (frames remaining).
	// Lives on the player entity; collision_system reads and writes it.
	using Invincibility = struct { int frames; };

	// Whole-formation movement and shoot timers plus direction.
	// Lives on a dedicated formation entity spawned alongside enemies.
	using FormationState = struct { int dir; Uint64 moveTimer; Uint64 shootTimer; };

	// End-of-game result flags — set by collision_system, read by run() and endgame_draw().
	// Lives on a game-status entity spawned with the rest of the gameplay entities.
	using GameStatus = struct { bool gameOver; bool won; };

	// Character-select screen state: which character is highlighted and key-hold debounce.
	// Lives on a persistent entity created at startup (no Transform, survives reset).
	using SelectState = struct { int selected; bool moved; };
}

// --- Storage specializations ---
// SparseStorage (default): array indexed directly by entity ID — fast for random access.
// PackedStorage: tightly packed array — better cache performance when iterating many entities.
// We use PackedStorage for components that systems will iterate over every frame.

template <> struct bagel::Storage<ci::LanePos> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::LanePos>;
};
template <> struct bagel::Storage<ci::PlayerTag> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::PlayerTag>;
};
template <> struct bagel::Storage<ci::InputState> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::InputState>;
};
template <> struct bagel::Storage<ci::EnemyTag> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::EnemyTag>;
};
template <> struct bagel::Storage<ci::BulletTag> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::BulletTag>;
};
template <> struct bagel::Storage<ci::Velocity> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::Velocity>;
};
template <> struct bagel::Storage<ci::Health> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::Health>;
};
template <> struct bagel::Storage<ci::Hazard> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::Hazard>;
};
template <> struct bagel::Storage<ci::Shield> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::Shield>;
};
template <> struct bagel::Storage<ci::Invincibility> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::Invincibility>;
};
template <> struct bagel::Storage<ci::Shelter> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::Shelter>;
};
