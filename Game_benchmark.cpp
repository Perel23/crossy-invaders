#include "Game.h"
#include "Game_constants.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <vector>
using namespace std;
using namespace bagel;

namespace ci
{
    static constexpr int BENCH_ENEMY_ROWS = 10;
    static constexpr int BENCH_ENEMY_COLS = 15;   // 150 enemies total
    static constexpr int BENCH_ENEMY_MIN  = 80;   // respawn threshold

    static constexpr float HAZARD_W = Game::TILE * 2.f - 4.f;
    static constexpr float HAZARD_H = Game::TILE       - 4.f;

    void Game::spawn_benchmark() const
    {
        // ── Load textures ──
        if (!enemy_texture) {
            SDL_Surface* s = IMG_Load("res/iranian_regime_pixel.png");
            if (s) { enemy_texture = SDL_CreateTextureFromSurface(ren, s); SDL_DestroySurface(s); }
        }
        const char* hazard_files[] = {
            "res/usa_presidential_limousine.png", "res/wing_of_zion.png",
            "res/tank_merkava_4.png",             "res/iranian_karrar_tank.png"
        };
        for (int i = 0; i < 4; i++) {
            if (!haz_textures[i]) {
                SDL_Surface* surf = IMG_Load(hazard_files[i]);
                if (surf) { haz_textures[i] = SDL_CreateTextureFromSurface(ren, surf); SDL_DestroySurface(surf); }
            }
        }
        // Load the currently selected character's texture
        {
            static const char* char_files[NUM_CHARS] = {
                "res/trump_pixel.png", "res/bibi_pixel.png", "res/bengvir_pixel.png",
                "res/zelensky_pixel.png", "res/putin_pixel.png", "res/obama_pixel.png",
                "res/eminem_pixel.png", "res/madonna_pixel.png", "res/michaeljackson_pixel.png",
                "res/sara_pixel.png", "res/stalin_pixel.png", "res/yoamashit_pixel.png"
            };
            static const Mask ssMask = MaskBuilder().set<SelectState>().build();
            int selected = 0;
            for (Entity e = Entity::first(); !e.eof(); e.next())
                if (e.test(ssMask)) { selected = e.get<SelectState>().selected; break; }
            if (player_texture) { SDL_DestroyTexture(player_texture); player_texture = nullptr; }
            SDL_Surface* ps = IMG_Load(char_files[selected < NUM_CHARS ? selected : 0]);
            if (ps) { player_texture = SDL_CreateTextureFromSurface(ren, ps); SDL_DestroySurface(ps); }
        }

        // ── Enemies: 10 rows × 15 cols spread across the screen ──
        for (int row = 0; row < BENCH_ENEMY_ROWS; row++) {
            for (int col = 0; col < BENCH_ENEMY_COLS; col++) {
                const int lane = 2 + row;
                const int ecol = col + (COLS - BENCH_ENEMY_COLS) / 2;
                const float phase = (col * 13 + row * 7) % 100 * 0.063f;
                Entity::create().addAll(
                    Transform{{TILE / 2.f + ecol * TILE, WIN_H - TILE / 2.f - lane * TILE}, 0.f},
                    Drawable{{0, 0, 0, 0}, {TILE - 4.f, TILE - 4.f}},
                    LanePos{lane, ecol}, EnemyTag{}, Health{20},
                    IndividualMove{SDL_GetTicks() + (Uint64)(std::rand() % 800)},
                    BreatheState{phase, 0.05f, 2.f});
            }
        }

        // ── 6 hazards (3 lanes, 2 directions) ──
        struct HazInfo { int lane; float dx; int tex; };
        const float spd = 3.5f;
        const HazInfo hz[] = { {5,  spd, 0}, {7, -spd, 2}, {9,  spd, 3} };
        for (const auto& h : hz) {
            const float hy = WIN_H - TILE / 2.f - h.lane * TILE;
            for (int k = 0; k < 2; k++) {
                const float initPhase = (float)(std::rand() % 628) * 0.01f;
                Entity::create().addAll(
                    Transform{{(float)(std::rand() % WIN_W), hy}, 0.f},
                    Drawable{{0, 0, 0, 0}, {HAZARD_W, HAZARD_H}},
                    Velocity{h.dx, 0.f}, Hazard{}, HazardVisual{h.tex},
                    BreatheState{initPhase, 0.15f, 1.5f});
            }
        }

        // ── Invincible auto-shooting player ──
        Entity::create().addAll(
            Transform{{WIN_W / 2.f, WIN_H - TILE * 1.5f}, 0.f},
            Drawable{{0, 0, 0, 0}, {TILE - 4.f, TILE - 4.f}},
            LanePos{1, COLS / 2},
            PlayerTag{},
            InputState{false,false,false,false,false,/*shoot=*/true,/*shotFired=*/false,false,false,false,false,false},
            Health{9999},
            Shield{0.f, 0},
            Invincibility{999999},
            DamageFlash{0}, ShieldFlash{0},
            RapidFire{999999, 0},   // always rapid fire
            DashState{0},
            SpreadShot{999999},     // always spread shot (3 bullets per shot)
            HopState{0, 10, 0.f},
            BreatheState{0.f, 0.05f, 2.5f},
            BlinkPhase{true, 0, 8});

        // ── Shelters (3) ──
        static constexpr float SHELTER_W  = TILE * 2.f - 4.f;
        static constexpr float SHELTER_H  = TILE - 4.f;
        static constexpr float SHELTER_XS[] = { 190.f, 512.f, 834.f };
        for (float sx : SHELTER_XS)
            Entity::create().addAll(
                Transform{{sx, WIN_H - TILE / 2.f - 3 * TILE}, 0.f},
                Drawable{{0, 0, 0, 0}, {SHELTER_W, SHELTER_H}},
                Shelter{}, Health{50});  // tougher shelters so they last longer

        // ── Global state ──
        Entity::create().addAll(Transform{{0.f, 0.f}, 0.f}, FormationState{1, 0, 0});
        Entity::create().addAll(
            Transform{{0.f, 0.f}, 0.f},
            GameStatus{false, false, false, 0, 0, 0, SDL_GetTicks(), 0},
            ScreenShake{0},
            ComboState{0, 0, 1},
            SlowMo{0, 0},
            SpeedScale{1.0f, 1.0f});
        // NOTE: SelectState entity persists from constructor — don't create another here

        _current_level     = 2;   // IndividualMove behaviour + level-2 bullet direction
        _difficulty        = 1;
        _camera_scroll     = 0.f;
        _camera_grace      = 999999;
        _score             = 0;
        _bench_timer       = SDL_GetTicks();
        _bench_frame       = 0;
        _bench_max_speedup = 0;   // reset peak on every new benchmark run
        _wave_enemy_count  = BENCH_ENEMY_ROWS * BENCH_ENEMY_COLS;
    }

    // Spawns a fresh batch of enemies (called when count drops below threshold)
    static void spawn_bench_enemies(int howMany)
    {
        for (int i = 0; i < howMany; i++) {
            const int row  = std::rand() % BENCH_ENEMY_ROWS;
            const int col  = std::rand() % BENCH_ENEMY_COLS;
            const int lane = 2 + row;
            const int ecol = col + (Game::COLS - BENCH_ENEMY_COLS) / 2;
            const float phase = (float)(std::rand() % 100) * 0.063f;
            Entity::create().addAll(
                Transform{{Game::TILE / 2.f + ecol * Game::TILE, Game::WIN_H - Game::TILE / 2.f - lane * Game::TILE}, 0.f},
                Drawable{{0, 0, 0, 0}, {Game::TILE - 4.f, Game::TILE - 4.f}},
                LanePos{lane, ecol}, EnemyTag{}, Health{20},
                IndividualMove{SDL_GetTicks() + (Uint64)(std::rand() % 800)},
                BreatheState{phase, 0.05f, 2.f});
        }
    }

    void Game::bench_system() const
    {
        // ── Keep player permanently invincible and auto-shooting ──
        static const Mask playerMask = MaskBuilder().set<PlayerTag>().set<InputState>().set<Invincibility>().build();
        static const int  qP         = World::createQuery(playerMask);
        for (Entity e = Entity::firstQ(qP); !e.eofQ(qP); e.nextQ(qP)) {
            e.get<InputState>().shoot     = true;
            e.get<InputState>().shotFired = false;
            e.get<Invincibility>().frames = 999999;
        }

        // ── Prevent game-over / won flags from ending the benchmark ──
        static const Mask gsMask = MaskBuilder().set<GameStatus>().build();
        static const int  qGS    = World::createQuery(gsMask);
        for (Entity e = Entity::firstQ(qGS); !e.eofQ(qGS); e.nextQ(qGS)) {
            auto& gs = e.get<GameStatus>();
            gs.gameOver = false;
            gs.won      = false;
        }

        // ── Respawn enemies when count drops below threshold ──
        static const Mask eMask = MaskBuilder().set<EnemyTag>().build();
        static const int  qE    = World::createQuery(eMask);
        int enemyCount = 0;
        for (Entity e = Entity::firstQ(qE); !e.eofQ(qE); e.nextQ(qE)) enemyCount++;
        if (enemyCount < BENCH_ENEMY_MIN) {
            spawn_bench_enemies(BENCH_ENEMY_ROWS * BENCH_ENEMY_COLS - enemyCount);
            _wave_enemy_count = BENCH_ENEMY_ROWS * BENCH_ENEMY_COLS;
        }

        // ── Also occasionally respawn shelters for more collision events ──
        _bench_frame++;
        if (_bench_frame % 600 == 0) {  // every ~10 seconds
            static const Mask shMask = MaskBuilder().set<Shelter>().build();
            for (Entity e = Entity::first(); !e.eof(); e.next())
                if (e.test(shMask)) e.destroy();
            static constexpr float SHELTER_W  = TILE * 2.f - 4.f;
            static constexpr float SHELTER_H  = TILE - 4.f;
            static constexpr float SHELTER_XS[] = { 190.f, 512.f, 834.f };
            for (float sx : SHELTER_XS)
                Entity::create().addAll(
                    Transform{{sx, WIN_H - TILE / 2.f - 3 * TILE}, 0.f},
                    Drawable{{0, 0, 0, 0}, {SHELTER_W, SHELTER_H}},
                    Shelter{}, Health{50});
        }
    }

    void Game::draw_benchmark_hud() const
    {
        const int bruteForce  = (World::maxId() + 1) * g_query_loop_starts;
        const int queryActual = g_entity_checks;
        const int speedup     = (queryActual > 0) ? bruteForce / queryActual : 0;
        const int entities    = World::maxId() + 1;

        // Track peak speedup across the entire benchmark session
        if (speedup > _bench_max_speedup) _bench_max_speedup = speedup;

        // ── Semi-transparent card in the top-right corner ──
        // Card: 390 × 148 px, 20px from top-right edge
        const float cx = (float)WIN_W - 410.f;
        const float cy = 14.f;
        const float cw = 390.f, ch = 148.f;

        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 200);
        SDL_FRect bg = {cx, cy, cw, ch};
        SDL_RenderFillRect(ren, &bg);
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

        // Border
        SDL_SetRenderDrawColor(ren, 100, 100, 200, 255);
        SDL_FRect border = {cx - 1.f, cy - 1.f, cw + 2.f, ch + 2.f};
        SDL_RenderFillRect(ren, &border);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 200);
        SDL_RenderFillRect(ren, &bg);

        char buf[80];

        // Title
        SDL_SetRenderScale(ren, 1.5f, 1.5f);
        SDL_SetRenderDrawColor(ren, 180, 180, 255, 255);
        SDL_RenderDebugText(ren, (cx + 6.f) / 1.5f, (cy + 5.f) / 1.5f, "ECS ENTITY-QUERY BENCHMARK");
        SDL_SetRenderScale(ren, 1.f, 1.f);

        auto divider = [&](float y) {
            SDL_SetRenderDrawColor(ren, 60, 60, 120, 255);
            SDL_FRect d = {cx + 4.f, y, cw - 8.f, 1.f};
            SDL_RenderFillRect(ren, &d);
        };

        divider(cy + 26.f);

        // Green: query checks  (scale 1.4 → 32 chars × 8 × 1.4 = 358px  < 382px usable)
        SDL_SetRenderScale(ren, 1.4f, 1.4f);
        SDL_SetRenderDrawColor(ren, 80, 220, 80, 255);
        SDL_snprintf(buf, sizeof(buf), "EntityQuery:  %5d checks/frame", queryActual);
        SDL_RenderDebugText(ren, (cx + 6.f) / 1.4f, (cy + 32.f) / 1.4f, buf);

        // Red: brute-force
        SDL_SetRenderDrawColor(ren, 230, 70, 70, 255);
        SDL_snprintf(buf, sizeof(buf), "Brute Force:  %5d checks/frame", bruteForce);
        SDL_RenderDebugText(ren, (cx + 6.f) / 1.4f, (cy + 50.f) / 1.4f, buf);
        SDL_SetRenderScale(ren, 1.f, 1.f);

        divider(cy + 68.f);

        // Yellow: current speedup + entity count
        SDL_SetRenderScale(ren, 1.8f, 1.8f);
        SDL_SetRenderDrawColor(ren, 255, 215, 40, 255);
        SDL_snprintf(buf, sizeof(buf), "SPEEDUP: %dx", speedup);
        SDL_RenderDebugText(ren, (cx + 6.f) / 1.8f, (cy + 75.f) / 1.8f, buf);
        SDL_SetRenderDrawColor(ren, 180, 220, 255, 255);
        SDL_snprintf(buf, sizeof(buf), "Entities: %d", entities);
        SDL_RenderDebugText(ren, (cx + 190.f) / 1.8f, (cy + 75.f) / 1.8f, buf);
        SDL_SetRenderScale(ren, 1.f, 1.f);

        divider(cy + 98.f);

        // Cyan: session peak
        SDL_SetRenderScale(ren, 1.8f, 1.8f);
        SDL_SetRenderDrawColor(ren, 0, 230, 220, 255);
        SDL_snprintf(buf, sizeof(buf), "SESSION PEAK: %dx", _bench_max_speedup);
        SDL_RenderDebugText(ren, (cx + 6.f) / 1.8f, (cy + 105.f) / 1.8f, buf);
        SDL_SetRenderScale(ren, 1.f, 1.f);

        // Hint line at very bottom
        SDL_SetRenderScale(ren, 1.2f, 1.2f);
        SDL_SetRenderDrawColor(ren, 80, 80, 120, 255);
        SDL_RenderDebugText(ren, (cx + 6.f) / 1.2f, (cy + ch - 16.f) / 1.2f, "ESC = back to menu  |  H = toggle HUD");
        SDL_SetRenderScale(ren, 1.f, 1.f);

        // ── "BENCHMARK MODE" badge at top-left ──
        SDL_SetRenderScale(ren, 2.2f, 2.2f);
        SDL_SetRenderDrawColor(ren, 255, 160, 0, 255);
        SDL_RenderDebugText(ren, 4.f, 2.f, "BENCHMARK MODE");
        SDL_SetRenderScale(ren, 1.f, 1.f);
    }

} // namespace ci
