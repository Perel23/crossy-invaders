#pragma once
#include <SDL3/SDL.h>
#include "bagel.h"
namespace ci
{
	using Transform = struct { SDL_FPoint p; float a; };
	using Drawable  = struct { SDL_FRect part; SDL_FPoint size; };
	using LanePos   = struct { int lane; int col; };
	struct PlayerTag {};
	struct HazardVisual {
		int tex_index;
	};
	using InputState = struct { bool up, down, left, right, moved, shoot, shotFired, activate, activateFired; };
	struct EnemyTag {};
	struct BossTag {};   // marks the level-3 boss; also carries EnemyTag
	using BulletTag = struct { bool fromPlayer; };
	using Velocity = struct { float dx, dy; };
	using Health = struct { int hp; };
	struct Hazard {};
	using Shield = struct { float timer; int charges; };
	struct Shelter {};
	using Invincibility = struct { int frames; };
	using FormationState = struct { int dir; Uint64 moveTimer; Uint64 shootTimer; };
	using GameStatus = struct { bool gameOver; bool won; bool sfxPlayed; };
	using SelectState  = struct { int selected; bool moved; };
	using LevelSplash  = struct { int framesLeft; };
	using DamageFlash  = struct { int frames; };
	using ScreenShake  = struct { int frames; };
	using Explosion    = struct { int frames; int maxFrames; float startSize; };
	using Pickup       = struct { int type; };    // 0=rapid-fire  1=+shield  2=+health
	using RapidFire    = struct { int frames; int cooldown; };
	using ComboState   = struct { int count; int timer; int multiplier; };
}
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
template <> struct bagel::Storage<ci::BossTag> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::BossTag>;
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
template <> struct bagel::Storage<ci::LevelSplash> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::LevelSplash>;
};
template <> struct bagel::Storage<ci::DamageFlash> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::DamageFlash>;
};
template <> struct bagel::Storage<ci::ScreenShake> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::ScreenShake>;
};
template <> struct bagel::Storage<ci::Explosion> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::Explosion>;
};
template <> struct bagel::Storage<ci::Pickup> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::Pickup>;
};
template <> struct bagel::Storage<ci::RapidFire> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::RapidFire>;
};
template <> struct bagel::Storage<ci::ComboState> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::ComboState>;
};
