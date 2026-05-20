#pragma once
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include "bagel.h"
#include "Components.h"

namespace ci
{
	class Game
	{
	public:
		Game();
		~Game();

		void run();
		bool valid() const { return ren != nullptr; }

	private:
		static constexpr int	WIN_W = 800;
		static constexpr int	WIN_H = 600;
		static constexpr int	FPS = 60;
		static constexpr Uint64	GAME_FRAME = 1000 / FPS;

		void input_system() const;
		void draw_system() const;

		SDL_Window*		win = nullptr;
		SDL_Renderer*	ren = nullptr;
	};
}
