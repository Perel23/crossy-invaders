#include "Game.h"
#include <iostream>
using namespace std;
using namespace bagel;

namespace ci
{
	static constexpr float TILE = 48.f;

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

		// Spawn the player entity at bottom-center.
		// addAll attaches all four components to a single new entity in one call.
		// The entity ID is an integer managed by bagel::World — we don't need to
		// store it because systems will find it via bitmask iteration.
		Entity::create().addAll(
			Transform{{WIN_W / 2.f, WIN_H - TILE}, 0.f},
			Drawable{{0, 0, 0, 0}, {TILE - 4, TILE - 4}},
			LanePos{0, 4},
			PlayerTag{});
	}

	Game::~Game()
	{
		if (ren) SDL_DestroyRenderer(ren);
		if (win) SDL_DestroyWindow(win);
		SDL_Quit();
	}

	void Game::input_system() const
	{
		// Step 3: keyboard input → player movement
	}

	void Game::draw_system() const
	{
		// Build the required component mask once (static = built only on first call).
		// Only entities that have BOTH Transform AND Drawable will be processed.
		static const Mask mask = MaskBuilder()
			.set<Transform>()
			.set<Drawable>()
			.build();

		for (Entity e = Entity::first(); !e.eof(); e.next()) {
			if (!e.test(mask)) continue;

			const auto& t = e.get<Transform>();
			const auto& d = e.get<Drawable>();

			// Center the rect on the entity's position (same convention as Pong)
			SDL_FRect dest = {
				t.p.x - d.size.x / 2,
				t.p.y - d.size.y / 2,
				d.size.x, d.size.y
			};

			// Placeholder: draw a solid green rectangle.
			// When we add a spritesheet this becomes SDL_RenderTextureRotated.
			SDL_SetRenderDrawColor(ren, 0, 200, 80, 255);
			SDL_RenderFillRect(ren, &dest);
		}

		SDL_SetRenderDrawColor(ren, 0, 0, 0, 255); // restore clear color
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
