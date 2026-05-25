#include "Game.h"
#include <algorithm>
#include <cmath>
#include <iostream>
using namespace std;
using namespace bagel;

namespace ci
{
	static constexpr int ENEMY_ROWS      = 2;
	static constexpr int ENEMY_COLS      = 8;
	static constexpr int ENEMY_START_COL = (Game::COLS - ENEMY_COLS) / 2;  // col 4 — centered
	static constexpr int ENEMY_TOP_LANE  = Game::LANES - 2;                // lane 10

	// -----------------------------------------------------------------------
	// Lifecycle
	// -----------------------------------------------------------------------

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

		// Persistent UI-state entity: survives reset() because it has no Transform.
		// Holds character-select state so select_input/select_draw don't need class members.
		Entity::create().add(SelectState{0, false});
	}

	Game::~Game()
	{
		if (player_texture) SDL_DestroyTexture(player_texture);
		if (ren) SDL_DestroyRenderer(ren);
		if (win) SDL_DestroyWindow(win);
		SDL_Quit();
	}

	// Two hazard lanes with generous gaps between cars.
	// Lanes 4 and 6 leave lane 5 as a clear safe strip between them.
	struct HazardDef { int lane; float xs[2]; float dx; };
	static constexpr HazardDef HAZARD_DEFS[] = {
		{ 4, { 200.f, 720.f },  2.5f },   // lane 4: 2 cars going right
		{ 6, { 100.f, 620.f }, -2.0f },   // lane 6: 2 cars going left
	};
	static constexpr float HAZARD_W = Game::TILE * 2.f - 4.f;   // 124 px wide
	static constexpr float HAZARD_H = Game::TILE       - 4.f;

	void Game::spawn_entities() const
	{
		// Read selected character from the persistent SelectState entity.
		static const Mask ssMask = MaskBuilder().set<SelectState>().build();
		int selected = 0;
		for (Entity e = Entity::first(); !e.eof(); e.next())
			if (e.test(ssMask)) { selected = e.get<SelectState>().selected; break; }

		// Load the matching character sprite; fall back gracefully if the file is missing.
		if (player_texture) { SDL_DestroyTexture(player_texture); player_texture = nullptr; }
		const char* path = (selected == 0) ? "res/trump_pixel.png" : "res/bibi_pixel.png";
		SDL_Surface* surf = IMG_Load(path);
		if (surf) {
			player_texture = SDL_CreateTextureFromSurface(ren, surf);
			SDL_DestroySurface(surf);
		}

		const int startCol  = COLS / 2;
		const int startLane = 0;
		Entity::create().addAll(
			Transform{{TILE / 2.f + startCol * TILE, WIN_H - TILE / 2.f - startLane * TILE}, 0.f},
			Drawable{{0, 0, 0, 0}, {TILE - 4.f, TILE - 4.f}},
			LanePos{startLane, startCol},
			PlayerTag{},
			InputState{},
			Health{3},
			Shield{0.f, SHIELD_CHARGES},
			Invincibility{0});

		for (int row = 0; row < ENEMY_ROWS; row++) {
			for (int col = 0; col < ENEMY_COLS; col++) {
				const int lane = ENEMY_TOP_LANE - row;
				const int ecol = ENEMY_START_COL + col;
				Entity::create().addAll(
					Transform{{TILE / 2.f + ecol * TILE, WIN_H - TILE / 2.f - lane * TILE}, 0.f},
					Drawable{{0, 0, 0, 0}, {TILE - 4.f, TILE - 4.f}},
					LanePos{lane, ecol},
					EnemyTag{},
					Health{1});
			}
		}

		// Shelters on lane 3.
		static constexpr float SHELTER_W    = TILE * 2.f - 4.f;
		static constexpr float SHELTER_H    = TILE       - 4.f;
		static constexpr float SHELTER_Y    = WIN_H - TILE / 2.f - 3 * TILE;
		static constexpr float SHELTER_XS[] = { 190.f, 512.f, 834.f };
		for (float sx : SHELTER_XS) {
			Entity::create().addAll(
				Transform{{sx, SHELTER_Y}, 0.f},
				Drawable{{0, 0, 0, 0}, {SHELTER_W, SHELTER_H}},
				Shelter{},
				Health{5});
		}

		// Lane hazards.
		for (const auto& def : HAZARD_DEFS) {
			const float y = WIN_H - TILE / 2.f - def.lane * TILE;
			for (float x : def.xs) {
				Entity::create().addAll(
					Transform{{x, y}, 0.f},
					Drawable{{0, 0, 0, 0}, {HAZARD_W, HAZARD_H}},
					Velocity{def.dx, 0.f},
					Hazard{});
			}
		}

		// Formation controller: holds movement direction + wall-clock timers.
		// Has a dummy Transform so reset() destroys it along with the other gameplay entities.
		Entity::create().addAll(
			Transform{{0.f, 0.f}, 0.f},
			FormationState{1, 0, 0});

		// Game status: end-of-game flags read by run() and endgame_draw().
		// Same dummy-Transform pattern — destroyed and recreated each game.
		Entity::create().addAll(
			Transform{{0.f, 0.f}, 0.f},
			GameStatus{false, false});
	}

	void Game::reset() const
	{
		// Destroy every live gameplay entity (all carry Transform).
		// The SelectState entity has no Transform so it survives here.
		static const Mask anyMask = MaskBuilder().set<Transform>().build();
		for (Entity e = Entity::first(); !e.eof(); e.next()) {
			if (e.test(anyMask)) e.destroy();
		}

		// Free the player sprite so the next spawn_entities() reloads it cleanly.
		if (player_texture) { SDL_DestroyTexture(player_texture); player_texture = nullptr; }

		// Reset character-select debounce on the persistent UI entity.
		static const Mask ssMask = MaskBuilder().set<SelectState>().build();
		for (Entity e = Entity::first(); !e.eof(); e.next()) {
			if (e.test(ssMask)) { e.get<SelectState>().moved = false; break; }
		}

		_state = GameState::Select;
	}

	// -----------------------------------------------------------------------
	// Character select
	// -----------------------------------------------------------------------

	void Game::select_input() const
	{
		static const Mask ssMask = MaskBuilder().set<SelectState>().build();
		Entity ssEnt = Entity::first();
		for (Entity e = Entity::first(); !e.eof(); e.next())
			if (e.test(ssMask)) { ssEnt = e; break; }
		auto& ss = ssEnt.get<SelectState>();

		const bool* keys = SDL_GetKeyboardState(nullptr);
		const bool lr = keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_RIGHT];
		if (lr && !ss.moved) {
			ss.selected = 1 - ss.selected;
			ss.moved    = true;
		} else if (!lr) {
			ss.moved = false;
		}
	}

	void Game::select_draw() const
	{
		static const Mask ssMask = MaskBuilder().set<SelectState>().build();
		int selected = 0;
		for (Entity e = Entity::first(); !e.eof(); e.next())
			if (e.test(ssMask)) { selected = e.get<SelectState>().selected; break; }

		constexpr float BOX_W   = 200.f;
		constexpr float BOX_H   = 240.f;
		constexpr float BOX_GAP = 80.f;
		constexpr float TRUMP_X = (WIN_W - 2 * BOX_W - BOX_GAP) / 2.f;
		constexpr float BIBI_X  = TRUMP_X + BOX_W + BOX_GAP;
		constexpr float BOX_Y   = (WIN_H - BOX_H) / 2.f;

		const float selX = (selected == 0) ? TRUMP_X : BIBI_X;
		SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
		SDL_FRect selBorder = {selX - 6.f, BOX_Y - 6.f, BOX_W + 12.f, BOX_H + 12.f};
		SDL_RenderFillRect(ren, &selBorder);

		SDL_SetRenderDrawColor(ren, 220, 120, 30, 255);
		SDL_FRect trumpBox = {TRUMP_X, BOX_Y, BOX_W, BOX_H};
		SDL_RenderFillRect(ren, &trumpBox);

		SDL_SetRenderDrawColor(ren, 60, 120, 220, 255);
		SDL_FRect bibiBox = {BIBI_X, BOX_Y, BOX_W, BOX_H};
		SDL_RenderFillRect(ren, &bibiBox);

		SDL_SetRenderScale(ren, 4.f, 4.f);
		SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
		SDL_RenderDebugText(ren, 72.f, 46.f, "SELECT FIGHTER");
		SDL_SetRenderScale(ren, 1.f, 1.f);

		SDL_SetRenderScale(ren, 3.f, 3.f);
		SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
		SDL_RenderDebugText(ren, 104.f, 124.f, "TRUMP");
		SDL_RenderDebugText(ren, 201.f, 124.f, "BIBI");
		SDL_SetRenderScale(ren, 1.f, 1.f);

		SDL_SetRenderScale(ren, 2.f, 2.f);
		SDL_SetRenderDrawColor(ren, 180, 180, 180, 255);
		SDL_RenderDebugText(ren, 152.f, 350.f, "LEFT/RIGHT   ENTER to play");
		SDL_SetRenderScale(ren, 1.f, 1.f);
	}

	// -----------------------------------------------------------------------
	// Systems
	// -----------------------------------------------------------------------

	void Game::input_system() const
	{
		static const Mask mask = MaskBuilder()
			.set<PlayerTag>()
			.set<InputState>()
			.build();

		const bool* keys = SDL_GetKeyboardState(nullptr);

		for (Entity e = Entity::first(); !e.eof(); e.next()) {
			if (!e.test(mask)) continue;
			auto& s  = e.get<InputState>();
			s.up     = keys[SDL_SCANCODE_UP];
			s.down   = keys[SDL_SCANCODE_DOWN];
			s.left   = keys[SDL_SCANCODE_LEFT];
			s.right  = keys[SDL_SCANCODE_RIGHT];
			s.shoot    = keys[SDL_SCANCODE_SPACE];
			s.activate = keys[SDL_SCANCODE_I];
		}
	}

	void Game::player_move_system() const
	{
		static const Mask mask = MaskBuilder()
			.set<PlayerTag>()
			.set<InputState>()
			.set<LanePos>()
			.set<Transform>()
			.build();

		for (Entity e = Entity::first(); !e.eof(); e.next()) {
			if (!e.test(mask)) continue;

			auto& s  = e.get<InputState>();
			auto& lp = e.get<LanePos>();
			auto& t  = e.get<Transform>();

			const bool any = s.up || s.down || s.left || s.right;

			if (any && !s.moved) {
				if (s.up)    lp.lane++;
				if (s.down)  lp.lane--;
				if (s.left)  lp.col--;
				if (s.right) lp.col++;
				s.moved = true;
			} else if (!any) {
				s.moved = false;
			}

			lp.lane = clamp(lp.lane, 0, LANES - 1);
			lp.col  = clamp(lp.col,  0, COLS  - 1);

			t.p.x = TILE / 2.f + lp.col  * TILE;
			t.p.y = WIN_H - TILE / 2.f - lp.lane * TILE;
		}
	}

	void Game::enemy_move_system() const
	{
		static const Mask mask = MaskBuilder()
			.set<EnemyTag>()
			.set<LanePos>()
			.set<Transform>()
			.build();
		static const Mask fsMask = MaskBuilder().set<FormationState>().build();

		// Find the formation-state entity and use its timer + direction.
		Entity fsEnt = Entity::first();
		for (Entity e = Entity::first(); !e.eof(); e.next())
			if (e.test(fsMask)) { fsEnt = e; break; }
		auto& fs = fsEnt.get<FormationState>();

		const Uint64 now = SDL_GetTicks();
		if (now - fs.moveTimer < ENEMY_MOVE_MS) return;
		fs.moveTimer = now;

		bool hitEdge = false;
		for (Entity e = Entity::first(); !e.eof(); e.next()) {
			if (!e.test(mask)) continue;
			const int col = e.get<LanePos>().col;
			if ((fs.dir == 1 && col >= COLS - 1) ||
				(fs.dir == -1 && col <= 0)) {
				hitEdge = true;
				break;
			}
		}

		if (hitEdge) fs.dir = -fs.dir;

		for (Entity e = Entity::first(); !e.eof(); e.next()) {
			if (!e.test(mask)) continue;
			auto& lp = e.get<LanePos>();
			auto& t  = e.get<Transform>();
			if (hitEdge) {
				lp.lane--;
			} else {
				lp.col += fs.dir;
			}
			t.p.x = TILE / 2.f + lp.col  * TILE;
			t.p.y = WIN_H - TILE / 2.f - lp.lane * TILE;
		}
	}

	void Game::shoot_system() const
	{
		static const Mask playerMask = MaskBuilder()
			.set<PlayerTag>().set<InputState>().set<Transform>().build();
		static const Mask enemyMask  = MaskBuilder()
			.set<EnemyTag>().set<LanePos>().set<Transform>().build();
		static const Mask fsMask     = MaskBuilder().set<FormationState>().build();

		for (Entity e = Entity::first(); !e.eof(); e.next()) {
			if (!e.test(playerMask)) continue;
			auto& s = e.get<InputState>();
			if (s.shoot && !s.shotFired) {
				const SDL_FPoint p = e.get<Transform>().p;
				Entity::create().addAll(
					Transform{p, 0.f},
					Drawable{{0, 0, 0, 0}, {6.f, 14.f}},
					BulletTag{true},
					Velocity{0.f, -BULLET_SPEED});
				s.shotFired = true;
			} else if (!s.shoot) {
				s.shotFired = false;
			}
		}

		// Find the formation-state entity and use its shoot timer.
		Entity fsEnt = Entity::first();
		for (Entity e = Entity::first(); !e.eof(); e.next())
			if (e.test(fsMask)) { fsEnt = e; break; }
		auto& fs = fsEnt.get<FormationState>();

		const Uint64 now = SDL_GetTicks();
		if (now - fs.shootTimer < ENEMY_SHOOT_MS) return;
		fs.shootTimer = now;

		int minLane = LANES;
		ent_type shooter{-1};
		for (Entity e = Entity::first(); !e.eof(); e.next()) {
			if (!e.test(enemyMask)) continue;
			const int lane = e.get<LanePos>().lane;
			if (lane < minLane) {
				minLane = lane;
				shooter  = e.entity();
			}
		}
		if (shooter.id >= 0) {
			const SDL_FPoint p = Entity(shooter).get<Transform>().p;
			Entity::create().addAll(
				Transform{p, 0.f},
				Drawable{{0, 0, 0, 0}, {6.f, 14.f}},
				BulletTag{false},
				Velocity{0.f, BULLET_SPEED});
		}
	}

	void Game::bullet_system() const
	{
		static const Mask mask = MaskBuilder()
			.set<BulletTag>().set<Transform>().set<Velocity>().build();

		for (Entity e = Entity::first(); !e.eof(); e.next()) {
			if (!e.test(mask)) continue;
			auto& t = e.get<Transform>();
			const auto& v = e.get<Velocity>();
			t.p.x += v.dx;
			t.p.y += v.dy;
			if (t.p.y < 0 || t.p.y > WIN_H) {
				e.destroy();
			}
		}
	}

	void Game::hazard_move_system() const
	{
		static const Mask mask = MaskBuilder()
			.set<Hazard>().set<Transform>().set<Drawable>().set<Velocity>().build();

		for (Entity e = Entity::first(); !e.eof(); e.next()) {
			if (!e.test(mask)) continue;
			auto& t        = e.get<Transform>();
			const float dx = e.get<Velocity>().dx;
			const float hw = e.get<Drawable>().size.x / 2.f;
			t.p.x += dx;
			if (t.p.x - hw >  WIN_W) t.p.x = -hw;
			if (t.p.x + hw <  0)     t.p.x =  WIN_W + hw;
		}
	}

	void Game::shield_system() const
	{
		static const Mask mask = MaskBuilder()
			.set<PlayerTag>().set<InputState>().set<Shield>().build();

		for (Entity e = Entity::first(); !e.eof(); e.next()) {
			if (!e.test(mask)) continue;
			auto& s  = e.get<InputState>();
			auto& sh = e.get<Shield>();

			if (s.activate && !s.activateFired && sh.charges > 0 && sh.timer <= 0.f) {
				sh.timer = SHIELD_DURATION;
				sh.charges--;
				s.activateFired = true;
			} else if (!s.activate) {
				s.activateFired = false;
			}

			if (sh.timer > 0.f) sh.timer -= 1.f;
		}
	}

	void Game::collision_system() const
	{
		static const Mask gsMask = MaskBuilder().set<GameStatus>().build();
		Entity gsEnt = Entity::first();
		for (Entity e = Entity::first(); !e.eof(); e.next())
			if (e.test(gsMask)) { gsEnt = e; break; }
		auto& gs = gsEnt.get<GameStatus>();
		if (gs.gameOver || gs.won) return;

		static const Mask playerMask  = MaskBuilder()
			.set<PlayerTag>().set<Transform>().set<Drawable>().set<Health>().build();
		static const Mask enemyMask   = MaskBuilder()
			.set<EnemyTag>().set<Transform>().set<Drawable>().set<Health>().build();
		static const Mask bulletMask  = MaskBuilder()
			.set<BulletTag>().set<Transform>().set<Drawable>().build();
		static const Mask shelterMask = MaskBuilder()
			.set<Shelter>().set<Transform>().set<Drawable>().set<Health>().build();

		// Decrement the player's post-hit invincibility each frame.
		for (Entity e = Entity::first(); !e.eof(); e.next()) {
			if (!e.test(playerMask)) continue;
			if (!e.has<Invincibility>()) continue;
			auto& inv = e.get<Invincibility>();
			if (inv.frames > 0) --inv.frames;
			break;
		}

		auto overlaps = [](SDL_FPoint p1, SDL_FPoint s1, SDL_FPoint p2, SDL_FPoint s2) {
			return std::abs(p1.x - p2.x) < (s1.x + s2.x) * 0.5f &&
			       std::abs(p1.y - p2.y) < (s1.y + s2.y) * 0.5f;
		};

		// Bullet × Shelter
		for (Entity b = Entity::first(); !b.eof(); b.next()) {
			if (!b.test(bulletMask)) continue;
			const SDL_FPoint bp = b.get<Transform>().p;
			const SDL_FPoint bs = b.get<Drawable>().size;

			for (Entity sh = Entity::first(); !sh.eof(); sh.next()) {
				if (!sh.test(shelterMask)) continue;
				if (!overlaps(bp, bs, sh.get<Transform>().p, sh.get<Drawable>().size)) continue;
				b.destroy();
				if (--sh.get<Health>().hp <= 0) sh.destroy();
				break;
			}
		}

		// Player bullet × Enemy
		for (Entity b = Entity::first(); !b.eof(); b.next()) {
			if (!b.test(bulletMask)) continue;
			if (!b.get<BulletTag>().fromPlayer) continue;
			const SDL_FPoint bp = b.get<Transform>().p;
			const SDL_FPoint bs = b.get<Drawable>().size;

			for (Entity en = Entity::first(); !en.eof(); en.next()) {
				if (!en.test(enemyMask)) continue;
				if (!overlaps(bp, bs, en.get<Transform>().p, en.get<Drawable>().size)) continue;
				b.destroy();
				if (--en.get<Health>().hp <= 0) en.destroy();
				break;
			}
		}

		// Enemy bullet × Player
		for (Entity b = Entity::first(); !b.eof(); b.next()) {
			if (!b.test(bulletMask)) continue;
			if (b.get<BulletTag>().fromPlayer) continue;
			const SDL_FPoint bp = b.get<Transform>().p;
			const SDL_FPoint bs = b.get<Drawable>().size;

			for (Entity pl = Entity::first(); !pl.eof(); pl.next()) {
				if (!pl.test(playerMask)) continue;
				if (!overlaps(bp, bs, pl.get<Transform>().p, pl.get<Drawable>().size)) continue;
				const bool shielded = pl.get<Shield>().timer > 0.f;
				b.destroy();
				if (!shielded && pl.get<Invincibility>().frames == 0) {
					if (--pl.get<Health>().hp <= 0) gs.gameOver = true;
					pl.get<Invincibility>().frames = 90;
				}
				break;
			}
		}

		// Hazard × Player
		static const Mask hazardMask = MaskBuilder()
			.set<Hazard>().set<Transform>().set<Drawable>().build();
		for (Entity h = Entity::first(); !h.eof(); h.next()) {
			if (!h.test(hazardMask)) continue;
			for (Entity pl = Entity::first(); !pl.eof(); pl.next()) {
				if (!pl.test(playerMask)) continue;
				if (pl.get<Invincibility>().frames > 0) continue;
				if (!overlaps(h.get<Transform>().p, h.get<Drawable>().size,
				              pl.get<Transform>().p, pl.get<Drawable>().size)) continue;
				if (--pl.get<Health>().hp <= 0) gs.gameOver = true;
				pl.get<Invincibility>().frames = 90;
				break;
			}
		}

		// Enemy × Player — overrun or direct touch
		for (Entity en = Entity::first(); !en.eof(); en.next()) {
			if (!en.test(enemyMask)) continue;
			if (en.get<LanePos>().lane < 0) { gs.gameOver = true; return; }

			for (Entity pl = Entity::first(); !pl.eof(); pl.next()) {
				if (!pl.test(playerMask)) continue;
				if (!overlaps(en.get<Transform>().p, en.get<Drawable>().size,
				              pl.get<Transform>().p, pl.get<Drawable>().size)) continue;
				gs.gameOver = true;
				return;
			}
		}

		// Win: no enemies left
		bool anyEnemy = false;
		for (Entity e = Entity::first(); !e.eof(); e.next()) {
			if (e.test(enemyMask)) { anyEnemy = true; break; }
		}
		if (!anyEnemy) gs.won = true;
	}

	void Game::draw_system() const
	{
		static const Mask drawMask    = MaskBuilder().set<Transform>().set<Drawable>().build();
		static const Mask playerMask  = MaskBuilder().set<PlayerTag>().set<Shield>().build();
		static const Mask enemyMask   = MaskBuilder().set<EnemyTag>().build();
		static const Mask bulletMask  = MaskBuilder().set<BulletTag>().build();
		static const Mask hazardMask2 = MaskBuilder().set<Hazard>().build();
		static const Mask shelterDMsk = MaskBuilder().set<Shelter>().set<Health>().build();

		for (Entity e = Entity::first(); !e.eof(); e.next()) {
			if (!e.test(drawMask)) continue;

			const auto& t = e.get<Transform>();
			const auto& d = e.get<Drawable>();

			SDL_FRect dest = {
				t.p.x - d.size.x / 2,
				t.p.y - d.size.y / 2,
				d.size.x, d.size.y
			};

			if (e.test(playerMask) && e.get<Shield>().timer > 0.f) {
				SDL_SetRenderDrawColor(ren, 80, 140, 255, 255);
				SDL_FRect halo = {dest.x - 7, dest.y - 7, dest.w + 14, dest.h + 14};
				SDL_RenderFillRect(ren, &halo);
			}

			if (e.test(playerMask)) {
				// Draw the character sprite if loaded, otherwise fall back to a colour rect.
				if (player_texture) {
					SDL_RenderTexture(ren, player_texture, nullptr, &dest);
				} else {
					SDL_SetRenderDrawColor(ren, 220, 200, 50, 255);
					SDL_RenderFillRect(ren, &dest);
				}
			} else if (e.test(bulletMask)) {
				if (e.get<BulletTag>().fromPlayer)
					SDL_SetRenderDrawColor(ren, 255, 255,   0, 255);
				else
					SDL_SetRenderDrawColor(ren, 255, 140,   0, 255);
				SDL_RenderFillRect(ren, &dest);
			} else if (e.test(shelterDMsk)) {
				const int hp    = e.get<Health>().hp;
				const Uint8 v   = static_cast<Uint8>(50 + hp * 26);
				SDL_SetRenderDrawColor(ren, v, v, v, 255);
				SDL_RenderFillRect(ren, &dest);
			} else if (e.test(hazardMask2)) {
				SDL_SetRenderDrawColor(ren,   0, 200, 220, 255);
				SDL_RenderFillRect(ren, &dest);
			} else if (e.test(enemyMask)) {
				SDL_SetRenderDrawColor(ren, 220,  50,  50, 255);
				SDL_RenderFillRect(ren, &dest);
			}
			// Formation/GameStatus entities have no Drawable — never reach here.
		}

		SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);

		// HUD: life icons and shield charges.
		static const Mask hpMask2 = MaskBuilder().set<PlayerTag>().set<Health>().set<Shield>().build();
		for (Entity e = Entity::first(); !e.eof(); e.next()) {
			if (!e.test(hpMask2)) continue;
			const int hp   = e.get<Health>().hp;
			const auto& sh = e.get<Shield>();

			for (int i = 0; i < 3; i++) {
				SDL_FRect icon = {10.f + i * 26.f, 10.f, 20.f, 20.f};
				SDL_SetRenderDrawColor(ren, i < hp ? 0 : 60, i < hp ? 220 : 60, i < hp ? 80 : 60, 255);
				SDL_RenderFillRect(ren, &icon);
			}

			for (int i = 0; i < SHIELD_CHARGES; i++) {
				SDL_FRect icon = {100.f + i * 26.f, 10.f, 20.f, 20.f};
				const bool avail = i < sh.charges || (i == 0 && sh.timer > 0.f);
				SDL_SetRenderDrawColor(ren, avail ? 80 : 60, avail ? 140 : 60, avail ? 255 : 60, 255);
				SDL_RenderFillRect(ren, &icon);
			}
			break;
		}

		SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
	}

	void Game::endgame_draw() const
	{
		SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);
		const SDL_FRect full = {0, 0, (float)WIN_W, (float)WIN_H};
		SDL_RenderFillRect(ren, &full);
		SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

		SDL_SetRenderScale(ren, 4.f, 4.f);

		static const Mask gsMask = MaskBuilder().set<GameStatus>().build();
		bool gameOver = false;
		for (Entity e = Entity::first(); !e.eof(); e.next())
			if (e.test(gsMask)) { gameOver = e.get<GameStatus>().gameOver; break; }

		if (gameOver) {
			SDL_SetRenderDrawColor(ren, 255, 80, 80, 255);
			SDL_RenderDebugText(ren, 64.f, 52.f, "GAME OVER");
		} else {
			SDL_SetRenderDrawColor(ren, 80, 255, 80, 255);
			SDL_RenderDebugText(ren, 68.f, 52.f, "YOU WIN!");
		}

		SDL_SetRenderDrawColor(ren, 210, 210, 210, 255);
		SDL_RenderDebugText(ren, 28.f, 72.f, "Press R to restart");

		SDL_SetRenderScale(ren, 1.f, 1.f);
	}

	// -----------------------------------------------------------------------
	// Main loop
	// -----------------------------------------------------------------------

	void Game::run()
	{
		auto start = SDL_GetTicks();
		bool quit  = false;

		static const Mask gsMask = MaskBuilder().set<GameStatus>().build();

		while (!quit) {
			bool gameOver = false, won = false;

			if (_state == GameState::Select) {
				select_input();
			} else {
				// Read game status from ECS (entity exists during Playing state).
				for (Entity e = Entity::first(); !e.eof(); e.next())
					if (e.test(gsMask)) { gameOver = e.get<GameStatus>().gameOver; won = e.get<GameStatus>().won; break; }

				if (!gameOver && !won) {
					input_system();
					player_move_system();
					enemy_move_system();
					shoot_system();
					bullet_system();
					hazard_move_system();
					shield_system();
					collision_system();
					// Re-read: collision_system may have just set a flag.
					for (Entity e = Entity::first(); !e.eof(); e.next())
						if (e.test(gsMask)) { gameOver = e.get<GameStatus>().gameOver; won = e.get<GameStatus>().won; break; }
				}
			}

			SDL_RenderClear(ren);
			if (_state == GameState::Select) {
				select_draw();
			} else {
				draw_system();
				if (gameOver || won) endgame_draw();
			}
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
				if (e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_RETURN &&
					_state == GameState::Select) {
					_state = GameState::Playing;
					spawn_entities();
				}
				if (e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_R &&
					(gameOver || won))
					reset();
			}
		}
	}
}
