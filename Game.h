#pragma once
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include "bagel.h"
#include "Components.h"
#include <vector>

namespace ci
{
	enum class GameState { Select, Playing, LevelTransition, Paused, MapScreen, Benchmark };

	class Game
	{
	public:
		Game();
		~Game();

		void run();
		bool valid() const { return ren != nullptr; }

		static constexpr int	WIN_W = 1024;
		static constexpr int	WIN_H = 704;
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
		void spawn_enemy_wave() const;
		void reset() const;
		void clear_game_entities() const;
		void clear_enemies_only() const;

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
		void floating_text_system() const;
		void sound_system() const;
		void hop_system() const;
		void animate_system() const;
		void blink_system() const;
		void map_screen_system() const;
		void draw_map_screen() const;
		void spawn_benchmark() const;
		void bench_system() const;
		void draw_benchmark_hud() const;
		void play_sfx(int type) const;
		void load_high_score();
		void save_high_score() const;
		void switch_character() const;
		void draw_background() const;
		void draw_system() const;
		void draw_level_splash() const;
		void draw_pause() const;
		void endgame_draw() const;

		SDL_Window*		        win            = nullptr;
		SDL_Renderer*	        ren            = nullptr;
		mutable SDL_Texture*	bg_texture       = nullptr;
		mutable SDL_Texture*	player_texture   = nullptr;
		mutable SDL_Texture*	enemy_texture    = nullptr;
		mutable SDL_Texture*	boss_texture     = nullptr;
		mutable SDL_Texture*	shelter_texture    = nullptr;
		mutable SDL_Texture*	hearts_texture     = nullptr;
		mutable SDL_Texture*	iron_dome_texture      = nullptr;
		mutable SDL_Texture*	iron_dome_icon_texture = nullptr;
		mutable SDL_Texture*	bibi_bullet_tex        = nullptr;
		mutable SDL_Texture*	sara_bullet_tex        = nullptr;
		mutable int _current_level    = 1;
		mutable int _score            = 0;
		mutable int _high_score       = 0;
		mutable int _total_kills      = 0;   // accumulated across all levels
		mutable int _wave_enemy_count = 0;
		static constexpr int NUM_CHARS = 12;
		mutable SDL_Texture* haz_textures[4]      = {nullptr, nullptr, nullptr, nullptr};
		mutable SDL_Texture* char_select_tex[NUM_CHARS] = {};
		mutable SDL_AudioStream* audio_stream  = nullptr;

		mutable float        _camera_scroll       = 0.f;  // px added to every rendered Y (grows → entities scroll down)
		mutable float        _level_start_scroll  = 0.f;  // value of _camera_scroll when this level began
		mutable int          _camera_grace        = 180;  // frames before camera starts moving
		mutable float        _select_scroll       = 0.f;  // animated select-screen background offset
		mutable int          _select_last_char   = -1;   // last character whose anthem played (-1=none)
		mutable bool         _show_perf_hud      = true; // toggle with H key
		mutable bool         _muted              = false;
		mutable Uint64       _bench_timer        = 0;   // benchmark: last enemy-respawn check time
		mutable int          _bench_frame        = 0;   // benchmark: frame counter for auto-shoot
		mutable int          _bench_max_speedup  = 0;   // benchmark: peak speedup ratio ever recorded
		mutable int          _difficulty     = 1;     // 0=Easy 1=Normal 2=Hard (cached at game start)
		// Previous-wave stats saved before spawn_enemy_wave() resets them, used by draw_level_splash()
		mutable int          _prev_kills     = 0;
		mutable int          _prev_shots     = 0;
		mutable int          _prev_hits      = 0;
		mutable int          _prev_wave_secs = 0;

		mutable GameState _state = GameState::Select;
	};
}
