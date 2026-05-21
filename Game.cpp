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
	}

	Game::~Game()
	{
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
		const int startCol  = COLS / 2;
		const int startLane = 0;
		Entity::create().addAll(
			Transform{{TILE / 2.f + startCol * TILE, WIN_H - TILE / 2.f - startLane * TILE}, 0.f},
			Drawable{{0, 0, 0, 0}, {TILE - 4.f, TILE - 4.f}},
			LanePos{startLane, startCol},
			PlayerTag{},
			InputState{},
			Health{3},
			Shield{0.f, SHIELD_CHARGES});

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

		// Shelters on lane 3 — one clear empty lane (lane 2) between player and shelter.
		// Enemy bullets hit them first (collision order) before they can reach the player.
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

		// Lane hazards — each moves horizontally and wraps around the screen.
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
	}

	void Game::reset() const
	{
		// Destroy every live entity (all game entities carry Transform).
		static const Mask anyMask = MaskBuilder().set<Transform>().build();
		for (Entity e = Entity::first(); !e.eof(); e.next()) {
			if (e.test(anyMask)) e.destroy();
		}

		_formationDir    = 1;
		_enemyTimer      = 0;
		_enemyShootTimer = 0;
		_gameOver        = false;
		_won             = false;
		_invincFrames    = 0;
		_state           = GameState::Select;
		_selectMoved     = false;
	}

	// -----------------------------------------------------------------------
	// Character select
	// -----------------------------------------------------------------------

	void Game::select_input() const
	{
		const bool* keys = SDL_GetKeyboardState(nullptr);
		const bool lr = keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_RIGHT];
		if (lr && !_selectMoved) {
			_selectedChar = 1 - _selectedChar;
			_selectMoved  = true;
		} else if (!lr) {
			_selectMoved = false;
		}
	}

	void Game::select_draw() const
	{
		constexpr float BOX_W   = 200.f;
		constexpr float BOX_H   = 240.f;
		constexpr float BOX_GAP = 80.f;
		// Center both boxes in the window horizontally.
		constexpr float TRUMP_X = (WIN_W - 2 * BOX_W - BOX_GAP) / 2.f;  // 272
		constexpr float BIBI_X  = TRUMP_X + BOX_W + BOX_GAP;              // 552
		constexpr float BOX_Y   = (WIN_H - BOX_H) / 2.f;                  // 264

		// White selection border drawn slightly larger than the active box.
		const float selX = (_selectedChar == 0) ? TRUMP_X : BIBI_X;
		SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
		SDL_FRect selBorder = {selX - 6.f, BOX_Y - 6.f, BOX_W + 12.f, BOX_H + 12.f};
		SDL_RenderFillRect(ren, &selBorder);

		SDL_SetRenderDrawColor(ren, 220, 120, 30, 255);  // Trump — orange
		SDL_FRect trumpBox = {TRUMP_X, BOX_Y, BOX_W, BOX_H};
		SDL_RenderFillRect(ren, &trumpBox);

		SDL_SetRenderDrawColor(ren, 60, 120, 220, 255);  // Bibi — blue
		SDL_FRect bibiBox = {BIBI_X, BOX_Y, BOX_W, BOX_H};
		SDL_RenderFillRect(ren, &bibiBox);

		// Title: scale 4 → logical 256×192.
		// "SELECT FIGHTER" = 14 chars * 8px = 112 logical. Centre X = (256-112)/2 = 72.
		SDL_SetRenderScale(ren, 4.f, 4.f);
		SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
		SDL_RenderDebugText(ren, 72.f, 46.f, "SELECT FIGHTER");
		SDL_SetRenderScale(ren, 1.f, 1.f);

		// Character names: scale 3 → logical 341×256.
		// "TRUMP" physical = 5*8*3=120. Trump box centre physical = 372. Start = 312. Logical = 104.
		// "BIBI"  physical = 4*8*3=96.  Bibi box centre physical = 652.  Start = 604. Logical = 201.
		// Label Y: box centre physical = BOX_Y+BOX_H/2=384. Logical = 384/3 = 128 (minus half char).
		SDL_SetRenderScale(ren, 3.f, 3.f);
		SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
		SDL_RenderDebugText(ren, 104.f, 124.f, "TRUMP");
		SDL_RenderDebugText(ren, 201.f, 124.f, "BIBI");
		SDL_SetRenderScale(ren, 1.f, 1.f);

		// Hint: scale 2 → logical 512×384.
		// "LEFT/RIGHT   ENTER to play" = 26 chars * 8px * 2 = 416 physical. Centre X = (1024-416)/2=304 → logical 152.
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

		const Uint64 now = SDL_GetTicks();
		if (now - _enemyTimer < ENEMY_MOVE_MS) return;
		_enemyTimer = now;

		bool hitEdge = false;
		for (Entity e = Entity::first(); !e.eof(); e.next()) {
			if (!e.test(mask)) continue;
			const int col = e.get<LanePos>().col;
			if ((_formationDir == 1 && col >= COLS - 1) ||
				(_formationDir == -1 && col <= 0)) {
				hitEdge = true;
				break;
			}
		}

		if (hitEdge) _formationDir = -_formationDir;

		for (Entity e = Entity::first(); !e.eof(); e.next()) {
			if (!e.test(mask)) continue;
			auto& lp = e.get<LanePos>();
			auto& t  = e.get<Transform>();
			if (hitEdge) {
				lp.lane--;
			} else {
				lp.col += _formationDir;
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

		const Uint64 now = SDL_GetTicks();
		if (now - _enemyShootTimer < ENEMY_SHOOT_MS) return;
		_enemyShootTimer = now;

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
			// Wrap: when fully off one side, re-enter from the other.
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

			// Activate on I press: must have charges left and no shield already running.
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
		if (_gameOver || _won) return;

		// Decrement invincibility window each frame.
		if (_invincFrames > 0) --_invincFrames;

		static const Mask playerMask  = MaskBuilder()
			.set<PlayerTag>().set<Transform>().set<Drawable>().set<Health>().build();
		static const Mask enemyMask   = MaskBuilder()
			.set<EnemyTag>().set<Transform>().set<Drawable>().set<Health>().build();
		static const Mask bulletMask  = MaskBuilder()
			.set<BulletTag>().set<Transform>().set<Drawable>().build();
		static const Mask shelterMask = MaskBuilder()
			.set<Shelter>().set<Transform>().set<Drawable>().set<Health>().build();

		auto overlaps = [](SDL_FPoint p1, SDL_FPoint s1, SDL_FPoint p2, SDL_FPoint s2) {
			return std::abs(p1.x - p2.x) < (s1.x + s2.x) * 0.5f &&
			       std::abs(p1.y - p2.y) < (s1.y + s2.y) * 0.5f;
		};

		// Bullet × Shelter — both player and enemy bullets chip shelters.
		// Checked first so shelters can intercept enemy bullets before they reach the player.
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
				// Shield absorbs the bullet with no HP loss.
				// Without shield, normal invincibility-gated damage applies.
				const bool shielded = pl.get<Shield>().timer > 0.f;
				b.destroy();
				if (!shielded && _invincFrames == 0) {
					if (--pl.get<Health>().hp <= 0) _gameOver = true;
					_invincFrames = 90;
				}
				break;
			}
		}

		// Hazard × Player
		static const Mask hazardMask = MaskBuilder()
			.set<Hazard>().set<Transform>().set<Drawable>().build();
		if (_invincFrames == 0) {
			for (Entity h = Entity::first(); !h.eof(); h.next()) {
				if (!h.test(hazardMask)) continue;
				for (Entity pl = Entity::first(); !pl.eof(); pl.next()) {
					if (!pl.test(playerMask)) continue;
					if (!overlaps(h.get<Transform>().p, h.get<Drawable>().size,
					              pl.get<Transform>().p, pl.get<Drawable>().size)) continue;
					auto& hp = pl.get<Health>();
					if (--hp.hp <= 0) _gameOver = true;
					_invincFrames = 90;
					break;
				}
			}
		}

		// Enemy × Player — overrun or direct touch
		for (Entity en = Entity::first(); !en.eof(); en.next()) {
			if (!en.test(enemyMask)) continue;
			if (en.get<LanePos>().lane < 0) { _gameOver = true; return; }

			for (Entity pl = Entity::first(); !pl.eof(); pl.next()) {
				if (!pl.test(playerMask)) continue;
				if (!overlaps(en.get<Transform>().p, en.get<Drawable>().size,
				              pl.get<Transform>().p, pl.get<Drawable>().size)) continue;
				_gameOver = true;
				return;
			}
		}

		// Win: no enemies left
		bool anyEnemy = false;
		for (Entity e = Entity::first(); !e.eof(); e.next()) {
			if (e.test(enemyMask)) { anyEnemy = true; break; }
		}
		if (!anyEnemy) _won = true;
	}

	void Game::draw_system() const
	{
		static const Mask drawMask    = MaskBuilder().set<Transform>().set<Drawable>().build();
		static const Mask enemyMask   = MaskBuilder().set<EnemyTag>().build();
		static const Mask bulletMask  = MaskBuilder().set<BulletTag>().build();
		static const Mask hazardMask2 = MaskBuilder().set<Hazard>().build();
		static const Mask shieldMask   = MaskBuilder().set<PlayerTag>().set<Shield>().build();
		static const Mask shelterDMask = MaskBuilder().set<Shelter>().set<Health>().build();

		for (Entity e = Entity::first(); !e.eof(); e.next()) {
			if (!e.test(drawMask)) continue;

			const auto& t = e.get<Transform>();
			const auto& d = e.get<Drawable>();

			SDL_FRect dest = {
				t.p.x - d.size.x / 2,
				t.p.y - d.size.y / 2,
				d.size.x, d.size.y
			};

			// Blue halo behind the player when shield is active.
			if (e.test(shieldMask) && e.get<Shield>().timer > 0.f) {
				SDL_SetRenderDrawColor(ren, 80, 140, 255, 255);
				SDL_FRect halo = {dest.x - 7, dest.y - 7, dest.w + 14, dest.h + 14};
				SDL_RenderFillRect(ren, &halo);
			}

			if (e.test(bulletMask)) {
				if (e.get<BulletTag>().fromPlayer)
					SDL_SetRenderDrawColor(ren, 255, 255,   0, 255); // yellow
				else
					SDL_SetRenderDrawColor(ren, 255, 140,   0, 255); // orange
			} else if (e.test(shelterDMask)) {
				// Shade from light grey (full HP) to dark grey (nearly destroyed).
				const int hp    = e.get<Health>().hp;
				const Uint8 v   = static_cast<Uint8>(50 + hp * 26); // 5→180, 3→128, 1→76
				SDL_SetRenderDrawColor(ren, v, v, v, 255);
			} else if (e.test(hazardMask2)) {
				SDL_SetRenderDrawColor(ren,   0, 200, 220, 255);     // cyan
			} else if (e.test(enemyMask)) {
				SDL_SetRenderDrawColor(ren, 220,  50,  50, 255);     // red
			} else {
				if (_selectedChar == 0)
					SDL_SetRenderDrawColor(ren, 220, 120,  30, 255); // Trump – orange
				else
					SDL_SetRenderDrawColor(ren,  60, 120, 220, 255); // Bibi – blue
			}

			SDL_RenderFillRect(ren, &dest);
		}

		SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);

		// HUD: life icons in the top-left corner.
		static const Mask hpMask2 = MaskBuilder().set<PlayerTag>().set<Health>().set<Shield>().build();
		for (Entity e = Entity::first(); !e.eof(); e.next()) {
			if (!e.test(hpMask2)) continue;
			const int hp      = e.get<Health>().hp;
			const auto& sh    = e.get<Shield>();

			// HP icons (green / dark)
			for (int i = 0; i < 3; i++) {
				SDL_FRect icon = {10.f + i * 26.f, 10.f, 20.f, 20.f};
				SDL_SetRenderDrawColor(ren, i < hp ? 0 : 60, i < hp ? 220 : 60, i < hp ? 80 : 60, 255);
				SDL_RenderFillRect(ren, &icon);
			}

			// Shield-charge icons (blue / dark), offset to the right of HP icons
			for (int i = 0; i < SHIELD_CHARGES; i++) {
				SDL_FRect icon = {100.f + i * 26.f, 10.f, 20.f, 20.f};
				const bool avail = i < sh.charges || (i == 0 && sh.timer > 0.f);
				SDL_SetRenderDrawColor(ren, avail ? 80 : 60, avail ? 140 : 60, avail ? 255 : 60, 255);
				SDL_RenderFillRect(ren, &icon);
			}
			break;
		}

		SDL_SetRenderDrawColor(ren, 0, 0, 0, 255); // restore clear color after HUD
	}

	void Game::endgame_draw() const
	{
		// Semi-transparent dark overlay over the frozen game frame.
		SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);
		const SDL_FRect full = {0, 0, (float)WIN_W, (float)WIN_H};
		SDL_RenderFillRect(ren, &full);
		SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

		// Scale 4× so the 8-px debug font renders at 32-px screen height.
		// Logical size becomes WIN_W/4 = 200 × WIN_H/4 = 150.
		SDL_SetRenderScale(ren, 4.f, 4.f);

		if (_gameOver) {
			SDL_SetRenderDrawColor(ren, 255, 80, 80, 255);
			SDL_RenderDebugText(ren, 64.f, 52.f, "GAME OVER");   // (200-9*8)/2 = 64
		} else {
			SDL_SetRenderDrawColor(ren, 80, 255, 80, 255);
			SDL_RenderDebugText(ren, 68.f, 52.f, "YOU WIN!");    // (200-8*8)/2 = 68
		}

		SDL_SetRenderDrawColor(ren, 210, 210, 210, 255);
		SDL_RenderDebugText(ren, 28.f, 72.f, "Press R to restart"); // (200-18*8)/2 = 28

		SDL_SetRenderScale(ren, 1.f, 1.f);
	}

	// -----------------------------------------------------------------------
	// Main loop
	// -----------------------------------------------------------------------

	void Game::run()
	{
		auto start = SDL_GetTicks();
		bool quit  = false;

		while (!quit) {
			if (_state == GameState::Select) {
				select_input();
			} else if (!_gameOver && !_won) {
				input_system();
				player_move_system();
				enemy_move_system();
				shoot_system();
				bullet_system();
				hazard_move_system();
				shield_system();
				collision_system();
			}

			SDL_RenderClear(ren);
			if (_state == GameState::Select) {
				select_draw();
			} else {
				draw_system();
				if (_gameOver || _won) endgame_draw();
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
					(_gameOver || _won))
					reset();
			}
		}
	}
}
