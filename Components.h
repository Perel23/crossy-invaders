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
