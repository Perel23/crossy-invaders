#pragma once
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include "bagel.h"
#include "Components.h"
#include <vector>

namespace ci
{
	enum class GameState { Select, Playing, LevelTransition, Paused };

	class Game
	{
	public:
		Game();
		~Game();

		void run();
		bool valid() const { return ren != nullptr; }

		static constexpr int	WIN_W = 1024;
		static constexpr int	WIN_H = 768;
		static constexpr int	TILE  = 64;
		static constexpr int	COLS  = WIN_W / TILE;   // 16 columns
		static constexpr int	LANES = WIN_H / TILE;   // 12 lanes

	private:
		static constexpr int	FPS              = 60;
		static constexpr Uint64	GAME_FRAME       = 1000 / FPS;
		static constexpr Uint64	ENEMY_MOVE_MS    = 600;    // ms between formation steps
		static constexpr Uint64	ENEMY_SHOOT_MS   = 1200;   // ms between enemy shots
		static constexpr float	BULLET_SPEED     = 8.f;    // pixels per frame (~480 px/s at 60 fps)
		static constexpr float	SHIELD_DURATION  = 300.f;  // frames (~5 s at 60 fps)
		static constexpr int	SHIELD_CHARGES   = 2;      // uses per life

		void spawn_entities() const;
		void reset() const;
		void clear_game_entities() const;

		void select_input() const;
		void select_draw() const;

		void input_system() const;
		void player_move_system() const;
		void enemy_move_system() const;
		void shoot_system() const;
		void bullet_system() const;
		void hazard_move_system() const;
		void shield_system() const;
		void collision_system() const;
		void explosion_system() const;
		void pickup_system() const;
		void combo_system() const;
		void splash_system() const;
		void play_sfx(int type) const;
		void draw_background() const;
		void draw_system() const;
		void draw_level_splash() const;
		void draw_pause() const;
		void endgame_draw() const;

		SDL_Window*		        win            = nullptr;
		SDL_Renderer*	        ren            = nullptr;
		mutable SDL_Texture*	player_texture   = nullptr;
		mutable SDL_Texture*	enemy_texture    = nullptr;
		mutable SDL_Texture*	boss_texture     = nullptr;
		mutable SDL_Texture*	shelter_texture  = nullptr;
		mutable SDL_Texture*	hearts_texture   = nullptr;
		mutable int _current_level    = 1;
		mutable int _score            = 0;
		mutable int _high_score       = 0;
		mutable int _wave_enemy_count = 0;
		mutable SDL_Texture* haz_textures[4]   = {nullptr, nullptr, nullptr, nullptr};
		mutable SDL_Texture* trump_select_tex  = nullptr;
		mutable SDL_Texture* bibi_select_tex   = nullptr;
		mutable SDL_AudioStream* audio_stream  = nullptr;

		mutable GameState _state = GameState::Select;
	};
}
