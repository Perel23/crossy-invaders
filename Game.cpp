#include "Game.h"
#include <iostream>
using namespace std;
using namespace bagel;

namespace ci
{
	Game::Game()
	{
		if (!SDL_Init(SDL_INIT_VIDEO)) {
			cout << SDL_GetError() << endl;
			return;
		}
		if (!SDL_CreateWindowAndRenderer("Crossy Invaders", WIN_W, WIN_H, 0, &win, &ren)) {
			cout << SDL_GetError() << endl;
			return;
		}
		SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
	}

	Game::~Game()
	{
		if (ren) SDL_DestroyRenderer(ren);
		if (win) SDL_DestroyWindow(win);
		SDL_Quit();
	}

	void Game::input_system()
	{
		// Step 3: keyboard input → player movement
	}

	void Game::draw_system()
	{
		// Step 2: render all Transform+Drawable entities
	}

	void Game::run()
	{
		auto start = SDL_GetTicks();
		bool quit = false;

		while (!quit) {
			input_system();

			SDL_RenderClear(ren);
			draw_system();
			SDL_RenderPresent(ren);

			const auto end = SDL_GetTicks();
			if (end - start < GAME_FRAME)
				SDL_Delay(GAME_FRAME - (end - start));
			start += GAME_FRAME;

			SDL_Event e;
			while (SDL_PollEvent(&e)) {
				if (e.type == SDL_EVENT_QUIT ||
					(e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_ESCAPE))
					quit = true;
			}
		}
	}
}
