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
	using InputState = struct { bool up, down, left, right, moved, shoot, shotFired, activate, activateFired, dash, slowmo, slowmoFired; };
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
	using GameStatus = struct { bool gameOver; bool won; bool sfxPlayed; int kills; int shots; int hits; Uint64 waveStartTime; Uint64 waveEndTime; };
	using SelectState  = struct { int selected; bool moved; int difficulty; int start_level; };  // difficulty: 0=Easy 1=Normal 2=Hard
	using LevelSplash  = struct { int framesLeft; };
	using MapScreen    = struct { int framesLeft; float planeT; int fromIdx; int toIdx; };
	using DamageFlash  = struct { int frames; };
	using ShieldFlash  = struct { int frames; };
	using ScreenShake  = struct { int frames; };
	using Explosion    = struct { int frames; int maxFrames; float startSize; };
	using Pickup       = struct { int type; };    // 0=rapid-fire  1=+shield  2=+health
	using RapidFire    = struct { int frames; int cooldown; };
	using ComboState      = struct { int count; int timer; int multiplier; };
	using IndividualMove  = struct { Uint64 nextMove; };
	using DashState    = struct { int cooldown; };
	using SpreadShot   = struct { int frames; };
	using FloatingText = struct { int frames; int maxFrames; int value; int mult; };
	using SlowMo       = struct { int frames; int cooldown; };
	using SpeedScale   = struct { float base; float active; };
	using SoundEvent   = struct { int type; };
	using HopState     = struct { int frames; int maxFrames; float hopDX; };
	using BreatheState = struct { float phase; float speed; float amplitude; };
	using BlinkPhase   = struct { bool visible; int counter; int period; };
	struct CharacterID    { int id; };            // which character was selected (0=Trump, etc.)
	struct WallTag        { int frames; };        // Trump's "Build the Wall" — frames remaining
	struct TrumpBulletTag {};                    // marks B2 bomber projectiles
	struct PutinBulletTag {};                    // marks nuclear missile projectiles
	struct BenGvirBulletTag {};                  // marks Torah scroll projectiles
	struct BibiBulletTag    {};                  // marks mahal.jpg image projectiles
	struct SaraBulletTag    {};                  // marks snowspray.png image projectiles
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
template <> struct bagel::Storage<ci::MapScreen> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::MapScreen>;
};
template <> struct bagel::Storage<ci::DamageFlash> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::DamageFlash>;
};
template <> struct bagel::Storage<ci::ShieldFlash> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::ShieldFlash>;
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
template <> struct bagel::Storage<ci::IndividualMove> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::IndividualMove>;
};
template <> struct bagel::Storage<ci::DashState> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::DashState>;
};
template <> struct bagel::Storage<ci::SpreadShot> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::SpreadShot>;
};
template <> struct bagel::Storage<ci::FloatingText> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::FloatingText>;
};
template <> struct bagel::Storage<ci::SlowMo> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::SlowMo>;
};
template <> struct bagel::Storage<ci::SpeedScale> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::SpeedScale>;
};
template <> struct bagel::Storage<ci::SoundEvent> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::SoundEvent>;
};
template <> struct bagel::Storage<ci::HopState> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::HopState>;
};
template <> struct bagel::Storage<ci::BreatheState> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::BreatheState>;
};
template <> struct bagel::Storage<ci::BlinkPhase> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::BlinkPhase>;
};
template <> struct bagel::Storage<ci::CharacterID> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::CharacterID>;
};
template <> struct bagel::Storage<ci::WallTag> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::WallTag>;
};
template <> struct bagel::Storage<ci::TrumpBulletTag> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::TrumpBulletTag>;
};
template <> struct bagel::Storage<ci::PutinBulletTag> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::PutinBulletTag>;
};
template <> struct bagel::Storage<ci::BenGvirBulletTag> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::BenGvirBulletTag>;
};
template <> struct bagel::Storage<ci::BibiBulletTag> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::BibiBulletTag>;
};
template <> struct bagel::Storage<ci::SaraBulletTag> final : bagel::NoInstance {
	using type = bagel::PackedStorage<ci::SaraBulletTag>;
};
