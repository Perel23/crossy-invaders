#include "Game.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <vector>
#include <unistd.h>
using namespace std;
using namespace bagel;

namespace ci
{
    static constexpr int   ENEMY_ROWS      = 2;
    static constexpr int   ENEMY_COLS      = 8;
    static constexpr int   BOSS_HP         = 40;
    static constexpr int   ENEMY_START_COL = (Game::COLS - ENEMY_COLS) / 2;
    static constexpr int   ENEMY_TOP_LANE  = Game::LANES - 2;
    static constexpr float ISO_SCALE       = 1.0f;   // flat top-down: no perspective squash
    static constexpr float HOP_HEIGHT      = 18.f;   // max pixel rise during player hop
    static constexpr float TILT            = 0.0f;   // flat top-down: no diagonal shear

    // Unified world-speed multiplier: Easy = 0.82×, Normal = 1.0×, Hard = 1.18×
    // Applied to enemy movement, shooting, hazard movement, and bullet speed.
    static float diff_base_scale(int d) { return d == 0 ? 0.82f : d == 2 ? 1.18f : 1.0f; }

    // ─────────────────────────────── Constructor / Destructor ────────────────────
    Game::Game()
    {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) { cout << SDL_GetError() << endl; return; }
        // Change CWD to the binary's directory so "res/..." paths work regardless of
        // where the binary was launched from.
        { const char* bp = SDL_GetBasePath(); if (bp) { chdir(bp); } }
        if (!SDL_CreateWindowAndRenderer("Crossy Invaders", WIN_W, WIN_H, 0, &win, &ren)) {
            cout << SDL_GetError() << endl; return;
        }
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        std::srand(static_cast<unsigned>(SDL_GetTicks()));

        // Load preview / persistent textures
        {
            auto loadTex = [&](const char* path) -> SDL_Texture* {
                SDL_Surface* s = IMG_Load(path);
                if (!s) return nullptr;
                SDL_Texture* t = SDL_CreateTextureFromSurface(ren, s);
                SDL_DestroySurface(s);
                return t;
            };
            bg_texture = nullptr; // procedural background — no image needed
            static const char* char_files[NUM_CHARS] = {
                "res/trump_pixel.png", "res/bibi_pixel.png", "res/bengvir_pixel.png",
                "res/zelensky_pixel.png", "res/putin_pixel.png", "res/obama_pixel.png",
                "res/eminem_pixel.png", "res/madonna_pixel.png", "res/michaeljackson_pixel.png",
                "res/sara_pixel.png", "res/stalin_pixel.png", "res/yoamashit_pixel.png"
            };
            for (int i = 0; i < NUM_CHARS; i++)
                char_select_tex[i] = loadTex(char_files[i]);
            shelter_texture       = loadTex("res/miklat.png");
            boss_texture          = loadTex("res/khamenai.png");
            hearts_texture        = loadTex("res/hearts_spreadsheet.png");
            iron_dome_texture     = loadTex("res/iron_dome.png");
            iron_dome_icon_texture= loadTex("res/iron_dome_icon.png");
        }

        // Audio
        SDL_AudioSpec spec{ SDL_AUDIO_S16, 1, 44100 };
        audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                                 &spec, nullptr, nullptr);
        if (audio_stream) SDL_ResumeAudioStreamDevice(audio_stream);

        Entity::create().add(SelectState{0, false, 1}); // difficulty default = Normal
        load_high_score();
    }

    Game::~Game()
    {
        if (bg_texture)             SDL_DestroyTexture(bg_texture);
        if (player_texture)         SDL_DestroyTexture(player_texture);
        if (enemy_texture)          SDL_DestroyTexture(enemy_texture);
        if (boss_texture)           SDL_DestroyTexture(boss_texture);
        if (shelter_texture)        SDL_DestroyTexture(shelter_texture);
        if (hearts_texture)         SDL_DestroyTexture(hearts_texture);
        if (iron_dome_texture)      SDL_DestroyTexture(iron_dome_texture);
        if (iron_dome_icon_texture) SDL_DestroyTexture(iron_dome_icon_texture);
        for (int i = 0; i < NUM_CHARS; i++)
            if (char_select_tex[i]) SDL_DestroyTexture(char_select_tex[i]);
        for (int i = 0; i < 4; i++)
            if (haz_textures[i]) SDL_DestroyTexture(haz_textures[i]);
        if (audio_stream) SDL_DestroyAudioStream(audio_stream);
        if (ren) SDL_DestroyRenderer(ren);
        if (win) SDL_DestroyWindow(win);
        SDL_Quit();
    }

    // ──────────────────────────────── Persistence ────────────────────────────────
    void Game::load_high_score()
    {
        FILE* f = fopen("highscore.dat", "r");
        if (f) { fscanf(f, "%d", &_high_score); fclose(f); }
    }

    void Game::save_high_score() const
    {
        FILE* f = fopen("highscore.dat", "w");
        if (f) { fprintf(f, "%d\n", _high_score); fclose(f); }
    }

    // ───────────────────────────────── Spawn ─────────────────────────────────────
    static constexpr float HAZARD_W = Game::TILE * 2.f - 4.f;
    static constexpr float HAZARD_H = Game::TILE       - 4.f;

    void Game::spawn_entities() const
    {
        const char* hazard_files[] = {
            "res/usa_presidential_limousine.png",
            "res/wing_of_zion.png",
            "res/tank_merkava_4.png",
            "res/iranian_karrar_tank.png"
        };
        for (int i = 0; i < 4; i++) {
            if (haz_textures[i]) { SDL_DestroyTexture(haz_textures[i]); haz_textures[i] = nullptr; }
            SDL_Surface* surf = IMG_Load(hazard_files[i]);
            if (surf) { haz_textures[i] = SDL_CreateTextureFromSurface(ren, surf); SDL_DestroySurface(surf); }
        }

        // Read selected character + difficulty from SelectState entity
        static const Mask ssMask = MaskBuilder().set<SelectState>().build();
        int selected = 0;
        for (Entity e = Entity::first(); !e.eof(); e.next())
            if (e.test(ssMask)) {
                selected    = e.get<SelectState>().selected;
                _difficulty = e.get<SelectState>().difficulty;
                break;
            }

        static const char* char_files[NUM_CHARS] = {
            "res/trump_pixel.png", "res/bibi_pixel.png", "res/bengvir_pixel.png",
            "res/zelensky_pixel.png", "res/putin_pixel.png", "res/obama_pixel.png",
            "res/eminem_pixel.png", "res/madonna_pixel.png", "res/michaeljackson_pixel.png",
            "res/sara_pixel.png", "res/stalin_pixel.png", "res/yoamashit_pixel.png"
        };
        if (player_texture) { SDL_DestroyTexture(player_texture); player_texture = nullptr; }
        SDL_Surface* ps = IMG_Load(char_files[selected < NUM_CHARS ? selected : 0]);
        if (ps) { player_texture = SDL_CreateTextureFromSurface(ren, ps); SDL_DestroySurface(ps); }

        if (enemy_texture) { SDL_DestroyTexture(enemy_texture); enemy_texture = nullptr; }
        SDL_Surface* es = IMG_Load("res/iranian_regime_pixel.png");
        if (es) { enemy_texture = SDL_CreateTextureFromSurface(ren, es); SDL_DestroySurface(es); }

        // ── Player ──
        // Level 4: player starts near the top; levels 1-3: bottom
        const int startLane = (_current_level == 4) ? 9 : 0;
        const int startCol  = COLS / 2;
        Entity::create().addAll(
            Transform{{TILE / 2.f + startCol * TILE, WIN_H - TILE / 2.f - startLane * TILE}, 0.f},
            Drawable{{0, 0, 0, 0}, {TILE - 4.f, TILE - 4.f}},
            LanePos{startLane, startCol},
            PlayerTag{},
            InputState{false,false,false,false,false,false,/*shotFired=*/true,false,false,false,false,false},
            Health{3},
            Shield{0.f, SHIELD_CHARGES},
            Invincibility{0},
            DamageFlash{0},
            ShieldFlash{0},
            RapidFire{0, 0},
            DashState{0},
            SpreadShot{0},
            HopState{0, 10, 0.f},
            BreatheState{0.f, 0.05f, 2.5f},
            BlinkPhase{true, 0, 8});

        // ── Enemies ──
        if (_current_level == 3) {
            Entity::create().addAll(
                Transform{{TILE / 2.f + (COLS / 2) * TILE, WIN_H - TILE / 2.f - ENEMY_TOP_LANE * TILE}, 0.f},
                Drawable{{0, 0, 0, 0}, {TILE * 3.f, TILE * 3.f}},
                LanePos{ENEMY_TOP_LANE, COLS / 2},
                EnemyTag{}, BossTag{}, Health{BOSS_HP},
                BreatheState{0.f, 0.04f, 3.f});
        } else if (_current_level == 4) {
            for (int row = 0; row < ENEMY_ROWS; row++) {
                for (int col = 0; col < ENEMY_COLS; col++) {
                    const int lane = 2 + row;
                    const int ecol = ENEMY_START_COL + col;
                    // Unique starting phase per enemy so they breathe out of sync
                    const float phase = (col * 13 + row * 7) % 100 * 0.063f;
                    Entity::create().addAll(
                        Transform{{TILE / 2.f + ecol * TILE, WIN_H - TILE / 2.f - lane * TILE}, 0.f},
                        Drawable{{0, 0, 0, 0}, {TILE - 4.f, TILE - 4.f}},
                        LanePos{lane, ecol}, EnemyTag{}, Health{3},
                        BreatheState{phase, 0.05f, 2.f});
                }
            }
        } else {
            const int rows = ENEMY_ROWS + (_current_level - 1);
            for (int row = 0; row < rows; row++) {
                for (int col = 0; col < ENEMY_COLS; col++) {
                    const int lane = ENEMY_TOP_LANE - row;
                    const int ecol = ENEMY_START_COL + col;
                    const float phase = (col * 13 + row * 7) % 100 * 0.063f;
                    if (_current_level == 2) {
                        Entity::create().addAll(
                            Transform{{TILE / 2.f + ecol * TILE, WIN_H - TILE / 2.f - lane * TILE}, 0.f},
                            Drawable{{0, 0, 0, 0}, {TILE - 4.f, TILE - 4.f}},
                            LanePos{lane, ecol}, EnemyTag{}, Health{2},
                            IndividualMove{SDL_GetTicks() + (Uint64)(std::rand() % 500)},
                            BreatheState{phase, 0.05f, 2.f});
                    } else {
                        Entity::create().addAll(
                            Transform{{TILE / 2.f + ecol * TILE, WIN_H - TILE / 2.f - lane * TILE}, 0.f},
                            Drawable{{0, 0, 0, 0}, {TILE - 4.f, TILE - 4.f}},
                            LanePos{lane, ecol}, EnemyTag{}, Health{1},
                            BreatheState{phase, 0.05f, 2.f});
                    }
                }
            }
        }

        // ── Shelters ──
        static constexpr float SHELTER_W = TILE * 2.f - 4.f;
        static constexpr float SHELTER_H = TILE - 4.f;
        static constexpr float SHELTER_Y = WIN_H - TILE / 2.f - 3 * TILE;
        static constexpr float SHELTER_XS[] = { 190.f, 512.f, 834.f };
        for (float sx : SHELTER_XS)
            Entity::create().addAll(
                Transform{{sx, SHELTER_Y}, 0.f},
                Drawable{{0, 0, 0, 0}, {SHELTER_W, SHELTER_H}},
                Shelter{}, Health{5});

        // ── Hazards (Level 1 / USA) ──
        // Pixel analysis of background.png confirmed:
        //   Lane 5 → image rows 1422-1520 (main highway)
        //   Lane 8 → image rows 1265-1299 (secondary road)
        const float base_speed = 2.5f + (_current_level - 1) * 0.6f;
        struct HazardDef { int lane; float dx; int tex; };
        const HazardDef hdefs[] = { {5, base_speed, 0}, {8, -base_speed, 1} };
        for (const auto& def : hdefs) {
            const float y = WIN_H - TILE / 2.f - def.lane * TILE;
            for (int k = 0; k < 2; k++) {
                const float initPhase = (float)(std::rand() % 628) * 0.01f; // 0..2π
                Entity::create().addAll(
                    Transform{{(float)(std::rand() % WIN_W), y}, 0.f},
                    Drawable{{0, 0, 0, 0}, {HAZARD_W, HAZARD_H}},
                    Velocity{def.dx, 0.f}, Hazard{}, HazardVisual{def.tex},
                    BreatheState{initPhase, 0.15f, 1.5f});
            }
        }

        // ── Global state entities ──
        Entity::create().addAll(Transform{{0.f, 0.f}, 0.f}, FormationState{1, 0, 0});
        const float baseScale = diff_base_scale(_difficulty);
        Entity::create().addAll(
            Transform{{0.f, 0.f}, 0.f},
            GameStatus{false, false, false, 0, 0, 0, SDL_GetTicks(), 0},
            ScreenShake{0},
            ComboState{0, 0, 1},
            SlowMo{0, 0},
            SpeedScale{baseScale, baseScale});

        // Camera
        _camera_scroll      = 0.f;
        _level_start_scroll = 0.f;
        _camera_grace       = (_current_level == 4) ? 999999 : 180; // no auto-scroll in level 4

        // Enemy count baseline for difficulty ramp
        _wave_enemy_count = 0;
        static const Mask eMask = MaskBuilder().set<EnemyTag>().build();
        for (Entity e = Entity::first(); !e.eof(); e.next())
            if (e.test(eMask)) _wave_enemy_count++;
    }

    void Game::reset() const
    {
        static const Mask anyMask = MaskBuilder().set<Transform>().build();
        for (Entity e = Entity::first(); !e.eof(); e.next())
            if (e.test(anyMask)) e.destroy();

        if (player_texture) { SDL_DestroyTexture(player_texture); player_texture = nullptr; }
        if (enemy_texture)  { SDL_DestroyTexture(enemy_texture);  enemy_texture  = nullptr; }

        static const Mask ssMask = MaskBuilder().set<SelectState>().build();
        for (Entity e = Entity::first(); !e.eof(); e.next())
            if (e.test(ssMask)) { e.get<SelectState>().moved = false; break; }

        _score = 0; _total_kills = 0; _camera_scroll = 0.f; _camera_grace = 180;
        _select_last_char = -1;
        _state = GameState::Select;
    }

    void Game::clear_game_entities() const
    {
        static const Mask anyMask = MaskBuilder().set<Transform>().build();
        for (Entity e = Entity::first(); !e.eof(); e.next())
            if (e.test(anyMask)) e.destroy();
        if (player_texture) { SDL_DestroyTexture(player_texture); player_texture = nullptr; }
        if (enemy_texture)  { SDL_DestroyTexture(enemy_texture);  enemy_texture  = nullptr; }
        for (int i = 0; i < 4; i++)
            if (haz_textures[i]) { SDL_DestroyTexture(haz_textures[i]); haz_textures[i] = nullptr; }
    }

    void Game::clear_enemies_only() const
    {
        static const Mask enemyMask  = MaskBuilder().set<EnemyTag>().build();
        static const Mask bulletMask = MaskBuilder().set<BulletTag>().build();
        static const Mask explMask   = MaskBuilder().set<Explosion>().build();
        static const Mask pickupMask = MaskBuilder().set<Pickup>().build();
        static const Mask ftMask     = MaskBuilder().set<FloatingText>().build();
        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (e.test(enemyMask) || e.test(bulletMask) ||
                e.test(explMask)  || e.test(pickupMask) || e.test(ftMask))
                e.destroy();
        }
        if (enemy_texture) { SDL_DestroyTexture(enemy_texture); enemy_texture = nullptr; }
    }

    void Game::spawn_enemy_wave() const
    {
        if (!enemy_texture) {
            SDL_Surface* s = IMG_Load("res/iranian_regime_pixel.png");
            if (s) { enemy_texture = SDL_CreateTextureFromSurface(ren, s); SDL_DestroySurface(s); }
        }

        if (_current_level == 4) {
            // Level 4 role reversal: enemies at bottom, player relocated to top
            static const Mask playerMask = MaskBuilder().set<PlayerTag>().set<LanePos>().set<Transform>().build();
            for (Entity e = Entity::first(); !e.eof(); e.next()) {
                if (!e.test(playerMask)) continue;
                e.get<LanePos>().lane  = 9;
                e.get<LanePos>().col   = COLS / 2;
                e.get<Transform>().p.x = TILE / 2.f + (COLS / 2) * TILE;
                e.get<Transform>().p.y = WIN_H - TILE / 2.f - 9 * TILE;
                break;
            }
            for (int row = 0; row < ENEMY_ROWS; row++) {
                for (int col = 0; col < ENEMY_COLS; col++) {
                    const int lane = 2 + row;
                    const int ecol = ENEMY_START_COL + col;
                    Entity::create().addAll(
                        Transform{{TILE / 2.f + ecol * TILE, WIN_H - TILE / 2.f - lane * TILE}, 0.f},
                        Drawable{{0, 0, 0, 0}, {TILE - 4.f, TILE - 4.f}},
                        LanePos{lane, ecol}, EnemyTag{}, Health{3});
                }
            }
            _camera_scroll = 0.f;
            _camera_grace  = 999999;
        } else {
            const int enemy_base_lane = ENEMY_TOP_LANE + static_cast<int>(_camera_scroll / TILE) + 4;
            if (_current_level == 3) {
                const int lane = enemy_base_lane;
                Entity::create().addAll(
                    Transform{{TILE / 2.f + (COLS / 2) * TILE, WIN_H - TILE / 2.f - lane * TILE}, 0.f},
                    Drawable{{0, 0, 0, 0}, {TILE * 3.f, TILE * 3.f}},
                    LanePos{lane, COLS / 2}, EnemyTag{}, BossTag{}, Health{BOSS_HP});
            } else {
                const int rows = ENEMY_ROWS + (_current_level - 1);
                for (int row = 0; row < rows; row++) {
                    for (int col = 0; col < ENEMY_COLS; col++) {
                        const int lane = enemy_base_lane - row;
                        const int ecol = ENEMY_START_COL + col;
                        Entity::create().addAll(
                            Transform{{TILE / 2.f + ecol * TILE, WIN_H - TILE / 2.f - lane * TILE}, 0.f},
                            Drawable{{0, 0, 0, 0}, {TILE - 4.f, TILE - 4.f}},
                            LanePos{lane, ecol}, EnemyTag{}, Health{_current_level},
                            IndividualMove{SDL_GetTicks() + (Uint64)(std::rand() % 500)});
                    }
                }
            }
        }

        // Refresh enemy count
        _wave_enemy_count = 0;
        {
            static const Mask eMask = MaskBuilder().set<EnemyTag>().build();
            for (Entity e = Entity::first(); !e.eof(); e.next())
                if (e.test(eMask)) _wave_enemy_count++;
        }

        // Reset formation state
        {
            static const Mask fsMask = MaskBuilder().set<FormationState>().build();
            for (Entity e = Entity::first(); !e.eof(); e.next()) {
                if (!e.test(fsMask)) continue;
                auto& fs = e.get<FormationState>();
                fs.dir = 1; fs.moveTimer = SDL_GetTicks(); fs.shootTimer = SDL_GetTicks();
                break;
            }
        }

        // Reset wave stats + clear won flag
        {
            static const Mask gsMask = MaskBuilder().set<GameStatus>().build();
            for (Entity e = Entity::first(); !e.eof(); e.next()) {
                if (!e.test(gsMask)) continue;
                auto& gs = e.get<GameStatus>();
                gs.won = false;
                gs.kills = 0; gs.shots = 0; gs.hits = 0;
                gs.waveStartTime = SDL_GetTicks(); gs.waveEndTime = 0;
                break;
            }
        }

        // Respawn shelters for levels 2 and 3 at the camera-adjusted position
        // (level 1 shelters are spawned in spawn_entities; level 4 has no shelters).
        if (_current_level == 2 || _current_level == 3) {
            static const Mask shMask = MaskBuilder().set<Shelter>().build();
            for (Entity e = Entity::first(); !e.eof(); e.next())
                if (e.test(shMask)) e.destroy();

            static constexpr float SHELTER_W  = TILE * 2.f - 4.f;
            static constexpr float SHELTER_H  = TILE - 4.f;
            static constexpr float SHELTER_XS[] = { 190.f, 512.f, 834.f };
            // Keep shelters at lane 3 relative to the current screen bottom.
            const float shelter_y = WIN_H - TILE / 2.f - 3 * TILE - _camera_scroll;
            for (float sx : SHELTER_XS)
                Entity::create().addAll(
                    Transform{{sx, shelter_y}, 0.f},
                    Drawable{{0, 0, 0, 0}, {SHELTER_W, SHELTER_H}},
                    Shelter{}, Health{5});
        }
    }

    // ──────────────────────────────── Systems ────────────────────────────────────
    void Game::explosion_system() const
    {
        static const Mask mask = MaskBuilder().set<Explosion>().build();
        for (Entity e = Entity::first(); !e.eof(); e.next())
            if (e.test(mask) && --e.get<Explosion>().frames <= 0) e.destroy();
    }

    void Game::pickup_system() const
    {
        static const Mask pickupMask = MaskBuilder().set<Pickup>().set<Transform>().set<Drawable>().build();
        static const Mask playerMask = MaskBuilder().set<PlayerTag>().set<Transform>().set<Drawable>().set<Health>().build();

        auto overlaps = [](SDL_FPoint p1, SDL_FPoint s1, SDL_FPoint p2, SDL_FPoint s2) {
            return std::abs(p1.x - p2.x) < (s1.x + s2.x) * 0.5f &&
                   std::abs(p1.y - p2.y) < (s1.y + s2.y) * 0.5f;
        };

        for (Entity pk = Entity::first(); !pk.eof(); pk.next()) {
            if (!pk.test(pickupMask)) continue;
            const SDL_FPoint pp = pk.get<Transform>().p;
            const SDL_FPoint ps = pk.get<Drawable>().size;
            bool collected = false;
            for (Entity pl = Entity::first(); !pl.eof(); pl.next()) {
                if (!pl.test(playerMask)) continue;
                if (!overlaps(pp, ps, pl.get<Transform>().p, pl.get<Drawable>().size)) continue;
                const int type = pk.get<Pickup>().type;
                collected = true;
                if (type == 0) {
                    pl.get<RapidFire>().frames   = 300;
                    pl.get<RapidFire>().cooldown  = 0;
                } else if (type == 1) {
                    auto& sh = pl.get<Shield>();
                    sh.charges = std::min(sh.charges + 1, 3);
                } else if (type == 2) {
                    pl.get<Health>().hp = std::min(pl.get<Health>().hp + 1, 5);
                } else { // type == 3: spread shot
                    if (pl.has<SpreadShot>()) pl.get<SpreadShot>().frames = 300;
                }
                Entity::create().add(SoundEvent{4});
                break;
            }
            if (collected) pk.destroy();
        }
    }

    void Game::combo_system() const
    {
        static const Mask mask = MaskBuilder().set<ComboState>().build();
        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (!e.test(mask)) continue;
            auto& cs = e.get<ComboState>();
            if (cs.timer > 0) cs.timer--;
            return;
        }
    }

    void Game::floating_text_system() const
    {
        static const Mask mask = MaskBuilder().set<FloatingText>().set<Transform>().build();
        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (!e.test(mask)) continue;
            e.get<Transform>().p.y -= 1.2f;   // float upward in world space
            if (--e.get<FloatingText>().frames <= 0) e.destroy();
        }
    }

    void Game::sound_system() const
    {
        static const Mask mask = MaskBuilder().set<SoundEvent>().build();
        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (!e.test(mask)) continue;
            play_sfx(e.get<SoundEvent>().type);
            e.destroy();
        }
    }

    void Game::hop_system() const
    {
        static const Mask mask = MaskBuilder().set<HopState>().build();
        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (!e.test(mask)) continue;
            if (e.get<HopState>().frames > 0) e.get<HopState>().frames--;
        }
    }

    void Game::animate_system() const
    {
        // Increments BreatheState.phase on every entity that has one.
        // draw_system reads phase to compute a sin-based Y offset — no SDL_GetTicks()
        // or wall-clock reads in the render path; all animation state lives in ECS.
        static const Mask mask = MaskBuilder().set<BreatheState>().build();
        constexpr float TWO_PI = 6.28318530f;
        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (!e.test(mask)) continue;
            auto& bs = e.get<BreatheState>();
            bs.phase += bs.speed;
            if (bs.phase >= TWO_PI) bs.phase -= TWO_PI; // keep in [0, 2π) to avoid float drift
        }
    }

    void Game::blink_system() const
    {
        static const Mask mask = MaskBuilder().set<BlinkPhase>().build();
        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (!e.test(mask)) continue;
            auto& bp = e.get<BlinkPhase>();
            if (++bp.counter >= bp.period) {
                bp.counter  = 0;
                bp.visible  = !bp.visible;
            }
        }
    }

    void Game::map_screen_system() const
    {
        static const Mask mask = MaskBuilder().set<MapScreen>().build();
        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (!e.test(mask)) continue;
            auto& ms = e.get<MapScreen>();
            // Advance plane over first 180 frames (~3 s), then it rests at destination
            if (ms.planeT < 1.f)
                ms.planeT = std::min(1.f, ms.planeT + 1.f / 180.f);
            if (--ms.framesLeft <= 0) {
                e.destroy();
                // Now start the level splash and make the player invincible through it
                const int splashFrames = (_current_level == 4 ? 300 : 150);
                Entity::create().add(LevelSplash{splashFrames});
                static const Mask plInvMask = MaskBuilder()
                    .set<PlayerTag>().set<Invincibility>().build();
                for (Entity p = Entity::first(); !p.eof(); p.next()) {
                    if (!p.test(plInvMask)) continue;
                    auto& inv = p.get<Invincibility>();
                    if (inv.frames < splashFrames) inv.frames = splashFrames;
                    break;
                }
                _state = GameState::Playing;
            }
            break;
        }
    }

    // ──────────────────────────────────────────────────────────────────────────────
    void Game::draw_map_screen() const
    {
        // Dark background
        SDL_SetRenderDrawColor(ren, 12, 20, 40, 255);
        SDL_FRect bg = {0, 0, (float)WIN_W, (float)WIN_H};
        SDL_RenderFillRect(ren, &bg);

        // Read selected character and MapScreen state
        static const Mask ssMask = MaskBuilder().set<SelectState>().build();
        int selected = 0;
        for (Entity e = Entity::first(); !e.eof(); e.next())
            if (e.test(ssMask)) { selected = e.get<SelectState>().selected; break; }

        static const Mask msMask = MaskBuilder().set<MapScreen>().build();
        float planeT = 1.f; int fromIdx = 0, toIdx = 0;
        for (Entity e = Entity::first(); !e.eof(); e.next())
            if (e.test(msMask)) {
                const auto& ms = e.get<MapScreen>();
                planeT = ms.planeT; fromIdx = ms.fromIdx; toIdx = ms.toIdx;
                break;
            }

        struct WayPoint { float x, y; const char* name; };
        const WayPoint trump_route[] = {
            {120.f, 390.f, "WASHINGTON"},
            {350.f, 270.f, "LONDON"},
            {640.f, 340.f, "DUBAI"},
            {890.f, 360.f, "TEHRAN"},
        };
        const WayPoint bibi_route[] = {
            {130.f, 410.f, "JERUSALEM"},
            {380.f, 385.f, "AMMAN"},
            {630.f, 360.f, "BAGHDAD"},
            {880.f, 345.f, "TEHRAN"},
        };
        const WayPoint* route    = (selected == 0) ? trump_route : bibi_route;
        const char*     charName = (selected == 0) ? "TRUMP" : "BIBI";

        // Helper: arc Y offset — sine curve so plane rises and falls between cities
        auto arcY = [](float t) { return -std::sin(t * 3.14159f) * 90.f; };

        // Title
        SDL_SetRenderScale(ren, 3.f, 3.f);
        SDL_SetRenderDrawColor(ren, 255, 220, 80, 255);
        SDL_RenderDebugText(ren, (WIN_W / 2.f - 36.f) / 3.f, 38.f / 3.f, charName);
        SDL_SetRenderScale(ren, 1.5f, 1.5f);
        SDL_SetRenderDrawColor(ren, 140, 155, 185, 255);
        SDL_RenderDebugText(ren, (WIN_W / 2.f - 100.f) / 1.5f, 72.f / 1.5f, "MISSION PROGRESS");
        SDL_SetRenderScale(ren, 1.f, 1.f);

        // Dim base route lines (all segments)
        SDL_SetRenderDrawColor(ren, 45, 60, 90, 255);
        for (int i = 0; i < 3; i++)
            SDL_RenderLine(ren, (int)route[i].x, (int)route[i].y,
                                (int)route[i+1].x, (int)route[i+1].y);

        // Gold lines for already-completed segments
        SDL_SetRenderDrawColor(ren, 180, 140, 50, 255);
        for (int i = 0; i < fromIdx; i++)
            SDL_RenderLine(ren, (int)route[i].x, (int)route[i].y,
                                (int)route[i+1].x, (int)route[i+1].y);

        // Draw the plane arc flight path (dashed dots between fromIdx and toIdx cities)
        if (fromIdx != toIdx) {
            const float fx = route[fromIdx].x, fy = route[fromIdx].y;
            const float tx = route[toIdx].x,   ty = route[toIdx].y;
            SDL_SetRenderDrawColor(ren, 100, 150, 220, 120);
            const int steps = 40;
            for (int s = 0; s <= steps; s++) {
                const float st = (float)s / steps;
                const float px = fx + (tx - fx) * st;
                const float py = fy + (ty - fy) * st + arcY(st);
                SDL_FRect dot = {px - 1.f, py - 1.f, 3.f, 3.f};
                if (s % 2 == 0) SDL_RenderFillRect(ren, &dot); // dashed
            }

            // Plane trail (solid dots from 0 to planeT)
            SDL_SetRenderDrawColor(ren, 120, 190, 255, 255);
            const int trailSteps = (int)(planeT * steps);
            for (int s = 0; s <= trailSteps; s++) {
                const float st = (float)s / steps;
                const float px = fx + (tx - fx) * st;
                const float py = fy + (ty - fy) * st + arcY(st);
                SDL_FRect dot = {px - 2.f, py - 2.f, 4.f, 4.f};
                SDL_RenderFillRect(ren, &dot);
            }

            // Plane body at current position
            const float px = fx + (tx - fx) * planeT;
            const float py = fy + (ty - fy) * planeT + arcY(planeT);
            // Angle of travel (approximate with next tiny step)
            const float nt = std::min(planeT + 0.02f, 1.f);
            const float nx = fx + (tx - fx) * nt;
            const float ny = fy + (ty - fy) * nt + arcY(nt);
            const float angle = std::atan2f(ny - py, nx - px);
            const float ca = std::cosf(angle), sa = std::sinf(angle);
            // Draw body as a small rotated rect (4 vertices via SDL_RenderGeometry)
            constexpr float BW = 18.f, BH = 7.f;
            SDL_Vertex verts[4];
            auto makeVert = [&](float lx, float ly, Uint8 r, Uint8 g, Uint8 b) {
                SDL_Vertex v;
                v.position.x = px + lx * ca - ly * sa;
                v.position.y = py + lx * sa + ly * ca;
                v.color = {r / 255.f, g / 255.f, b / 255.f, 1.f};
                v.tex_coord = {0.f, 0.f};
                return v;
            };
            verts[0] = makeVert(-BW/2, -BH/2, 240, 240, 255);
            verts[1] = makeVert( BW/2, -BH/2, 200, 220, 255);
            verts[2] = makeVert( BW/2,  BH/2, 200, 220, 255);
            verts[3] = makeVert(-BW/2,  BH/2, 240, 240, 255);
            int idx[] = {0,1,2, 0,2,3};
            SDL_RenderGeometry(ren, nullptr, verts, 4, idx, 6);
            // Wing: thin bar perpendicular to travel
            constexpr float WW = 14.f, WH = 4.f;
            SDL_Vertex wing[4];
            wing[0] = makeVert(-WW/2, -BH/2 - WH, 160, 200, 255);
            wing[1] = makeVert( WW/2, -BH/2 - WH, 160, 200, 255);
            wing[2] = makeVert( WW/2,  BH/2 + WH, 160, 200, 255);
            wing[3] = makeVert(-WW/2,  BH/2 + WH, 160, 200, 255);
            SDL_RenderGeometry(ren, nullptr, wing, 4, idx, 6);
        }

        // Waypoint dots
        for (int i = 0; i < 4; i++) {
            const bool visited = (i < toIdx) || (i == toIdx && planeT >= 1.f);
            const bool isCurrent = (i == toIdx);
            const float r = isCurrent ? 13.f : 8.f;

            if (isCurrent && planeT >= 1.f)
                SDL_SetRenderDrawColor(ren, 255, 200, 40, 255);   // gold: arrived
            else if (visited)
                SDL_SetRenderDrawColor(ren, 70, 200, 90, 255);    // green: passed
            else if (i == fromIdx)
                SDL_SetRenderDrawColor(ren, 150, 160, 180, 255);  // light: just left
            else
                SDL_SetRenderDrawColor(ren, 50, 60, 80, 255);     // dark: future

            SDL_FRect dot = {route[i].x - r, route[i].y - r, r * 2.f, r * 2.f};
            SDL_RenderFillRect(ren, &dot);

            // City label
            SDL_SetRenderScale(ren, 1.2f, 1.2f);
            const bool bright = (visited || i == fromIdx);
            SDL_SetRenderDrawColor(ren, bright ? 210 : 90, bright ? 210 : 90, bright ? 215 : 110, 255);
            SDL_RenderDebugText(ren, (route[i].x - 28.f) / 1.2f, (route[i].y + 18.f) / 1.2f, route[i].name);
            SDL_SetRenderScale(ren, 1.f, 1.f);
        }

        // Blinking "ENTER to skip" prompt
        if ((SDL_GetTicks() / 500) % 2 == 0) {
            SDL_SetRenderScale(ren, 1.5f, 1.5f);
            SDL_SetRenderDrawColor(ren, 130, 130, 140, 255);
            SDL_RenderDebugText(ren, (WIN_W / 2.f - 84.f) / 1.5f, (WIN_H - 52.f) / 1.5f, "SPACE to skip");
            SDL_SetRenderScale(ren, 1.f, 1.f);
        }
    }

    // ──────────────────────────────── Audio ──────────────────────────────────────
    void Game::play_sfx(int type) const
    {
        if (!audio_stream) return;
        // Anthems (types 7-14) bypass queue throttle and clear the stream first
        if (type >= 7) {
            SDL_ClearAudioStream(audio_stream);
        } else {
            if (SDL_GetAudioStreamQueued(audio_stream) > 44100 * 150 / 1000 * (int)sizeof(Sint16)) return;
        }

        std::vector<Sint16> buf;
        const int SR = 44100;
        auto tone = [&](float freq, float dur, float vol = 0.28f, float endVol = 0.0f) {
            int n = (int)(SR * dur); buf.resize(n);
            for (int i = 0; i < n; i++) {
                float fade = vol + (endVol - vol) * ((float)i / n);
                buf[i] = (Sint16)(32767 * fade * std::sin(2 * M_PI * freq * (float)i / SR));
            }
        };
        switch (type) {
            case 0: tone(880.f, 0.055f, 0.22f, 0.0f); break;           // shoot
            case 1: tone(520.f, 0.08f,  0.20f, 0.0f); break;           // enemy hit
            case 2: {   // enemy death: sweep down
                int n = (int)(SR * 0.14f); buf.resize(n);
                for (int i = 0; i < n; i++) {
                    float f = 700.f - 550.f * ((float)i / n);
                    float v = 0.26f * (1.f - (float)i / n);
                    buf[i] = (Sint16)(32767 * v * std::sin(2 * M_PI * f * (float)i / SR));
                }
                break;
            }
            case 3: tone(90.f,  0.22f, 0.38f, 0.05f); break;           // player hit
            case 4: {   // pickup / level clear: jingle
                int n = (int)(SR * 0.35f); buf.resize(n);
                const float notes[] = {330.f, 440.f, 550.f, 660.f};
                for (int i = 0; i < n; i++) {
                    int ni = std::min((int)(4.f * i / n), 3);
                    float v = 0.22f * (1.f - (float)i / n);
                    buf[i] = (Sint16)(32767 * v * std::sin(2 * M_PI * notes[ni] * (float)i / SR));
                }
                break;
            }
            case 5: {   // game over: descend
                int n = (int)(SR * 0.9f); buf.resize(n);
                for (int i = 0; i < n; i++) {
                    float f = 280.f - 200.f * ((float)i / n);
                    float v = 0.30f * (1.f - (float)i / n);
                    buf[i] = (Sint16)(32767 * v * std::sin(2 * M_PI * f * (float)i / SR));
                }
                break;
            }
            case 6: {   // bullet-time activation: whoosh
                int n = (int)(SR * 0.18f); buf.resize(n);
                for (int i = 0; i < n; i++) {
                    float f = 200.f + 400.f * ((float)i / n);
                    float v = 0.20f * std::sin(M_PI * (float)i / n);
                    buf[i] = (Sint16)(32767 * v * std::sin(2 * M_PI * f * (float)i / SR));
                }
                break;
            }
            case 7:  case 8:  case 9:  case 10:
            case 11: case 12: case 13: case 14:
            case 15: case 16: case 17: case 18: {
                static const char* wav_files[] = {
                    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                    "res/hatikva.wav",         //  7
                    "res/ssb.wav",             //  8
                    "res/ani_maamin.wav",       //  9
                    "res/ukrainian_anthem.wav", // 10
                    "res/russian_anthem.wav",   // 11
                    "res/slim_shady.wav",       // 12
                    "res/like_a_virgin.wav",    // 13
                    "res/thriller.wav",         // 14
                    "res/ba_ma.wav",            // 15 Sara
                    "res/moscow.wav",           // 16 Stalin
                    "res/ma_sheat_ohevet.wav",  // 17 Yoamashit
                    "res/f_the_police.wav",     // 18 Obama
                };
                SDL_AudioSpec wavSpec{};
                Uint8* wavBuf = nullptr;
                Uint32 wavLen = 0;
                if (SDL_LoadWAV(wav_files[type], &wavSpec, &wavBuf, &wavLen) && wavBuf) {
                    // Auto-convert to stream format (mono S16 44100) so stereo files work too
                    const SDL_AudioSpec streamSpec{ SDL_AUDIO_S16, 1, 44100 };
                    Uint8* convBuf = nullptr;
                    int    convLen = 0;
                    if (SDL_ConvertAudioSamples(&wavSpec, wavBuf, (int)wavLen,
                                               &streamSpec, &convBuf, &convLen) && convBuf) {
                        SDL_PutAudioStreamData(audio_stream, convBuf, convLen);
                        SDL_free(convBuf);
                    }
                    SDL_free(wavBuf);
                }
                return;
            }
            default: return;
        }
        if (!buf.empty())
            SDL_PutAudioStreamData(audio_stream, buf.data(), (int)(buf.size() * sizeof(Sint16)));
    }

    // ──────────────────────────────── Background ─────────────────────────────────
    void Game::draw_background() const
    {
        // ── Procedurally-generated scrolling lane background ──────────────────────
        // Each lane is a coloured rectangle; colours depend on the current level.
        // Camera scroll is applied so the bands move with the player.

        for (int lane = -2; lane < LANES + 35; lane++) {
            const float world_y_top  = WIN_H - (lane + 1) * TILE;
            const float screen_y_top = world_y_top + _camera_scroll;
            if (screen_y_top > (float)WIN_H + TILE) continue;
            if (screen_y_top + TILE < -(float)TILE)  continue;

            const int tl = ((lane % LANES) + LANES) % LANES;
            Uint8 r, g, b;
            if (_current_level == 1) {
                if      (tl <= 1)            { r= 20; g= 70; b= 20; }   // safe zone (grass)
                else if (tl == 4 || tl == 6) { r= 40; g= 40; b= 50; }   // road lanes
                else if (tl == 5)            { r= 28; g= 55; b= 28; }   // road median
                else if (tl >= LANES - 2)    { r= 35; g=  8; b=  8; }   // enemy zone
                else                         { r= 24; g= 55; b= 24; }   // open grass
            } else if (_current_level == 2) {
                if      (tl <= 1)            { r=160; g=120; b= 50; }   // sandy safe zone
                else if (tl == 4 || tl == 6) { r= 90; g= 75; b= 40; }   // dirt road
                else if (tl == 5)            { r=130; g=100; b= 45; }   // dirt median
                else if (tl >= LANES - 2)    { r= 80; g= 25; b= 10; }   // enemy zone
                else                         { r=145; g=110; b= 48; }   // open sand
            } else if (_current_level == 3) {
                if      (tl <= 1)            { r= 25; g= 20; b= 35; }   // dark safe zone
                else if (tl == 4 || tl == 6) { r= 18; g= 15; b= 25; }   // dark road
                else if (tl == 5)            { r= 30; g= 20; b= 40; }   // dark median
                else if (tl >= LANES - 2)    { r= 50; g=  5; b=  5; }   // enemy zone
                else                         { r= 22; g= 18; b= 30; }   // open dark
            } else {
                if      (tl <= 1)            { r=  8; g= 15; b= 35; }   // space safe zone
                else if (tl == 4 || tl == 6) { r= 12; g= 10; b= 20; }   // space road
                else if (tl == 5)            { r= 15; g= 12; b= 28; }   // space median
                else if (tl >= LANES - 2)    { r= 35; g=  5; b= 50; }   // enemy zone (purple)
                else                         { r=  6; g= 10; b= 25; }   // open space
            }

            SDL_SetRenderDrawColor(ren, r, g, b, 255);
            SDL_FRect band = {0.f, screen_y_top, (float)WIN_W, (float)TILE + 1.f};
            SDL_RenderFillRect(ren, &band);
        }

        // ── Road centre-line dashes on road lanes ─────────────────────────────────
        if      (_current_level == 1) SDL_SetRenderDrawColor(ren, 180, 160,  30, 255);
        else if (_current_level == 2) SDL_SetRenderDrawColor(ren, 120, 100,  30, 255);
        else if (_current_level == 3) SDL_SetRenderDrawColor(ren,  60,  40,  80, 255);
        else                          SDL_SetRenderDrawColor(ren,  40,  60, 120, 255);
        for (int lane = -2; lane < LANES + 35; lane++) {
            const int tl = ((lane % LANES) + LANES) % LANES;
            if (tl != 4 && tl != 6) continue;
            const float worldCY  = WIN_H - lane * TILE - TILE / 2.f;
            const float screenCY = worldCY + _camera_scroll;
            if (screenCY < -(float)TILE || screenCY > (float)WIN_H + TILE) continue;
            for (float cx = 0.f; cx < WIN_W; cx += 40.f) {
                SDL_FRect dash = {cx, screenCY - 3.f, 24.f, 6.f};
                SDL_RenderFillRect(ren, &dash);
            }
        }

        // ── Lane divider lines ────────────────────────────────────────────────────
        if      (_current_level == 1) SDL_SetRenderDrawColor(ren,  55,  55,  55, 255);
        else if (_current_level == 2) SDL_SetRenderDrawColor(ren, 100,  80,  40, 255);
        else if (_current_level == 3) SDL_SetRenderDrawColor(ren,  40,  30,  55, 255);
        else                          SDL_SetRenderDrawColor(ren,  20,  30,  60, 255);
        for (int lane = -1; lane <= LANES + 35; lane++) {
            const float worldY  = WIN_H - lane * TILE;
            const float screenY = worldY + _camera_scroll;
            if (screenY < -(float)TILE || screenY > (float)WIN_H + TILE) continue;
            SDL_RenderLine(ren, 0.f, screenY, (float)WIN_W, screenY);
        }

        // ── Level 4: scatter stars in screen space ────────────────────────────────
        if (_current_level == 4) {
            SDL_SetRenderDrawColor(ren, 150, 180, 255, 255);
            for (int i = 0; i < 60; i++) {
                float sx = (float)((i * 137 + 23) % WIN_W);
                float sy = (float)((i *  89 + 47) % WIN_H);
                SDL_FRect star = {sx, sy, 2.f, 2.f};
                SDL_RenderFillRect(ren, &star);
            }
        }
    }

    // ──────────────────────────────── Splash ─────────────────────────────────────
    void Game::splash_system() const
    {
        static const Mask mask = MaskBuilder().set<LevelSplash>().build();
        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (!e.test(mask)) continue;
            if (--e.get<LevelSplash>().framesLeft <= 0) e.destroy();
            return;
        }
    }

    void Game::draw_level_splash() const
    {
        // Read framesLeft to drive phase-based animation
        static const Mask splashMask2 = MaskBuilder().set<LevelSplash>().build();
        int framesLeft = 150;
        for (Entity e = Entity::first(); !e.eof(); e.next())
            if (e.test(splashMask2)) { framesLeft = e.get<LevelSplash>().framesLeft; break; }

        // ── LEVEL 4: three-phase dramatic surprise reveal ──────────────────────
        if (_current_level == 4) {
            // Phase 1 (frames 300→241): victory celebration with stats
            if (framesLeft > 240) {
                SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(ren, 0, 20, 0, 200);
                SDL_FRect full = {0,0,(float)WIN_W,(float)WIN_H}; SDL_RenderFillRect(ren, &full);
                SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

                SDL_SetRenderScale(ren, 5.f, 5.f);
                SDL_SetRenderDrawColor(ren, 80, 255, 80, 255);
                SDL_RenderDebugText(ren, 60.f, 34.f, "BOSS DEFEATED!");
                SDL_SetRenderScale(ren, 1.f, 1.f);

                const int acc = _prev_shots > 0 ? (_prev_hits * 100 / _prev_shots) : 0;
                char sb[64];
                SDL_SetRenderScale(ren, 2.f, 2.f);
                SDL_SetRenderDrawColor(ren, 160, 255, 160, 255);
                SDL_snprintf(sb, sizeof(sb), "Kills: %d   Accuracy: %d%%   Time: %ds",
                             _prev_kills, acc, _prev_wave_secs);
                SDL_RenderDebugText(ren, 30.f, 130.f, sb);
                SDL_SetRenderScale(ren, 1.f, 1.f);
            }
            // Phase 2 (frames 240→151): ominous "but wait…"
            else if (framesLeft > 150) {
                SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(ren, 20, 0, 0, 200);
                SDL_FRect full = {0,0,(float)WIN_W,(float)WIN_H}; SDL_RenderFillRect(ren, &full);
                SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

                const bool blink = (SDL_GetTicks() / 300) % 2 == 0;
                if (blink) {
                    SDL_SetRenderScale(ren, 6.f, 6.f);
                    SDL_SetRenderDrawColor(ren, 255, 60, 20, 255);
                    SDL_RenderDebugText(ren, 42.f, 42.f, "BUT WAIT...");
                    SDL_SetRenderScale(ren, 1.f, 1.f);
                }
                SDL_SetRenderScale(ren, 2.f, 2.f);
                SDL_SetRenderDrawColor(ren, 160, 100, 80, 255);
                SDL_RenderDebugText(ren, 170.f, 200.f, "...something is coming...");
                SDL_SetRenderScale(ren, 1.f, 1.f);
            }
            // Phase 3 (frames 150→0): FULL SURPRISE REVEAL
            else {
                // Red alarm flash
                const bool alarm = (SDL_GetTicks() / 200) % 2 == 0;
                SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(ren, alarm ? 80 : 20, 0, 0, 210);
                SDL_FRect full = {0,0,(float)WIN_W,(float)WIN_H}; SDL_RenderFillRect(ren, &full);
                SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

                SDL_SetRenderScale(ren, 3.5f, 3.5f);
                SDL_SetRenderDrawColor(ren, 255, 30, 30, 255);
                SDL_RenderDebugText(ren, 24.f, 22.f, "THEY SURVIVED THE STRIKE!");
                SDL_SetRenderScale(ren, 1.f, 1.f);

                SDL_SetRenderScale(ren, 2.5f, 2.5f);
                SDL_SetRenderDrawColor(ren, 255, 200, 30, 255);
                SDL_RenderDebugText(ren, 48.f, 68.f, "ATTACKING FROM BELOW!");
                SDL_SetRenderScale(ren, 1.f, 1.f);

                SDL_SetRenderScale(ren, 3.f, 3.f);
                SDL_SetRenderDrawColor(ren, 80, 220, 255, 255);
                SDL_RenderDebugText(ren, 42.f, 110.f, "YOU ARE THE INVADER NOW");
                SDL_SetRenderScale(ren, 1.f, 1.f);

                SDL_SetRenderScale(ren, 2.f, 2.f);
                SDL_SetRenderDrawColor(ren, 180, 180, 180, 255);
                SDL_RenderDebugText(ren, 158.f, 175.f, "Shoot  D O W N  this time!");
                SDL_SetRenderScale(ren, 1.f, 1.f);
            }
            return;
        }

        // ── Regular level transition (levels 1→2, 2→3) ────────────────────────
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 175);
        SDL_FRect full = {0, 0, (float)WIN_W, (float)WIN_H};
        SDL_RenderFillRect(ren, &full);
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

        // Central card — screen Y 194–474 (height 280)
        SDL_SetRenderDrawColor(ren, 80, 220, 80, 255);   // green border
        SDL_FRect cardBorder = {259.f, 194.f, 506.f, 286.f};
        SDL_RenderFillRect(ren, &cardBorder);
        SDL_SetRenderDrawColor(ren, 20, 55, 20, 255);    // dark green fill
        SDL_FRect card = {262.f, 197.f, 500.f, 280.f};
        SDL_RenderFillRect(ren, &card);

        // ── Title "LEVEL X"  — screen y=212, bottom=252  (scale 5 → char=40px tall)
        // canvas y = 212/5 = 42.4 → 42
        // "LEVEL 1" = 7 chars; "LEVEL 10" = 8 chars; centred on screen
        char buf[32];
        SDL_snprintf(buf, sizeof(buf), "LEVEL %d", _current_level);
        const int titleChars = (int)SDL_strlen(buf);
        SDL_SetRenderScale(ren, 5.f, 5.f);
        SDL_SetRenderDrawColor(ren, 80, 255, 80, 255);
        SDL_RenderDebugText(ren, (WIN_W / 5.f - titleChars * 8.f) / 2.f, 42.f, buf);
        SDL_SetRenderScale(ren, 1.f, 1.f);

        // Divider at screen y=260
        SDL_SetRenderDrawColor(ren, 50, 130, 50, 255);
        SDL_FRect div1 = {282.f, 260.f, 460.f, 1.f};
        SDL_RenderFillRect(ren, &div1);

        // ── "Get ready..."  — screen y=270, bottom=286  (scale 2 → char=16px tall)
        // canvas y = 270/2 = 135;  "Get ready..." = 12 chars × 8 × 2 = 192px; centre x = (512-192)/2 = 160
        SDL_SetRenderScale(ren, 2.f, 2.f);
        SDL_SetRenderDrawColor(ren, 140, 200, 140, 255);
        SDL_RenderDebugText(ren, 160.f, 135.f, "Get ready...");
        SDL_SetRenderScale(ren, 1.f, 1.f);

        // Divider at screen y=300
        SDL_SetRenderDrawColor(ren, 50, 130, 50, 255);
        SDL_FRect div2 = {282.f, 300.f, 460.f, 1.f};
        SDL_RenderFillRect(ren, &div2);

        // ── Stats  — scale 2; three rows starting at screen y=314
        // canvas y = 314/2 = 157;  row spacing = 26px screen = 13 canvas
        const int acc = _prev_shots > 0 ? (_prev_hits * 100 / _prev_shots) : 0;
        char statBuf[48];
        SDL_SetRenderScale(ren, 2.f, 2.f);
        SDL_SetRenderDrawColor(ren, 160, 220, 160, 255);

        SDL_snprintf(statBuf, sizeof(statBuf), "Kills: %d", _prev_kills);
        SDL_RenderDebugText(ren, 157.f, 157.f, statBuf);  // screen y=314

        if (_prev_shots > 0)
            SDL_snprintf(statBuf, sizeof(statBuf), "Accuracy: %d%%", acc);
        else
            SDL_snprintf(statBuf, sizeof(statBuf), "Accuracy: --");
        SDL_RenderDebugText(ren, 157.f, 170.f, statBuf);  // screen y=340

        SDL_snprintf(statBuf, sizeof(statBuf), "Time: %ds", _prev_wave_secs);
        SDL_RenderDebugText(ren, 157.f, 183.f, statBuf);  // screen y=366
        SDL_SetRenderScale(ren, 1.f, 1.f);
    }

    // ──────────────────────── Select screen ──────────────────────────────────────
    void Game::select_input() const
    {
        static const Mask ssMask = MaskBuilder().set<SelectState>().build();
        Entity ssEnt = Entity::first();
        for (Entity e = Entity::first(); !e.eof(); e.next())
            if (e.test(ssMask)) { ssEnt = e; break; }
        auto& ss = ssEnt.get<SelectState>();

        const bool* keys = SDL_GetKeyboardState(nullptr);
        const bool goRight = keys[SDL_SCANCODE_RIGHT];
        const bool goLeft  = keys[SDL_SCANCODE_LEFT];
        const bool lr = goLeft || goRight;
        const bool ud = keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_DOWN];

        if ((lr || ud) && !ss.moved) {
            if (goRight) ss.selected = (ss.selected + 1) % NUM_CHARS;
            if (goLeft)  ss.selected = (ss.selected - 1 + NUM_CHARS) % NUM_CHARS;
            if (keys[SDL_SCANCODE_UP])   ss.difficulty = std::min(ss.difficulty + 1, 2);
            if (keys[SDL_SCANCODE_DOWN]) ss.difficulty = std::max(ss.difficulty - 1, 0);
            ss.moved = true;
        } else if (!lr && !ud) {
            ss.moved = false;
        }

        // Play anthem/song when character changes
        if (_select_last_char != ss.selected) {
            _select_last_char = ss.selected;
            // sfx map: index → type (0=no sound)
            static const int char_sfx[NUM_CHARS] = {
                8,   // 0  Trump       — Star-Spangled Banner
                7,   // 1  Bibi        — Hatikva
                9,   // 2  Ben Gvir    — Ani Maamin
                10,  // 3  Zelensky    — Ukrainian anthem
                11,  // 4  Putin       — Russian anthem
                18,  // 5  Obama       — F*** tha Police
                12,  // 6  Eminem      — Slim Shady
                13,  // 7  Madonna     — Like a Virgin
                14,  // 8  M.Jackson   — Thriller
                15,  // 9  Sara        — Ba Ma
                16,  // 10 Stalin      — Moscow
                17,  // 11 Yoamashit   — Ma Sheat Ohevet
            };
            const int sfx = char_sfx[ss.selected];
            if (sfx != 0) play_sfx(sfx);
            else          SDL_ClearAudioStream(audio_stream);
        }
    }

    void Game::select_draw() const
    {
        static const Mask ssMask = MaskBuilder().set<SelectState>().build();
        int selected = 0, difficulty = 1;
        for (Entity e = Entity::first(); !e.eof(); e.next())
            if (e.test(ssMask)) {
                selected   = e.get<SelectState>().selected;
                difficulty = e.get<SelectState>().difficulty;
                break;
            }

        // ── Deep-space animated starfield ──
        SDL_SetRenderDrawColor(ren, 4, 4, 18, 255);
        SDL_FRect bg = {0, 0, (float)WIN_W, (float)WIN_H};
        SDL_RenderFillRect(ren, &bg);
        // slow layer
        SDL_SetRenderDrawColor(ren, 60, 60, 110, 255);
        for (int i = 0; i < 80; i++) {
            float sx = (float)((i * 137 + 23) % WIN_W);
            float sy = (float)(((int)(_select_scroll * 0.35f) + i * 89) % WIN_H);
            SDL_FRect star = {sx, sy, 1.f, 1.f}; SDL_RenderFillRect(ren, &star);
        }
        // fast layer
        SDL_SetRenderDrawColor(ren, 200, 200, 255, 255);
        for (int i = 0; i < 30; i++) {
            float sx = (float)((i * 317 + 71) % WIN_W);
            float sy = (float)(((int)(_select_scroll * 0.9f) + i * 157) % WIN_H);
            SDL_FRect star = {sx, sy, 2.f, 2.f}; SDL_RenderFillRect(ren, &star);
        }

        // ── Title panel  (Y: 0 → 82) ──
        // Panel bg — dark navy, compact so images get maximum room
        SDL_SetRenderDrawColor(ren, 8, 12, 38, 255);
        SDL_FRect titlePanel = {0.f, 0.f, (float)WIN_W, 82.f};
        SDL_RenderFillRect(ren, &titlePanel);
        SDL_SetRenderDrawColor(ren, 60, 90, 200, 255);
        SDL_FRect titleBorder = {0.f, 80.f, (float)WIN_W, 2.f};
        SDL_RenderFillRect(ren, &titleBorder);

        // "CROSSY INVADERS" — scale 3: char height 24px
        // canvas y=9 → screen y=27, bottom=51
        // 15 chars × 8 = 120 canvas px; canvas width=341.3; centre x=(341.3-120)/2=110
        SDL_SetRenderScale(ren, 3.f, 3.f);
        SDL_SetRenderDrawColor(ren, 100, 70, 0, 255);    // shadow
        SDL_RenderDebugText(ren, 111.f, 10.f, "CROSSY INVADERS");
        SDL_SetRenderDrawColor(ren, 255, 200, 50, 255);  // gold
        SDL_RenderDebugText(ren, 110.f, 9.f, "CROSSY INVADERS");
        SDL_SetRenderScale(ren, 1.f, 1.f);

        // Subtitle — scale 1.2: char height ~10px
        // screen y=60, bottom=70  (9px clear of title bottom=51)
        // 26 chars × 8 = 208 canvas px; canvas width=853; centre x=(853-208)/2=322
        SDL_SetRenderScale(ren, 1.2f, 1.2f);
        SDL_SetRenderDrawColor(ren, 130, 170, 255, 255);
        SDL_RenderDebugText(ren, 322.f, 60.f / 1.2f, "Stop the Iranian Invasion!");
        SDL_SetRenderScale(ren, 1.f, 1.f);

        // Best score — top-right of panel
        // "BEST XXXXX" = 10 chars × 8 × 1.2 = 96px; right-align
        SDL_SetRenderScale(ren, 1.2f, 1.2f);
        SDL_SetRenderDrawColor(ren, 255, 215, 30, 255);
        char hsbuf[24]; SDL_snprintf(hsbuf, sizeof(hsbuf), "BEST %05d", _high_score);
        SDL_RenderDebugText(ren, ((float)WIN_W - 104.f) / 1.2f, 9.f, hsbuf);
        SDL_SetRenderScale(ren, 1.f, 1.f);

        // ── Character carousel  (Y: 92 → 402) ──
        // Carousel shows 5 cards: selected (large) + ±1 (medium) + ±2 (small)
        // Card geometry
        static const char* char_names[NUM_CHARS] = {
            "TRUMP", "BIBI", "BEN GVIR", "ZELENSKY", "PUTIN", "OBAMA",
            "EMINEM", "MADONNA", "M.JACKSON", "SARA", "STALIN", "YOAMASHIT"
        };
        // Per-character accent colours (bg, header)
        static const SDL_Color char_bg[NUM_CHARS] = {
            {150,75,15,255}, {20,60,150,255}, {80,20,20,255}, {20,100,60,255},
            {80,10,10,255},  {10,40,100,255}, {60,20,80,255}, {100,20,60,255},
            {20,20,20,255},  {80,40,80,255},  {30,10,10,255}, {10,60,40,255}
        };
        static const SDL_Color char_hdr[NUM_CHARS] = {
            {210,110,30,255}, {50,110,220,255}, {160,40,40,255}, {40,180,100,255},
            {160,20,20,255},  {20,80,200,255},  {130,50,180,255},{200,50,120,255},
            {60,60,60,255},   {160,80,160,255}, {80,20,20,255},  {20,140,80,255}
        };

        // Slot offsets: -2, -1, 0(selected), +1, +2
        constexpr int SLOTS = 5;
        const int offsets[SLOTS] = { -2, -1, 0, 1, 2 };
        // Card sizes per slot (index 0..4)
        const float card_w[SLOTS] = { 120.f, 175.f, 240.f, 175.f, 120.f };
        const float card_h[SLOTS] = { 165.f, 240.f, 310.f, 240.f, 165.f };
        constexpr float BOX_Y = 92.f;
        constexpr float CX = WIN_W / 2.f;  // 512

        // Compute left-edge x for each slot (cards packed from center)
        // selected card is centered at CX
        float card_x[SLOTS];
        card_x[2] = CX - card_w[2] / 2.f;                          // selected
        card_x[1] = card_x[2] - 20.f - card_w[1];                  // left-1
        card_x[0] = card_x[1] - 15.f - card_w[0];                  // left-2
        card_x[3] = card_x[2] + card_w[2] + 20.f;                  // right-1
        card_x[4] = card_x[3] + card_w[3] + 15.f;                  // right-2

        // Draw cards back-to-front (±2 first, selected last)
        const int draw_order[SLOTS] = { 0, 4, 1, 3, 2 };
        for (int di = 0; di < SLOTS; di++) {
            const int si = draw_order[di];
            const int ci = (selected + offsets[si] + NUM_CHARS) % NUM_CHARS;
            const float bx = card_x[si];
            const float by = BOX_Y + (card_h[2] - card_h[si]) / 2.f;  // vertically centered
            const float bw = card_w[si];
            const float bh = card_h[si];
            const bool isSel = (si == 2);

            // Yellow glow border for selected
            if (isSel) {
                SDL_SetRenderDrawColor(ren, 255, 255, 100, 255);
                SDL_FRect glow = {bx - 6.f, by - 6.f, bw + 12.f, bh + 12.f};
                SDL_RenderFillRect(ren, &glow);
            }

            // Card background
            SDL_SetRenderDrawColor(ren, char_bg[ci].r, char_bg[ci].g, char_bg[ci].b, 255);
            SDL_FRect box = {bx, by, bw, bh};
            SDL_RenderFillRect(ren, &box);

            // Header strip
            const float hdrH = bh * 0.10f;  // ~10% of card height
            SDL_SetRenderDrawColor(ren, char_hdr[ci].r, char_hdr[ci].g, char_hdr[ci].b, 255);
            SDL_FRect hdr = {bx, by, bw, hdrH};
            SDL_RenderFillRect(ren, &hdr);

            // Character name in header — scale by card width relative to selected
            const float nameScale = isSel ? 2.f : (si == 1 || si == 3) ? 1.5f : 1.1f;
            SDL_SetRenderScale(ren, nameScale, nameScale);
            SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
            const float nameLen = (float)SDL_strlen(char_names[ci]) * 8.f;
            const float nameX = (bx + (bw - nameLen * nameScale) / 2.f) / nameScale;
            const float nameY = (by + (hdrH - 8.f * nameScale) / 2.f) / nameScale;
            SDL_RenderDebugText(ren, nameX, nameY, char_names[ci]);
            SDL_SetRenderScale(ren, 1.f, 1.f);

            // Portrait sprite
            const float pad = bw * 0.04f;
            SDL_FRect img = {bx + pad, by + hdrH + 2.f, bw - pad * 2.f, bh - hdrH - 4.f};
            if (char_select_tex[ci])
                SDL_RenderTexture(ren, char_select_tex[ci], nullptr, &img);

            // Dim overlay for non-selected cards (makes selected pop)
            if (!isSel) {
                SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(ren, 0, 0, 0, (si == 1 || si == 3) ? 90 : 150);
                SDL_RenderFillRect(ren, &box);
                SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
            }

            // Shine sweep on selected card only
            if (isSel) {
                constexpr float PERIOD = 240.f, SHINE_W = 28.f;
                const float t = std::fmod(_select_scroll, PERIOD) / PERIOD;
                const float rawX = bx + pad - SHINE_W + t * (img.w + SHINE_W * 2.f);
                const float alpha = std::sin(t * (float)M_PI) * 110.f;
                if (alpha > 2.f) {
                    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
                    SDL_SetRenderDrawColor(ren, 255, 255, 240, (Uint8)alpha);
                    const float sx = std::max(bx + pad, std::min(rawX, bx + pad + img.w - SHINE_W));
                    SDL_FRect shine = {sx, img.y, SHINE_W, img.h};
                    SDL_RenderFillRect(ren, &shine);
                    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
                }
            }
        }

        // ← → hint + character counter below carousel
        {
            char cntbuf[16];
            SDL_snprintf(cntbuf, sizeof(cntbuf), "%d / %d", selected + 1, NUM_CHARS);
            // Navigation arrows
            SDL_SetRenderScale(ren, 1.2f, 1.2f);
            SDL_SetRenderDrawColor(ren, 140, 140, 160, 255);
            SDL_RenderDebugText(ren, 290.f, 414.f / 1.2f, "<  LEFT / RIGHT  >");
            // Counter centred
            SDL_SetRenderDrawColor(ren, 200, 200, 80, 255);
            const float cntW = (float)SDL_strlen(cntbuf) * 8.f * 1.2f;
            SDL_RenderDebugText(ren, ((float)WIN_W / 2.f - cntW / 2.f) / 1.2f, 430.f / 1.2f, cntbuf);
            SDL_SetRenderScale(ren, 1.f, 1.f);
        }

        // ── Difficulty selector  (Y: 438 → 488) ──
        constexpr float diffY = 440.f;

        SDL_SetRenderDrawColor(ren, 10, 10, 28, 255);
        SDL_FRect diffStrip = {0.f, diffY - 6.f, (float)WIN_W, 56.f};
        SDL_RenderFillRect(ren, &diffStrip);
        SDL_SetRenderDrawColor(ren, 40, 40, 80, 255);
        SDL_FRect diffLineTop = {0.f, diffY - 7.f, (float)WIN_W, 1.f};
        SDL_FRect diffLineBot = {0.f, diffY + 49.f, (float)WIN_W, 1.f};
        SDL_RenderFillRect(ren, &diffLineTop);
        SDL_RenderFillRect(ren, &diffLineBot);

        // "DIFFICULTY:" label
        SDL_SetRenderScale(ren, 1.2f, 1.2f);
        SDL_SetRenderDrawColor(ren, 130, 130, 180, 255);
        SDL_RenderDebugText(ren, 16.f, (diffY + 12.f) / 1.2f, "DIFFICULTY:");
        SDL_SetRenderScale(ren, 1.f, 1.f);

        // Three buttons — centred; each 120×36
        // Total = 3×120 + 2×18 = 396; start x = (1024-396)/2 = 314
        const float diffBtnX[3] = {314.f, 452.f, 590.f};
        const SDL_Color diffBgSel[3]  = {{0,100,0,255},{0,0,100,255},{100,0,0,255}};
        const SDL_Color diffTxtCol[3] = {{80,255,80,255},{150,220,255,255},{255,80,80,255}};
        const char* diffLabels[]      = {"  EASY  ", " NORMAL ", "  HARD  "};
        for (int d = 0; d < 3; d++) {
            const bool sel = (d == difficulty);
            if (sel) {
                SDL_SetRenderDrawColor(ren, 255, 255, 80, 255);
                SDL_FRect bord = {diffBtnX[d] - 2.f, diffY - 2.f, 124.f, 40.f};
                SDL_RenderFillRect(ren, &bord);
            }
            SDL_SetRenderDrawColor(ren, sel ? diffBgSel[d].r : (Uint8)20,
                                         sel ? diffBgSel[d].g : (Uint8)20,
                                         sel ? diffBgSel[d].b : (Uint8)30, 255);
            SDL_FRect db = {diffBtnX[d], diffY, 120.f, 36.f};
            SDL_RenderFillRect(ren, &db);
            // label at scale 1.6
            SDL_SetRenderScale(ren, 1.6f, 1.6f);
            SDL_SetRenderDrawColor(ren, diffTxtCol[d].r, diffTxtCol[d].g, diffTxtCol[d].b, 255);
            SDL_RenderDebugText(ren, (diffBtnX[d] + 6.f) / 1.6f, (diffY + 11.f) / 1.6f, diffLabels[d]);
            SDL_SetRenderScale(ren, 1.f, 1.f);
        }
        // "UP/DN" hint
        SDL_SetRenderScale(ren, 1.2f, 1.2f);
        SDL_SetRenderDrawColor(ren, 80, 80, 120, 255);
        SDL_RenderDebugText(ren, (diffBtnX[2] + 130.f) / 1.2f, (diffY + 12.f) / 1.2f, "UP/DN");
        SDL_SetRenderScale(ren, 1.f, 1.f);

        // ── Instructions  (Y: 502 → 622)  scale 1.8 ──
        // Top separator
        SDL_SetRenderDrawColor(ren, 40, 50, 90, 255);
        SDL_FRect instrSep = {40.f, 500.f, (float)WIN_W - 80.f, 1.f};
        SDL_RenderFillRect(ren, &instrSep);

        // Canvas width at scale 1.8 = 1024/1.8 ≈ 568 px
        SDL_SetRenderScale(ren, 1.8f, 1.8f);

        // ── Header ──
        SDL_SetRenderDrawColor(ren, 200, 200, 240, 255);
        SDL_RenderDebugText(ren, 10.f, 510.f / 1.8f, "HOW TO PLAY:");

        // ── Controls row 1: ARROWS · SPACE · I ──
        // Key colour: bright blue  |  description colour: soft grey
        SDL_SetRenderDrawColor(ren, 100, 160, 255, 255);
        SDL_RenderDebugText(ren,  10.f, 540.f / 1.8f, "ARROWS");
        SDL_SetRenderDrawColor(ren, 180, 180, 205, 255);
        SDL_RenderDebugText(ren,  66.f, 540.f / 1.8f, "Move");

        SDL_SetRenderDrawColor(ren, 100, 160, 255, 255);
        SDL_RenderDebugText(ren, 180.f, 540.f / 1.8f, "SPACE");
        SDL_SetRenderDrawColor(ren, 180, 180, 205, 255);
        SDL_RenderDebugText(ren, 228.f, 540.f / 1.8f, "Shoot");

        SDL_SetRenderDrawColor(ren, 100, 160, 255, 255);
        SDL_RenderDebugText(ren, 350.f, 540.f / 1.8f, "I");
        SDL_SetRenderDrawColor(ren, 180, 180, 205, 255);
        SDL_RenderDebugText(ren, 366.f, 540.f / 1.8f, "Iron Dome");

        // ── Controls row 2: SHIFT · P · Q  (aligned to same x-columns as row 1) ──
        SDL_SetRenderDrawColor(ren, 100, 160, 255, 255);
        SDL_RenderDebugText(ren,  10.f, 558.f / 1.8f, "SHIFT");
        SDL_SetRenderDrawColor(ren, 180, 180, 205, 255);
        SDL_RenderDebugText(ren,  58.f, 558.f / 1.8f, "Dash");

        SDL_SetRenderDrawColor(ren, 100, 160, 255, 255);
        SDL_RenderDebugText(ren, 180.f, 558.f / 1.8f, "P");
        SDL_SetRenderDrawColor(ren, 180, 180, 205, 255);
        SDL_RenderDebugText(ren, 196.f, 558.f / 1.8f, "Pause");

        SDL_SetRenderDrawColor(ren, 100, 160, 255, 255);
        SDL_RenderDebugText(ren, 350.f, 558.f / 1.8f, "Q");
        SDL_SetRenderDrawColor(ren, 180, 180, 205, 255);
        SDL_RenderDebugText(ren, 366.f, 558.f / 1.8f, "Slow Mo");

        // ── Pickups — each name is the colour of its in-game square/icon ──
        SDL_SetRenderDrawColor(ren, 200, 200, 240, 255);
        SDL_RenderDebugText(ren,  10.f, 575.f / 1.8f, "PICKUPS:");

        SDL_SetRenderDrawColor(ren, 255, 220, 30, 255);   // yellow  → rapid fire
        SDL_RenderDebugText(ren, 110.f, 575.f / 1.8f, "Rapid Fire");

        SDL_SetRenderDrawColor(ren,  80, 180, 255, 255);  // sky-blue → Iron Dome charge
        SDL_RenderDebugText(ren, 250.f, 575.f / 1.8f, "Iron Dome");

        SDL_SetRenderDrawColor(ren, 255,  80,  80, 255);  // red      → +Heart
        SDL_RenderDebugText(ren, 382.f, 575.f / 1.8f, "+Heart");

        SDL_SetRenderScale(ren, 1.f, 1.f);

        // Bottom separator
        SDL_SetRenderDrawColor(ren, 40, 50, 90, 255);
        SDL_FRect instrSep2 = {40.f, 622.f, (float)WIN_W - 80.f, 1.f};
        SDL_RenderFillRect(ren, &instrSep2);

        // ── Blinking "PRESS SPACE TO PLAY"  (Y: 686) ──
        // Pushed down slightly to leave breathing room after the larger instructions block.
        if ((SDL_GetTicks() / 500) % 2 == 0) {
            // scale 2: "PRESS  SPACE  TO  PLAY" = 22×8×2=352px; centre x=(1024-352)/2=336; canvas=168
            SDL_SetRenderScale(ren, 2.f, 2.f);
            SDL_SetRenderDrawColor(ren, 255, 200, 50, 255);
            SDL_RenderDebugText(ren, 168.f, 686.f / 2.f, "PRESS  SPACE  TO  PLAY");
            SDL_SetRenderScale(ren, 1.f, 1.f);
        }
    }

    // ────────────────────────────── Game systems ─────────────────────────────────
    void Game::input_system() const
    {
        static const Mask mask = MaskBuilder().set<PlayerTag>().set<InputState>().build();
        const bool* keys = SDL_GetKeyboardState(nullptr);
        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (!e.test(mask)) continue;
            auto& s    = e.get<InputState>();
            s.up       = keys[SDL_SCANCODE_UP];
            s.down     = keys[SDL_SCANCODE_DOWN];
            s.left     = keys[SDL_SCANCODE_LEFT];
            s.right    = keys[SDL_SCANCODE_RIGHT];
            s.shoot    = keys[SDL_SCANCODE_SPACE];
            s.activate = keys[SDL_SCANCODE_I];
            s.dash     = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
            s.slowmo   = keys[SDL_SCANCODE_Q];
        }
    }

    void Game::player_move_system() const
    {
        static const Mask mask = MaskBuilder()
            .set<PlayerTag>().set<InputState>().set<LanePos>().set<Transform>().build();
        static const Mask enemyMask = MaskBuilder().set<EnemyTag>().build();

        bool anyEnemy = false;
        for (Entity e = Entity::first(); !e.eof(); e.next())
            if (e.test(enemyMask)) { anyEnemy = true; break; }

        const int scrollTopLane = static_cast<int>((WIN_H - TILE / 2.f + _camera_scroll) / TILE);
        int max_lane = scrollTopLane - (anyEnemy ? 1 : 0);

        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (!e.test(mask)) continue;
            auto& s  = e.get<InputState>();
            auto& lp = e.get<LanePos>();
            auto& t  = e.get<Transform>();

            const bool any = s.up || s.down || s.left || s.right;

            // Determine move delta (dash vs normal)
            int delta = 1;
            bool isDash = false;
            if (s.dash && e.has<DashState>() && e.get<DashState>().cooldown == 0) {
                delta  = 2;
                isDash = true;
            }

            if (any && !s.moved) {
                const SDL_FPoint oldPos = t.p;
                if (s.up)    lp.lane += delta;
                if (s.down)  lp.lane -= delta;
                if (s.left)  lp.col  -= delta;
                if (s.right) lp.col  += delta;
                s.moved = true;

                if (isDash) {
                    e.get<DashState>().cooldown = 60;
                    if (e.has<Invincibility>())
                        e.get<Invincibility>().frames = std::max(e.get<Invincibility>().frames, 12);
                }

                // Hop animation — direction drives diagonal arc
                if (e.has<HopState>()) {
                    auto& hop = e.get<HopState>();
                    hop.frames = hop.maxFrames;
                    hop.hopDX  = s.up ? 1.f : s.down ? -1.f : 0.f;
                }

                // Footstep dust particle at old position
                Entity::create().addAll(Transform{oldPos, 0.f}, Explosion{8, 8, 10.f});
            } else if (!any) {
                s.moved = false;
            }

            lp.lane = clamp(lp.lane, 0, max_lane);
            lp.col  = clamp(lp.col,  0, COLS - 1);
            t.p.x = TILE / 2.f + lp.col  * TILE;
            t.p.y = WIN_H - TILE / 2.f - lp.lane * TILE;
        }
    }

    void Game::enemy_move_system() const
    {
        static const Mask mask    = MaskBuilder().set<EnemyTag>().set<LanePos>().set<Transform>().set<Drawable>().build();
        static const Mask fsMask  = MaskBuilder().set<FormationState>().build();
        static const Mask ssMask  = MaskBuilder().set<SpeedScale>().build();

        float speedScale = 1.0f;
        for (Entity e = Entity::first(); !e.eof(); e.next())
            if (e.test(ssMask)) { speedScale = e.get<SpeedScale>().active; break; }

        // ── Level 2: independent per-enemy random movement ──
        if (_current_level == 2) {
            static const Mask indvMask = MaskBuilder()
                .set<EnemyTag>().set<LanePos>().set<Transform>().set<IndividualMove>().build();
            const Uint64 now = SDL_GetTicks();
            for (Entity e = Entity::first(); !e.eof(); e.next()) {
                if (!e.test(indvMask)) continue;
                auto& im = e.get<IndividualMove>();
                if (now < im.nextMove) continue;
                auto& lp = e.get<LanePos>();
                auto& t  = e.get<Transform>();
                const int dir = (std::rand() % 2 == 0) ? 1 : -1;
                lp.col += dir;
                lp.col  = clamp(lp.col, 0, COLS - 1);
                t.p.x   = TILE / 2.f + lp.col * TILE;
                Uint64 interval = (Uint64)((200 + std::rand() % 500) / speedScale);
                im.nextMove = now + interval;
            }
            return;
        }

        // ── Levels 1, 3, 4: classic formation ──
        Entity fsEnt = Entity::first();
        for (Entity e = Entity::first(); !e.eof(); e.next())
            if (e.test(fsMask)) { fsEnt = e; break; }
        auto& fs = fsEnt.get<FormationState>();

        const Uint64 now = SDL_GetTicks();
        Uint64 move_ms = (Uint64)(ENEMY_MOVE_MS / speedScale);
        if (_current_level == 3) move_ms = (Uint64)(move_ms * 0.5f);
        if (_current_level == 4) move_ms = (Uint64)(move_ms * 0.8f);

        // Difficulty ramp within wave
        if (_wave_enemy_count > 0) {
            int remaining = 0;
            for (Entity e = Entity::first(); !e.eof(); e.next())
                if (e.test(mask)) remaining++;
            move_ms = (Uint64)(move_ms * std::max(0.3f, (float)remaining / _wave_enemy_count));
        }

        if (now - fs.moveTimer < move_ms) return;
        fs.moveTimer = now;

        // Check wall hit — uses both col-index guard (regular enemies) and
        // a visual-size guard (large boss whose half-width exceeds TILE/2).
        bool hitEdge = false;
        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (!e.test(mask)) continue;
            const int   col = e.get<LanePos>().col;
            const float hw  = e.get<Drawable>().size.x / 2.f;
            const float cx  = TILE / 2.f + col * TILE;
            if ((fs.dir ==  1 && (col >= COLS - 1 || cx + hw >= (float)WIN_W)) ||
                (fs.dir == -1 && (col <= 0        || cx - hw <= 0.f))) {
                hitEdge = true; break;
            }
        }

        if (hitEdge) {
            fs.dir = -fs.dir;
            if (_current_level == 4) {
                // Role reversal: enemies advance UPWARD on edge hit
                for (Entity e = Entity::first(); !e.eof(); e.next()) {
                    if (!e.test(mask)) continue;
                    e.get<LanePos>().lane++;
                    e.get<Transform>().p.y -= TILE;
                }
            }
        }

        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (!e.test(mask)) continue;
            auto& lp = e.get<LanePos>();
            auto& t  = e.get<Transform>();
            lp.col += fs.dir;
            lp.col  = clamp(lp.col, 0, COLS - 1);
            t.p.x   = TILE / 2.f + lp.col * TILE;
            t.p.y   = WIN_H - TILE / 2.f - lp.lane * TILE;
            // Hard-clamp screen X so oversized entities (boss) never stray off screen.
            const float hw = e.get<Drawable>().size.x / 2.f;
            if (t.p.x - hw < 0.f)          t.p.x = hw;
            if (t.p.x + hw > (float)WIN_W) t.p.x = (float)WIN_W - hw;
        }
    }

    void Game::shoot_system() const
    {
        static const Mask playerMask = MaskBuilder().set<PlayerTag>().set<InputState>().set<Transform>().build();
        static const Mask enemyMask  = MaskBuilder().set<EnemyTag>().set<LanePos>().set<Transform>().build();
        static const Mask fsMask     = MaskBuilder().set<FormationState>().build();
        static const Mask gsMask     = MaskBuilder().set<GameStatus>().build();

        // Level 4: player shoots DOWN, enemies shoot UP; levels 1-3: opposite
        const float playerBulletDY = (_current_level == 4) ?  BULLET_SPEED : -BULLET_SPEED;
        const float enemyBulletDY  = (_current_level == 4) ? -BULLET_SPEED :  BULLET_SPEED;

        // Player shooting
        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (!e.test(playerMask)) continue;
            auto& s  = e.get<InputState>();
            auto& rf = e.get<RapidFire>();

            bool doShoot = false;
            if (rf.frames > 0) {
                rf.frames--;
                if (rf.cooldown > 0) rf.cooldown--;
                else if (s.shoot) { doShoot = true; rf.cooldown = 5; }
            } else {
                if (s.shoot && !s.shotFired) { doShoot = true; s.shotFired = true; }
                else if (!s.shoot) s.shotFired = false;
            }

            if (doShoot) {
                // Track shots fired for accuracy stat
                for (Entity g = Entity::first(); !g.eof(); g.next())
                    if (g.test(gsMask)) { g.get<GameStatus>().shots++; break; }

                const SDL_FPoint p = e.get<Transform>().p;
                const bool spread = e.has<SpreadShot>() && e.get<SpreadShot>().frames > 0;
                if (spread) {
                    for (float dx : {-2.5f, 0.f, 2.5f})
                        Entity::create().addAll(Transform{p, 0.f}, Drawable{{0,0,0,0},{6.f,14.f}},
                                               BulletTag{true}, Velocity{dx, playerBulletDY});
                } else {
                    Entity::create().addAll(Transform{p, 0.f}, Drawable{{0,0,0,0},{6.f,14.f}},
                                           BulletTag{true}, Velocity{0.f, playerBulletDY});
                }
                Entity::create().add(SoundEvent{0});
            }
        }

        // Enemy shooting
        Entity fsEnt = Entity::first();
        for (Entity e = Entity::first(); !e.eof(); e.next())
            if (e.test(fsMask)) { fsEnt = e; break; }
        auto& fs = fsEnt.get<FormationState>();

        const Uint64 now = SDL_GetTicks();
        // Read SpeedScale for unified difficulty + SlowMo speed control
        static const Mask ssMask2 = MaskBuilder().set<SpeedScale>().build();
        float shootSpeedScale = 1.0f;
        for (Entity e = Entity::first(); !e.eof(); e.next())
            if (e.test(ssMask2)) { shootSpeedScale = e.get<SpeedScale>().active; break; }

        Uint64 shoot_ms = (Uint64)(ENEMY_SHOOT_MS / shootSpeedScale);
        if (_current_level == 2) shoot_ms = (Uint64)(shoot_ms * 0.67f);
        if (_current_level == 3) shoot_ms = (Uint64)(shoot_ms * 0.33f);
        if (_current_level == 4) shoot_ms = (Uint64)(shoot_ms * 0.75f);
        if (now - fs.shootTimer < shoot_ms) return;
        fs.shootTimer = now;

        // For level 4 role reversal: find the MAX lane (highest = closest to player)
        // For other levels: find the MIN lane (lowest = closest to player)
        int targetLane = (_current_level == 4) ? 0 : LANES;
        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (!e.test(enemyMask)) continue;
            const int lane = e.get<LanePos>().lane;
            if (_current_level == 4) { if (lane > targetLane) targetLane = lane; }
            else                     { if (lane < targetLane) targetLane = lane; }
        }

        // Count front-row enemies; front row shoots 3× more often
        // (front row = targetLane for levels 1-3; targetLane for level 4)
        // Build a weighted pool: front row has 3 slots, back rows have 1 slot each
        struct ShooterSlot { SDL_FPoint pos; };
        std::vector<ShooterSlot> pool;
        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (!e.test(enemyMask)) continue;
            const int lane   = e.get<LanePos>().lane;
            const int weight = (lane == targetLane) ? 3 : 1;
            for (int w = 0; w < weight; w++)
                pool.push_back({e.get<Transform>().p});
        }
        if (pool.empty()) return;

        const ShooterSlot& chosen = pool[std::rand() % pool.size()];
        const SDL_FPoint p = chosen.pos;
        if (_current_level == 3) {
            for (float dx : {-2.5f, 0.f, 2.5f})
                Entity::create().addAll(Transform{p, 0.f}, Drawable{{0,0,0,0},{6.f,14.f}},
                                       BulletTag{false}, Velocity{dx, enemyBulletDY});
        } else {
            Entity::create().addAll(Transform{p, 0.f}, Drawable{{0,0,0,0},{6.f,14.f}},
                                   BulletTag{false}, Velocity{0.f, enemyBulletDY});
        }
    }

    void Game::bullet_system() const
    {
        static const Mask mask   = MaskBuilder().set<BulletTag>().set<Transform>().set<Velocity>().build();
        static const Mask ssMask = MaskBuilder().set<SpeedScale>().build();

        float speedScale = 1.0f;
        for (Entity e = Entity::first(); !e.eof(); e.next())
            if (e.test(ssMask)) { speedScale = e.get<SpeedScale>().active; break; }

        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (!e.test(mask)) continue;
            auto& t         = e.get<Transform>();
            const auto& v   = e.get<Velocity>();
            const bool fromPl = e.get<BulletTag>().fromPlayer;
            const float sf  = fromPl ? 1.0f : speedScale;
            t.p.x += v.dx * sf;
            t.p.y += v.dy * sf;
            const float screenY = t.p.y + _camera_scroll + (t.p.x - WIN_W * 0.5f) * TILT;
            if (screenY < -(float)TILE || screenY > (float)WIN_H + TILE)
                e.destroy();
        }
    }

    void Game::hazard_move_system() const
    {
        static const Mask mask   = MaskBuilder().set<Hazard>().set<Transform>().set<Drawable>().set<Velocity>().build();
        static const Mask ssMask = MaskBuilder().set<SpeedScale>().build();

        float speedScale = 1.0f;
        for (Entity e = Entity::first(); !e.eof(); e.next())
            if (e.test(ssMask)) { speedScale = e.get<SpeedScale>().active; break; }

        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (!e.test(mask)) continue;
            auto& t        = e.get<Transform>();
            const float dx = e.get<Velocity>().dx;
            const float hw = e.get<Drawable>().size.x / 2.f;
            t.p.x += dx * speedScale;
            if (t.p.x - hw >  WIN_W) t.p.x = -hw;
            if (t.p.x + hw <  0)     t.p.x =  WIN_W + hw;
            const float screenY = t.p.y + _camera_scroll + (t.p.x - WIN_W * 0.5f) * TILT;
            if (screenY > (float)WIN_H + TILE) {
                t.p.y  -= LANES * TILE;
                t.p.x   = (float)(std::rand() % WIN_W);
            }
        }
    }

    void Game::shield_system() const
    {
        static const Mask mask   = MaskBuilder().set<PlayerTag>().set<InputState>().set<Shield>().build();
        static const Mask smMask = MaskBuilder().set<SlowMo>().build();

        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (!e.test(mask)) continue;
            auto& s  = e.get<InputState>();
            auto& sh = e.get<Shield>();

            // Shield (Iron Dome)
            if (s.activate && !s.activateFired && sh.charges > 0 && sh.timer <= 0.f) {
                sh.timer = SHIELD_DURATION; sh.charges--;
                s.activateFired = true;
                // Trigger flash on the icon slot that just disappeared
                if (e.has<ShieldFlash>()) e.get<ShieldFlash>().frames = 45;
            } else if (!s.activate) {
                s.activateFired = false;
            }
            if (sh.timer > 0.f) sh.timer -= 1.f;

            // ShieldFlash countdown
            if (e.has<ShieldFlash>() && e.get<ShieldFlash>().frames > 0) e.get<ShieldFlash>().frames--;

            // SpreadShot countdown
            if (e.has<SpreadShot>() && e.get<SpreadShot>().frames > 0) e.get<SpreadShot>().frames--;

            // Dash cooldown countdown
            if (e.has<DashState>() && e.get<DashState>().cooldown > 0) e.get<DashState>().cooldown--;

            // Bullet-time (SlowMo) activation + SpeedScale update
            for (Entity sm = Entity::first(); !sm.eof(); sm.next()) {
                if (!sm.test(smMask)) continue;
                auto& smc = sm.get<SlowMo>();
                if (smc.frames   > 0) smc.frames--;
                if (smc.cooldown > 0) smc.cooldown--;
                if (s.slowmo && !s.slowmoFired && smc.cooldown == 0 && smc.frames == 0) {
                    smc.frames   = 180;   // 3 s
                    smc.cooldown = 600;   // 10 s total cooldown
                    s.slowmoFired = true;
                    Entity::create().add(SoundEvent{6});
                } else if (!s.slowmo) {
                    s.slowmoFired = false;
                }
                // Keep SpeedScale.active in sync: dramatic override while Q is active,
                // otherwise restore the difficulty baseline.
                auto& ss = sm.get<SpeedScale>();
                ss.active = (smc.frames > 0) ? 0.35f : ss.base;
                break;
            }
            break; // one player
        }
    }

    void Game::collision_system() const
    {
        static const Mask gsMask     = MaskBuilder().set<GameStatus>().set<ScreenShake>().set<ComboState>().build();
        Entity gsEnt = Entity::first();
        for (Entity e = Entity::first(); !e.eof(); e.next())
            if (e.test(gsMask)) { gsEnt = e; break; }
        auto& gs = gsEnt.get<GameStatus>();
        if (gs.gameOver || gs.won) return;

        static const Mask playerMask  = MaskBuilder().set<PlayerTag>().set<Transform>().set<Drawable>().set<Health>().build();
        static const Mask enemyMask   = MaskBuilder().set<EnemyTag>().set<Transform>().set<Drawable>().set<Health>().build();
        static const Mask bulletMask  = MaskBuilder().set<BulletTag>().set<Transform>().set<Drawable>().build();
        static const Mask shelterMask = MaskBuilder().set<Shelter>().set<Transform>().set<Drawable>().set<Health>().build();

        // Decrement invincibility + DamageFlash
        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (!e.test(playerMask)) continue;
            if (e.has<Invincibility>() && e.get<Invincibility>().frames > 0) --e.get<Invincibility>().frames;
            if (e.has<DamageFlash>()   && e.get<DamageFlash>().frames   > 0) --e.get<DamageFlash>().frames;
            break;
        }
        if (gsEnt.has<ScreenShake>() && gsEnt.get<ScreenShake>().frames > 0) --gsEnt.get<ScreenShake>().frames;

        // Full-size AABB — used for bullet×shelter, player-bullet×enemy, enemy-body×player.
        auto overlaps = [](SDL_FPoint p1, SDL_FPoint s1, SDL_FPoint p2, SDL_FPoint s2) {
            return std::abs(p1.x - p2.x) < (s1.x + s2.x) * 0.5f &&
                   std::abs(p1.y - p2.y) < (s1.y + s2.y) * 0.5f;
        };
        // Tight hitbox (75 % of visual) — used for car×player and enemy-bullet×player.
        // Y threshold (60+60)*0.375 = 45 px < TILE=64 px → adjacent lanes never trigger.
        // Car X threshold (124+60)*0.375 = 69 px ≈ 1 tile — only real contact counts.
        auto overlaps_tight = [](SDL_FPoint p1, SDL_FPoint s1, SDL_FPoint p2, SDL_FPoint s2) {
            return std::abs(p1.x - p2.x) < (s1.x + s2.x) * 0.375f &&
                   std::abs(p1.y - p2.y) < (s1.y + s2.y) * 0.375f;
        };

        auto hurt_player = [&](Entity pl) {
            if (pl.get<Invincibility>().frames != 0) return;
            if (--pl.get<Health>().hp <= 0) gs.gameOver = true;
            pl.get<Invincibility>().frames = 90;
            pl.get<DamageFlash>().frames   = 22;
            gsEnt.get<ScreenShake>().frames = 8;
            Entity::create().add(SoundEvent{3});
        };

        auto kill_enemy = [&](Entity en) {
            const SDL_FPoint pos = en.get<Transform>().p;
            const float sz       = en.get<Drawable>().size.x;
            en.destroy();
            gs.kills++;

            // Explosion particle
            Entity::create().addAll(Transform{pos, 0.f}, Explosion{12, 12, sz});

            // Floating score text
            {
                auto& cs = gsEnt.get<ComboState>();
                cs.count      = (cs.timer > 0) ? cs.count + 1 : 1;
                cs.timer      = 60;
                cs.multiplier = std::min(cs.count, 5);
                const int pts = 10 * _current_level * cs.multiplier;
                _score += pts;
                Entity::create().addAll(
                    Transform{{pos.x, pos.y - 16.f}, 0.f},
                    FloatingText{50, 50, 10 * _current_level, cs.multiplier});
            }

            // Pickup drop (~50% chance, not for boss)
            if (sz < TILE * 2.f && std::rand() % 2 == 0) {
                const int type     = std::rand() % 4;   // 0-3 (includes spread shot)
                const int dropLane = 1 + std::rand() % 7;
                const float dropY  = WIN_H - TILE / 2.f - dropLane * TILE;
                Entity::create().addAll(Transform{{pos.x, dropY}, 0.f},
                                       Drawable{{0,0,0,0},{28.f, 28.f}}, Pickup{type});
            }
            Entity::create().add(SoundEvent{2});
        };

        // Bullet × shelter
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

        // Player bullet × enemy
        for (Entity b = Entity::first(); !b.eof(); b.next()) {
            if (!b.test(bulletMask) || !b.get<BulletTag>().fromPlayer) continue;
            const SDL_FPoint bp = b.get<Transform>().p;
            const SDL_FPoint bs = b.get<Drawable>().size;
            for (Entity en = Entity::first(); !en.eof(); en.next()) {
                if (!en.test(enemyMask)) continue;
                if (!overlaps(bp, bs, en.get<Transform>().p, en.get<Drawable>().size)) continue;
                b.destroy();
                gs.hits++;
                if (--en.get<Health>().hp <= 0) kill_enemy(en);
                else Entity::create().add(SoundEvent{1});
                break;
            }
        }

        // Enemy bullet × player  (tight hitbox — near-miss bullets should feel like misses)
        for (Entity b = Entity::first(); !b.eof(); b.next()) {
            if (!b.test(bulletMask) || b.get<BulletTag>().fromPlayer) continue;
            const SDL_FPoint bp = b.get<Transform>().p;
            const SDL_FPoint bs = b.get<Drawable>().size;
            for (Entity pl = Entity::first(); !pl.eof(); pl.next()) {
                if (!pl.test(playerMask)) continue;
                if (!overlaps_tight(bp, bs, pl.get<Transform>().p, pl.get<Drawable>().size)) continue;
                if (pl.get<Shield>().timer > 0.f) { b.destroy(); break; }
                b.destroy();
                hurt_player(pl);
                break;
            }
        }

        // Hazard × player  (tight hitbox — car must really be on your tile)
        static const Mask hazardMask = MaskBuilder().set<Hazard>().set<Transform>().set<Drawable>().build();
        for (Entity h = Entity::first(); !h.eof(); h.next()) {
            if (!h.test(hazardMask)) continue;
            for (Entity pl = Entity::first(); !pl.eof(); pl.next()) {
                if (!pl.test(playerMask)) continue;
                if (!overlaps_tight(h.get<Transform>().p, h.get<Drawable>().size,
                                    pl.get<Transform>().p, pl.get<Drawable>().size)) continue;
                hurt_player(pl);
                break;
            }
        }

        // Enemy body × player
        for (Entity en = Entity::first(); !en.eof(); en.next()) {
            if (!en.test(enemyMask)) continue;
            for (Entity pl = Entity::first(); !pl.eof(); pl.next()) {
                if (!pl.test(playerMask)) continue;
                if (!overlaps(en.get<Transform>().p, en.get<Drawable>().size,
                              pl.get<Transform>().p, pl.get<Drawable>().size)) continue;
                gs.gameOver = true; return;
            }
        }

        // Camera-scroll death (levels 1-3)
        if (_current_level != 4) {
            for (Entity pl = Entity::first(); !pl.eof(); pl.next()) {
                if (!pl.test(playerMask)) continue;
                if (pl.get<Invincibility>().frames > 0) break;  // invulnerable during transition
                const SDL_FPoint& pPos = pl.get<Transform>().p;
                if (pPos.y + _camera_scroll + (pPos.x - WIN_W * 0.5f) * TILT > WIN_H + TILE / 2.f) {
                    gs.gameOver = true; return;
                }
                break;
            }
        }

        // Level 4: game over if any enemy reaches lane >= 8 (player zone)
        if (_current_level == 4) {
            static const Mask lpEMask = MaskBuilder().set<EnemyTag>().set<LanePos>().build();
            for (Entity en = Entity::first(); !en.eof(); en.next()) {
                if (!en.test(lpEMask)) continue;
                if (en.get<LanePos>().lane >= 8) { gs.gameOver = true; return; }
            }
        }

        // Win condition
        bool anyEnemy = false;
        for (Entity e = Entity::first(); !e.eof(); e.next())
            if (e.test(enemyMask)) { anyEnemy = true; break; }
        if (!anyEnemy) { gs.won = true; gs.waveEndTime = SDL_GetTicks(); }
    }

    // ────────────────────────────────── Draw ─────────────────────────────────────
    void Game::draw_system() const
    {
        // Screen shake
        {
            static const Mask shMask = MaskBuilder().set<ScreenShake>().build();
            int ox = 0, oy = 0;
            for (Entity e = Entity::first(); !e.eof(); e.next()) {
                if (!e.test(shMask)) continue;
                if (e.get<ScreenShake>().frames > 0) {
                    ox = (std::rand() % 9) - 4; oy = (std::rand() % 9) - 4;
                }
                break;
            }
            if (ox || oy) {
                SDL_Rect vp = {ox, oy, WIN_W, WIN_H};
                SDL_SetRenderViewport(ren, &vp);
            }
        }

        draw_background();

        // Compute minimum enemy lane for per-row depth tinting
        static const Mask em2 = MaskBuilder().set<EnemyTag>().set<LanePos>().build();
        int minEnemyLane = 99, maxEnemyLane = -1;
        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (!e.test(em2)) continue;
            int lane = e.get<LanePos>().lane;
            if (lane < minEnemyLane) minEnemyLane = lane;
            if (lane > maxEnemyLane) maxEnemyLane = lane;
        }

        static const Mask drawMask     = MaskBuilder().set<Transform>().set<Drawable>().build();
        static const Mask playerMask   = MaskBuilder().set<PlayerTag>().set<Shield>().build();
        static const Mask bossMask     = MaskBuilder().set<BossTag>().set<Health>().build();
        static const Mask enemyMask    = MaskBuilder().set<EnemyTag>().set<Health>().build();
        static const Mask bulletMask   = MaskBuilder().set<BulletTag>().build();
        static const Mask shelterDMsk  = MaskBuilder().set<Shelter>().set<Health>().build();
        static const Mask hazardVisMsk = MaskBuilder().set<Hazard>().set<HazardVisual>().build();
        static const Mask explMask     = MaskBuilder().set<Explosion>().set<Transform>().build();
        static const Mask pickupMask   = MaskBuilder().set<Pickup>().set<Transform>().build();
        static const Mask ftMask       = MaskBuilder().set<FloatingText>().set<Transform>().build();
        static const Mask lpEMask2     = MaskBuilder().set<EnemyTag>().set<LanePos>().set<Health>().build();
        static const Mask worldMask    = MaskBuilder().set<Transform>().build();

        // Diagonal camera: horizontal offset coupled to vertical camera scroll
        const float camX = _camera_scroll * TILT;

        // Build depth-sorted draw list (back-to-front: smaller world Y = farther = drawn first)
        static std::vector<Entity> drawList;
        drawList.clear();
        for (Entity e = Entity::first(); !e.eof(); e.next())
            if (e.test(worldMask)) drawList.push_back(e);
        std::sort(drawList.begin(), drawList.end(),
            [](const Entity& a, const Entity& b) {
                const SDL_FPoint pa = a.get<Transform>().p;
                const SDL_FPoint pb = b.get<Transform>().p;
                const float ya = pa.y + (pa.x - WIN_W * 0.5f) * TILT;
                const float yb = pb.y + (pb.x - WIN_W * 0.5f) * TILT;
                return ya < yb;
            });

        SDL_SetRenderScale(ren, 1.f, 1.f);
        for (Entity e : drawList) {
            // FloatingText pop-ups
            if (e.test(ftMask)) {
                const auto& ft = e.get<FloatingText>();
                const SDL_FPoint p = e.get<Transform>().p;
                const float ftXAdj  = -(p.y - WIN_H + TILE * 0.5f) * TILT;
                const float ftScrX  = p.x + ftXAdj - camX;
                const float screenY = p.y + _camera_scroll + (ftScrX - WIN_W * 0.5f) * TILT;
                if (screenY > -20.f && screenY < WIN_H + 20.f) {
                    SDL_SetRenderScale(ren, 1.5f, 1.5f);
                    SDL_SetRenderDrawColor(ren, 255, 220, 30, 255);
                    char tbuf[16];
                    SDL_snprintf(tbuf, sizeof(tbuf), "+%d", ft.value * ft.mult);
                    SDL_RenderDebugText(ren, (ftScrX - 12.f) / 1.5f, screenY / 1.5f, tbuf);
                    SDL_SetRenderScale(ren, 1.f, 1.f);
                }
                continue;
            }

            // Explosions
            if (e.test(explMask)) {
                const auto& ex = e.get<Explosion>();
                const SDL_FPoint p = e.get<Transform>().p;
                float progress = 1.0f - (float)ex.frames / ex.maxFrames;
                float size     = ex.startSize * (1.0f + progress * 1.8f);
                Uint8 alpha    = (Uint8)(255 * (1.0f - progress));
                const float exXAdj  = -(p.y - WIN_H + TILE * 0.5f) * TILT;
                const float exScrX  = p.x + exXAdj - camX;
                const float exTiltOff = (exScrX - WIN_W * 0.5f) * TILT;
                SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(ren, 255, (Uint8)(200 - (Uint8)(100*progress)), 30, alpha);
                const float szH = size * ISO_SCALE;
                SDL_FRect er = {exScrX - size/2, p.y - szH/2 + _camera_scroll + exTiltOff, size, szH};
                SDL_RenderFillRect(ren, &er);
                SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
                continue;
            }

            // Pickups
            if (e.test(pickupMask)) {
                const int type = e.get<Pickup>().type;
                const SDL_FPoint p = e.get<Transform>().p;
                constexpr float PICK_SZ = 30.f;
                const float sqPH = PICK_SZ * ISO_SCALE;
                const float pkXAdj      = -(p.y - WIN_H + TILE * 0.5f) * TILT;
                const float pkScrX      = p.x + pkXAdj - camX;
                const float pickTiltOff = (pkScrX - WIN_W * 0.5f) * TILT;
                const SDL_FRect pr = {pkScrX - PICK_SZ/2.f, p.y - sqPH/2.f + _camera_scroll + pickTiltOff, PICK_SZ, sqPH};

                if (type == 2 && hearts_texture) {
                    float tw = 0, th = 0; SDL_GetTextureSize(hearts_texture, &tw, &th);
                    SDL_FRect srcHeart = {0.f, 0.f, tw/3.f, th/3.f};
                    SDL_RenderTexture(ren, hearts_texture, &srcHeart, &pr);
                } else if (type == 1 && iron_dome_icon_texture) {
                    SDL_RenderTexture(ren, iron_dome_icon_texture, nullptr, &pr);
                } else {
                    // Colour-coded background square
                    if      (type == 0) SDL_SetRenderDrawColor(ren, 255, 220,  30, 255); // rapid-fire: yellow
                    else if (type == 1) SDL_SetRenderDrawColor(ren,  80, 160, 255, 255); // shield: blue
                    else if (type == 2) SDL_SetRenderDrawColor(ren, 220,  60,  60, 255); // health: red
                    else                SDL_SetRenderDrawColor(ren, 160,  60, 220, 255); // spread: purple
                    SDL_RenderFillRect(ren, &pr);
                }
                // Pickup label
                SDL_SetRenderScale(ren, 1.5f, 1.5f);
                SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
                const char* labels[] = {"R", "", "", "W"};
                SDL_RenderDebugText(ren, (pkScrX - 3.f) / 1.5f, (p.y + _camera_scroll + pickTiltOff - 4.f) / 1.5f, labels[type]);
                SDL_SetRenderScale(ren, 1.f, 1.f);
                continue;
            }

            if (!e.test(drawMask)) continue;

            const auto& t = e.get<Transform>();
            const auto& d = e.get<Drawable>();
            // Isometric X shift: entities at higher lanes shift left on screen
            const float xAdj    = -(t.p.y - WIN_H + TILE * 0.5f) * TILT;
            const float screenX = t.p.x + xAdj - camX;
            // Hop visual offset — Y arc + diagonal X drift when moving forward/back
            float hopOffY = 0.f, hopOffX = 0.f;
            if (e.has<HopState>() && e.get<HopState>().frames > 0) {
                const auto& hop = e.get<HopState>();
                const float sinT = std::sin(M_PI * (float)hop.frames / hop.maxFrames);
                hopOffY = sinT * HOP_HEIGHT;
                hopOffX = sinT * HOP_HEIGHT * TILT * hop.hopDX;
            }
            const float tiltOff = (screenX - WIN_W * 0.5f) * TILT;
            const float sqH = d.size.y * ISO_SCALE;

            // Breathing / wobble animation — driven entirely by BreatheState component.
            // animate_system() increments phase each frame; draw_system only reads it.
            float breatheY = 0.f;
            if (e.has<BreatheState>()) {
                const auto& bs = e.get<BreatheState>();
                breatheY = std::sin(bs.phase) * bs.amplitude;
                // Hop takes over vertical position while the player is mid-air
                if (hopOffY > 0.f) breatheY = 0.f;
            }

            SDL_FRect dest = {screenX - d.size.x/2 - hopOffX, t.p.y - sqH/2 + _camera_scroll + tiltOff - hopOffY - breatheY, d.size.x, sqH};

            // Iron Dome shield halo
            if (e.test(playerMask) && e.get<Shield>().timer > 0.f) {
                constexpr float DOME_W = Game::TILE * 2.1f;
                constexpr float DOME_H = DOME_W * (369.f / 676.f) * ISO_SCALE;
                const SDL_FRect domeRect = {screenX - DOME_W/2.f - hopOffX, t.p.y - DOME_H/2.f + _camera_scroll + tiltOff - hopOffY, DOME_W, DOME_H};
                if (iron_dome_texture) {
                    const float phase = e.get<Shield>().timer / SHIELD_DURATION;
                    const Uint8 alpha = (Uint8)(180 + 60 * std::abs(std::sin(phase * 12.f)));
                    SDL_SetTextureAlphaMod(iron_dome_texture, alpha);
                    SDL_RenderTexture(ren, iron_dome_texture, nullptr, &domeRect);
                    SDL_SetTextureAlphaMod(iron_dome_texture, 255);
                } else {
                    SDL_SetRenderDrawColor(ren, 80, 140, 255, 255);
                    SDL_RenderFillRect(ren, &domeRect);
                }
            }

            if (e.test(playerMask)) {
                const int hp       = e.has<Health>()      ? e.get<Health>().hp      : 3;
                const int dfFrames = e.has<DamageFlash>() ? e.get<DamageFlash>().frames : 0;
                // Sprite blinks every 3 frames while DamageFlash is active
                const bool spriteOn = !(dfFrames > 0 && (dfFrames / 3) % 2 == 1);
                if (spriteOn) {
                    if (player_texture) {
                        if (hp == 1) SDL_SetTextureColorMod(player_texture, 255, 80, 80);
                        SDL_RenderTexture(ren, player_texture, nullptr, &dest);
                        if (hp == 1) SDL_SetTextureColorMod(player_texture, 255, 255, 255);
                    } else {
                        SDL_SetRenderDrawColor(ren, hp == 1 ? 255 : 220, hp == 1 ? 60 : 200, 50, 255);
                        SDL_RenderFillRect(ren, &dest);
                    }
                }
            } else if (e.test(bulletMask)) {
                const bool fp = e.get<BulletTag>().fromPlayer;
                SDL_SetRenderDrawColor(ren, 255, fp ? 255 : 140, 0, 255);
                SDL_RenderFillRect(ren, &dest);
            } else if (e.test(shelterDMsk)) {
                const Uint8 v = (Uint8)(80 + e.get<Health>().hp * 35);
                if (shelter_texture) {
                    SDL_SetTextureColorMod(shelter_texture, v, v, v);
                    SDL_RenderTexture(ren, shelter_texture, nullptr, &dest);
                    SDL_SetTextureColorMod(shelter_texture, 255, 255, 255);
                } else {
                    SDL_SetRenderDrawColor(ren, v, v, v, 255); SDL_RenderFillRect(ren, &dest);
                }
            } else if (e.test(hazardVisMsk)) {
                const int idx = e.get<HazardVisual>().tex_index;
                const float cx = screenX;
                // Driving-over-bumps wobble: read from BreatheState, updated by animate_system
                const float wobble = e.has<BreatheState>()
                    ? std::sin(e.get<BreatheState>().phase) * e.get<BreatheState>().amplitude
                    : 0.f;
                const float cy = t.p.y + _camera_scroll + tiltOff - wobble;
                const float hw = d.size.x * 0.5f;
                const float hh = sqH * 0.5f;
                const float sh = hw * TILT;
                if (haz_textures[idx]) {
                    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
                    const SDL_FColor wh = {1.f, 1.f, 1.f, 1.f};
                    SDL_Vertex v[4] = {
                        {{cx - hw, cy - hh - sh}, wh, {0.f, 0.f}},
                        {{cx + hw, cy - hh + sh}, wh, {1.f, 0.f}},
                        {{cx + hw, cy + hh + sh}, wh, {1.f, 1.f}},
                        {{cx - hw, cy + hh - sh}, wh, {0.f, 1.f}},
                    };
                    const int hidx[6] = {0,1,2, 0,2,3};
                    SDL_RenderGeometry(ren, haz_textures[idx], v, 4, hidx, 6);
                    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
                } else { SDL_SetRenderDrawColor(ren, 0, 200, 220, 255); SDL_RenderFillRect(ren, &dest); }
            } else if (e.test(bossMask)) {
                const int hp = e.get<Health>().hp;
                const Uint8 g = (Uint8)(255 * hp / BOSS_HP);
                if (boss_texture) {
                    SDL_SetTextureColorMod(boss_texture, 255, g, g);
                    SDL_RenderTexture(ren, boss_texture, nullptr, &dest);
                    SDL_SetTextureColorMod(boss_texture, 255, 255, 255);
                } else {
                    SDL_SetRenderDrawColor(ren, 200, 0, 0, 255); SDL_RenderFillRect(ren, &dest);
                }
            } else if (e.test(lpEMask2)) {
                // Regular enemy: per-row depth tinting (further back = cooler/bluer)
                const int hp    = e.get<Health>().hp;
                const int maxHp = (_current_level == 4) ? 3 : _current_level;
                const int depth = e.has<LanePos>()
                    ? std::abs(e.get<LanePos>().lane - (_current_level == 4 ? maxEnemyLane : minEnemyLane))
                    : 0;
                Uint8 r = (Uint8)std::max(120, 255 - depth * 55);
                Uint8 g = (Uint8)std::max(100, 255 - depth * 45);
                Uint8 b = 255;
                if (hp < maxHp) { r = 255; g = 80; b = 80; } // HP-damage tint overrides
                if (enemy_texture) {
                    SDL_SetTextureColorMod(enemy_texture, r, g, b);
                    SDL_RenderTexture(ren, enemy_texture, nullptr, &dest);
                    SDL_SetTextureColorMod(enemy_texture, 255, 255, 255);
                } else {
                    SDL_SetRenderDrawColor(ren, r, 50, b, 255); SDL_RenderFillRect(ren, &dest);
                }
            } else if (e.test(enemyMask)) {
                // Fallback for any enemy without LanePos (shouldn't happen)
                if (enemy_texture) SDL_RenderTexture(ren, enemy_texture, nullptr, &dest);
                else { SDL_SetRenderDrawColor(ren, 220, 50, 50, 255); SDL_RenderFillRect(ren, &dest); }
            }
        }

        // Reset viewport before HUD
        SDL_SetRenderViewport(ren, nullptr);

        // Off-screen enemy warning arrows
        {
            static const Mask warnMask = MaskBuilder().set<EnemyTag>().set<Transform>().build();
            for (Entity e = Entity::first(); !e.eof(); e.next()) {
                if (!e.test(warnMask)) continue;
                const SDL_FPoint p  = e.get<Transform>().p;
                const float arXAdj  = -(p.y - WIN_H + TILE * 0.5f) * TILT;
                const float arScrX  = p.x + arXAdj - camX;
                const float screenY = p.y + _camera_scroll + (arScrX - WIN_W * 0.5f) * TILT;
                if (screenY >= 0.f) continue;
                const float ax = std::clamp(arScrX, 12.f, (float)WIN_W - 12.f);
                SDL_Vertex verts[3] = {
                    {{ax,         8.f}, {255, 255, 0, 255}, {0,0}},
                    {{ax - 9.f, 24.f}, {255, 200, 0, 255}, {0,0}},
                    {{ax + 9.f, 24.f}, {255, 200, 0, 255}, {0,0}},
                };
                SDL_RenderGeometry(ren, nullptr, verts, 3, nullptr, 0);
            }
        }

        // Damage flash
        {
            static const Mask dfMask = MaskBuilder().set<PlayerTag>().set<DamageFlash>().build();
            for (Entity e = Entity::first(); !e.eof(); e.next()) {
                if (!e.test(dfMask)) continue;
                const int fr = e.get<DamageFlash>().frames;
                if (fr > 0) {
                    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
                    SDL_SetRenderDrawColor(ren, 220, 20, 20, (Uint8)(130 * fr / 22));
                    SDL_FRect flashRect = {0, 0, (float)WIN_W, (float)WIN_H};
                    SDL_RenderFillRect(ren, &flashRect);
                    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
                }
                break;
            }
        }

        // SlowMo overlay tint
        {
            static const Mask smMask = MaskBuilder().set<SlowMo>().build();
            for (Entity e = Entity::first(); !e.eof(); e.next()) {
                if (!e.test(smMask)) continue;
                const auto& sm = e.get<SlowMo>();
                if (sm.frames > 0) {
                    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
                    SDL_SetRenderDrawColor(ren, 30, 60, 160, 35);
                    SDL_FRect overlay = {0, 0, (float)WIN_W, (float)WIN_H};
                    SDL_RenderFillRect(ren, &overlay);
                    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
                }
                break;
            }
        }

        // HUD: HP + Shield icons
        static const Mask hpMask = MaskBuilder().set<PlayerTag>().set<Health>().set<Shield>().set<BlinkPhase>().build();
        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (!e.test(hpMask)) continue;
            const int hp   = e.get<Health>().hp;
            const auto& sh = e.get<Shield>();

            if (hearts_texture) {
                float tw = 0, th = 0; SDL_GetTextureSize(hearts_texture, &tw, &th);
                const float cw = tw/3.f, ch = th/3.f;
                const SDL_FRect srcFull  = {0.f, 0.f, cw, ch};
                const SDL_FRect srcEmpty = {2.f*cw, ch, cw, ch};
                const int dfFrames = e.has<DamageFlash>() ? e.get<DamageFlash>().frames : 0;
                for (int i = 0; i < 3; i++) {
                    const SDL_FRect dst = {8.f + i*30.f, 6.f, 26.f, 26.f};
                    // Flash the just-lost slot: blink it on/off while DamageFlash is active
                    if (i == hp && dfFrames > 0) {
                        if ((dfFrames / 4) % 2 == 0) {
                            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
                            SDL_SetTextureAlphaMod(hearts_texture, 200);
                            SDL_RenderTexture(ren, hearts_texture, &srcFull, &dst);
                            SDL_SetTextureAlphaMod(hearts_texture, 255);
                            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
                        }
                    } else {
                        SDL_RenderTexture(ren, hearts_texture, i < hp ? &srcFull : &srcEmpty, &dst);
                    }
                }
            } else {
                for (int i = 0; i < 5; i++) {
                    SDL_FRect icon = {10.f + i*26.f, 10.f, 20.f, 20.f};
                    SDL_SetRenderDrawColor(ren, i<hp?0:55, i<hp?220:55, i<hp?80:55, 255);
                    SDL_RenderFillRect(ren, &icon);
                }
            }
            const bool blinkOn = e.get<BlinkPhase>().visible;
            const int sfFrames = e.has<ShieldFlash>() ? e.get<ShieldFlash>().frames : 0;
            for (int i = 0; i < 3; i++) {
                const bool active = (i == 0 && sh.timer > 0.f);
                const bool avail  = (i < sh.charges) || active;
                const SDL_FRect icon = {150.f + i*32.f, 4.f, 28.f, 25.f};
                // Flash the just-used slot before it goes dark
                const bool flashing = (sfFrames > 0 && i == sh.charges && !avail);
                if (iron_dome_icon_texture) {
                    Uint8 alpha = avail ? (active && !blinkOn ? 120 : 255)
                                       : (flashing && (sfFrames / 4) % 2 == 0 ? 220 : 60);
                    SDL_SetTextureAlphaMod(iron_dome_icon_texture, alpha);
                    SDL_RenderTexture(ren, iron_dome_icon_texture, nullptr, &icon);
                    SDL_SetTextureAlphaMod(iron_dome_icon_texture, 255);
                } else {
                    SDL_SetRenderDrawColor(ren, avail?80:55, avail?140:55, avail?255:55, 255);
                    SDL_RenderFillRect(ren, &icon);
                }
            }

            // Power-up timer bars near player sprite
            const SDL_FPoint p = e.get<Transform>().p;
            const float hudXAdj   = -(p.y - WIN_H + TILE * 0.5f) * TILT;
            const float hudScrX   = p.x + hudXAdj - camX;
            float hudHopOff = 0.f, hudHopOffX = 0.f;
            if (e.has<HopState>() && e.get<HopState>().frames > 0) {
                const auto& hop = e.get<HopState>();
                const float sinT = std::sin(M_PI * (float)hop.frames / hop.maxFrames);
                hudHopOff  = sinT * HOP_HEIGHT;
                hudHopOffX = sinT * HOP_HEIGHT * TILT * hop.hopDX;
            }
            const float screenY = p.y + _camera_scroll + (hudScrX - WIN_W * 0.5f) * TILT - hudHopOff;
            const float barW    = 48.f;
            const float barX    = hudScrX - barW / 2.f - hudHopOffX;
            float barY          = screenY + 30.f;

            const auto& rf = e.get<RapidFire>();
            if (rf.frames > 0) {
                SDL_SetRenderDrawColor(ren, 60, 50, 10, 255);
                SDL_FRect bgr = {barX, barY, barW, 4.f}; SDL_RenderFillRect(ren, &bgr);
                if (blinkOn) {
                    SDL_SetRenderDrawColor(ren, 255, 220, 30, 255);
                    SDL_FRect fill = {barX, barY, barW * std::min((float)rf.frames/300.f,1.f), 4.f};
                    SDL_RenderFillRect(ren, &fill);
                }
                barY += 6.f;
            }
            if (e.has<SpreadShot>() && e.get<SpreadShot>().frames > 0) {
                const int ssFrames = e.get<SpreadShot>().frames;
                SDL_SetRenderDrawColor(ren, 40, 20, 60, 255);
                SDL_FRect bgr = {barX, barY, barW, 4.f}; SDL_RenderFillRect(ren, &bgr);
                if (blinkOn) {
                    SDL_SetRenderDrawColor(ren, 160, 60, 255, 255);
                    SDL_FRect fill = {barX, barY, barW * std::min((float)ssFrames/300.f,1.f), 4.f};
                    SDL_RenderFillRect(ren, &fill);
                }
            }

            // Dash cooldown indicator (small cyan arc-like bar above player)
            if (e.has<DashState>()) {
                const auto& ds = e.get<DashState>();
                if (ds.cooldown > 0) {
                    SDL_SetRenderDrawColor(ren, 0, 180, 220, 255);
                    const float dashW = barW * (1.f - (float)ds.cooldown / 60.f);
                    SDL_FRect dbg = {barX, screenY - 36.f, barW, 3.f};
                    SDL_SetRenderDrawColor(ren, 20, 60, 70, 255);
                    SDL_RenderFillRect(ren, &dbg);
                    SDL_SetRenderDrawColor(ren, 0, 200, 255, 255);
                    SDL_FRect dfill = {barX, screenY - 36.f, dashW, 3.f};
                    SDL_RenderFillRect(ren, &dfill);
                }
            }
            break;
        }

        // SlowMo timer bar (top-right)
        {
            static const Mask smMask2 = MaskBuilder().set<SlowMo>().build();
            for (Entity e = Entity::first(); !e.eof(); e.next()) {
                if (!e.test(smMask2)) continue;
                const auto& sm = e.get<SlowMo>();
                if (sm.frames > 0) {
                    // look up BlinkPhase from the player entity for sync'd blink
                    static const Mask plBlink = MaskBuilder().set<PlayerTag>().set<BlinkPhase>().build();
                    bool smBlink = true;
                    for (Entity p = Entity::first(); !p.eof(); p.next()) {
                        if (!p.test(plBlink)) continue;
                        smBlink = p.get<BlinkPhase>().visible;
                        break;
                    }
                    const float smW = 120.f * sm.frames / 180.f;
                    SDL_SetRenderDrawColor(ren, 20, 40, 90, 255);
                    SDL_FRect bg = {(float)WIN_W - 130.f, 8.f, 120.f, 10.f};
                    SDL_RenderFillRect(ren, &bg);
                    if (smBlink) {
                        SDL_SetRenderDrawColor(ren, 80, 160, 255, 255);
                        SDL_FRect bar = {(float)WIN_W - 130.f, 8.f, smW, 10.f};
                        SDL_RenderFillRect(ren, &bar);
                        SDL_SetRenderScale(ren, 1.5f, 1.5f);
                        SDL_SetRenderDrawColor(ren, 140, 200, 255, 255);
                        SDL_RenderDebugText(ren, ((float)WIN_W - 130.f) / 1.5f, 20.f / 1.5f, "SLOW");
                        SDL_SetRenderScale(ren, 1.f, 1.f);
                    }
                }
                break;
            }
        }

        // Combo display
        {
            static const Mask comboMask = MaskBuilder().set<ComboState>().build();
            for (Entity e = Entity::first(); !e.eof(); e.next()) {
                if (!e.test(comboMask)) continue;
                const auto& cs = e.get<ComboState>();
                if (cs.count >= 2 && cs.timer > 0) {
                    SDL_SetRenderScale(ren, 2.5f, 2.5f);
                    SDL_SetRenderDrawColor(ren, 255, 200, 30, 255);
                    char buf[16]; SDL_snprintf(buf, sizeof(buf), "x%d COMBO!", cs.count);
                    SDL_RenderDebugText(ren, (WIN_W / 2.5f - 50.f), 60.f, buf);
                    SDL_SetRenderScale(ren, 1.f, 1.f);
                }
                break;
            }
        }

        // Global status text + score
        // Shows active powerup name (coloured) or falls back to "LVL X"
        {
            static const Mask pStatusMask = MaskBuilder().set<PlayerTag>().set<RapidFire>().set<SpreadShot>().build();
            static const Mask smStatusMask = MaskBuilder().set<SlowMo>().build();

            const char* statusTxt = nullptr;
            Uint8 sr = 200, sg = 200, sb = 200;
            for (Entity e = Entity::first(); !e.eof(); e.next()) {
                if (!e.test(pStatusMask)) continue;
                if (e.get<RapidFire>().frames > 0)                           { statusTxt = "RAPIDFIRE";  sr=255; sg=220; sb=30;  break; }
                if (e.has<SpreadShot>() && e.get<SpreadShot>().frames > 0)   { statusTxt = "SPREADSHOT"; sr=160; sg=60;  sb=255; break; }
                break;
            }
            if (!statusTxt) {
                for (Entity e = Entity::first(); !e.eof(); e.next()) {
                    if (!e.test(smStatusMask)) continue;
                    if (e.get<SlowMo>().frames > 0) { statusTxt = "SLOW-MO"; sr=80; sg=160; sb=255; }
                    break;
                }
            }

            char buf[48];
            SDL_SetRenderScale(ren, 2.f, 2.f);
            if (statusTxt) {
                SDL_SetRenderDrawColor(ren, sr, sg, sb, 255);
                SDL_RenderDebugText(ren, 5.f, 20.f, statusTxt);
            } else {
                SDL_SetRenderDrawColor(ren, 200, 200, 200, 255);
                SDL_snprintf(buf, sizeof(buf), "LVL %d", _current_level);
                SDL_RenderDebugText(ren, 5.f, 20.f, buf);
            }
            SDL_SetRenderDrawColor(ren, 200, 200, 200, 255);
            SDL_snprintf(buf, sizeof(buf), "SCORE %05d", _score);
            SDL_RenderDebugText(ren, 216.f, 5.f, buf);
            SDL_SetRenderScale(ren, 1.f, 1.f);
        }

        // Difficulty badge (small)
        {
            static const Mask ssMask = MaskBuilder().set<SelectState>().build();
            for (Entity e = Entity::first(); !e.eof(); e.next()) {
                if (!e.test(ssMask)) continue;
                const char* dlabel = e.get<SelectState>().difficulty == 0 ? "EZ" :
                                     e.get<SelectState>().difficulty == 2 ? "HD" : "NM";
                SDL_SetRenderScale(ren, 1.5f, 1.5f);
                SDL_SetRenderDrawColor(ren, 120, 120, 120, 255);
                SDL_RenderDebugText(ren, 4.f, 6.f, dlabel);
                SDL_SetRenderScale(ren, 1.f, 1.f);
                break;
            }
        }

        // Boss health bar (level 3)
        if (_current_level == 3) {
            static const Mask bossHpMask = MaskBuilder().set<EnemyTag>().set<Health>().build();
            for (Entity e = Entity::first(); !e.eof(); e.next()) {
                if (!e.test(bossHpMask)) continue;
                const int hp = e.get<Health>().hp;
                const float bw = WIN_W * 0.55f, bx = (WIN_W - bw) / 2.f;
                SDL_SetRenderDrawColor(ren, 70, 0, 0, 255);
                SDL_FRect barBg = {bx, 46.f, bw, 12.f}; SDL_RenderFillRect(ren, &barBg);
                SDL_SetRenderDrawColor(ren, 230, 50, 50, 255);
                SDL_FRect barFill = {bx, 46.f, bw * hp / (float)BOSS_HP, 12.f};
                SDL_RenderFillRect(ren, &barFill);
                SDL_SetRenderScale(ren, 1.5f, 1.5f);
                SDL_SetRenderDrawColor(ren, 255, 180, 180, 255);
                SDL_RenderDebugText(ren, (bx - 30.f) / 1.5f, 30.f, "BOSS");
                SDL_SetRenderScale(ren, 1.f, 1.f);
                break;
            }
        }

        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    }

    void Game::draw_pause() const
    {
        // Starfield background (same as select screen)
        SDL_SetRenderDrawColor(ren, 4, 4, 18, 255);
        { SDL_FRect full = {0, 0, (float)WIN_W, (float)WIN_H}; SDL_RenderFillRect(ren, &full); }
        SDL_SetRenderDrawColor(ren, 60, 60, 110, 255);
        for (int i = 0; i < 80; i++) {
            float sx = (float)((i * 137 + 23) % WIN_W);
            float sy = (float)(((int)(_select_scroll * 0.35f) + i * 89) % WIN_H);
            SDL_FRect star = {sx, sy, 1.f, 1.f}; SDL_RenderFillRect(ren, &star);
        }
        SDL_SetRenderDrawColor(ren, 200, 200, 255, 255);
        for (int i = 0; i < 30; i++) {
            float sx = (float)((i * 317 + 71) % WIN_W);
            float sy = (float)(((int)(_select_scroll * 0.9f) + i * 157) % WIN_H);
            SDL_FRect star = {sx, sy, 2.f, 2.f}; SDL_RenderFillRect(ren, &star);
        }

        // Central card — 480 × 280, centred
        const float cx = (WIN_W - 480.f) / 2.f;   // 272
        const float cy = (WIN_H - 280.f) / 2.f;   // 244
        // Border
        SDL_SetRenderDrawColor(ren, 100, 130, 255, 255);
        SDL_FRect border = {cx - 3.f, cy - 3.f, 486.f, 286.f};
        SDL_RenderFillRect(ren, &border);
        // Card body
        SDL_SetRenderDrawColor(ren, 10, 14, 40, 255);
        SDL_FRect card = {cx, cy, 480.f, 280.f};
        SDL_RenderFillRect(ren, &card);
        // Header strip
        SDL_SetRenderDrawColor(ren, 25, 35, 90, 255);
        SDL_FRect header = {cx, cy, 480.f, 60.f};
        SDL_RenderFillRect(ren, &header);

        // "PAUSED" title (scale 5 → canvas 204.8)
        SDL_SetRenderScale(ren, 5.f, 5.f);
        SDL_SetRenderDrawColor(ren, 160, 190, 255, 255);
        SDL_RenderDebugText(ren, (WIN_W/5.f - 6.f*8.f)/2.f, (cy + 15.f)/5.f, "PAUSED");
        SDL_SetRenderScale(ren, 1.f, 1.f);

        // Divider
        SDL_SetRenderDrawColor(ren, 50, 70, 150, 255);
        SDL_FRect div = {cx + 20.f, cy + 62.f, 440.f, 1.f};
        SDL_RenderFillRect(ren, &div);

        // Level / score info
        SDL_SetRenderScale(ren, 2.f, 2.f);
        SDL_SetRenderDrawColor(ren, 200, 200, 200, 255);
        char buf[32];
        SDL_snprintf(buf, sizeof(buf), "Level %d", _current_level);
        SDL_RenderDebugText(ren, (cx + 30.f)/2.f, (cy + 78.f)/2.f, buf);
        SDL_snprintf(buf, sizeof(buf), "Score %05d", _score);
        SDL_RenderDebugText(ren, (cx + 260.f)/2.f, (cy + 78.f)/2.f, buf);
        SDL_SetRenderScale(ren, 1.f, 1.f);

        // Divider
        SDL_RenderFillRect(ren, &div); // reuse style
        SDL_SetRenderDrawColor(ren, 50, 70, 150, 255);
        SDL_FRect div2 = {cx + 20.f, cy + 108.f, 440.f, 1.f};
        SDL_RenderFillRect(ren, &div2);

        // Controls reminder
        struct CtrlRow { const char* key; const char* action; };
        const CtrlRow rows[] = {
            {"ARROWS",     "Move"},
            {"SPACE",      "Shoot"},
            {"SHIFT+DIR",  "Dash"},
            {"I",          "Iron Dome"},
            {"Q",          "Bullet-Time"},
        };
        SDL_SetRenderScale(ren, 1.5f, 1.5f);
        for (int i = 0; i < 5; i++) {
            SDL_SetRenderDrawColor(ren, 100, 160, 255, 255);
            SDL_RenderDebugText(ren, (cx + 30.f)/1.5f, (cy + 120.f + i*22.f)/1.5f, rows[i].key);
            SDL_SetRenderDrawColor(ren, 140, 140, 180, 255);
            SDL_RenderDebugText(ren, (cx + 200.f)/1.5f, (cy + 120.f + i*22.f)/1.5f, rows[i].action);
        }
        SDL_SetRenderScale(ren, 1.f, 1.f);

        // Resume hint (blinking)
        if ((SDL_GetTicks() / 550) % 2 == 0) {
            SDL_SetRenderScale(ren, 2.f, 2.f);
            SDL_SetRenderDrawColor(ren, 80, 200, 255, 255);
            SDL_RenderDebugText(ren, (cx + 100.f)/2.f, (cy + 248.f)/2.f, "P / ESC  to resume");
            SDL_SetRenderScale(ren, 1.f, 1.f);
        }
    }

    void Game::endgame_draw() const
    {
        static const Mask gsMask = MaskBuilder().set<GameStatus>().build();
        bool gameOver = false;
        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (!e.test(gsMask)) continue;
            gameOver = e.get<GameStatus>().gameOver;
            break;
        }
        // _total_kills accumulates every level; _prev_shots/_prev_hits are from the last level.
        const int kills = _total_kills;
        const int shots = _prev_shots;
        const int hits  = _prev_hits;
        const bool newRecord = (_score > _high_score);
        if (newRecord) { _high_score = _score; save_high_score(); }

        // Starfield background (same as select screen)
        SDL_SetRenderDrawColor(ren, 4, 4, 18, 255);
        { SDL_FRect full = {0, 0, (float)WIN_W, (float)WIN_H}; SDL_RenderFillRect(ren, &full); }
        SDL_SetRenderDrawColor(ren, 60, 60, 110, 255);
        for (int i = 0; i < 80; i++) {
            float sx = (float)((i * 137 + 23) % WIN_W);
            float sy = (float)(((int)(_select_scroll * 0.35f) + i * 89) % WIN_H);
            SDL_FRect star = {sx, sy, 1.f, 1.f}; SDL_RenderFillRect(ren, &star);
        }
        SDL_SetRenderDrawColor(ren, 200, 200, 255, 255);
        for (int i = 0; i < 30; i++) {
            float sx = (float)((i * 317 + 71) % WIN_W);
            float sy = (float)(((int)(_select_scroll * 0.9f) + i * 157) % WIN_H);
            SDL_FRect star = {sx, sy, 2.f, 2.f}; SDL_RenderFillRect(ren, &star);
        }

        // Central card — 620 × 340, centred
        const float cx = (WIN_W - 620.f) / 2.f;   // 202
        const float cy = (WIN_H - 340.f) / 2.f;   // 214
        // Border
        SDL_SetRenderDrawColor(ren, gameOver ? 200 : 50, gameOver ? 40 : 200, gameOver ? 40 : 50, 255);
        SDL_FRect border = {cx - 3.f, cy - 3.f, 626.f, 346.f};
        SDL_RenderFillRect(ren, &border);
        // Card bg
        SDL_SetRenderDrawColor(ren, gameOver ? 35 : 10, gameOver ? 8 : 35, gameOver ? 8 : 10, 255);
        SDL_FRect card = {cx, cy, 620.f, 340.f};
        SDL_RenderFillRect(ren, &card);
        // Header strip
        SDL_SetRenderDrawColor(ren, gameOver ? 90 : 20, gameOver ? 15 : 90, gameOver ? 15 : 20, 255);
        SDL_FRect header = {cx, cy, 620.f, 68.f};
        SDL_RenderFillRect(ren, &header);

        // Big title (scale 5 → each char = 40px wide)
        // "GAME OVER" = 9 chars, 360px.  canvas width at scale5 = 204.8.  x=(204.8-72)/2 = 66.4
        // "YOU WIN!" = 8 chars, 320px.  x=(204.8-64)/2 = 70.4
        SDL_SetRenderScale(ren, 5.f, 5.f);
        if (gameOver) {
            SDL_SetRenderDrawColor(ren, 255, 70, 70, 255);
            SDL_RenderDebugText(ren, (WIN_W/5.f - 9.f*8.f)/2.f, (cy + 14.f)/5.f, "GAME OVER");
        } else {
            SDL_SetRenderDrawColor(ren, 80, 255, 100, 255);
            SDL_RenderDebugText(ren, (WIN_W/5.f - 8.f*8.f)/2.f, (cy + 14.f)/5.f, "YOU WIN!");
        }
        SDL_SetRenderScale(ren, 1.f, 1.f);

        // Divider line inside card
        SDL_SetRenderDrawColor(ren, gameOver ? 90:30, gameOver ? 20:90, gameOver ? 20:30, 255);
        SDL_FRect div = {cx + 20.f, cy + 70.f, 580.f, 1.f};
        SDL_RenderFillRect(ren, &div);

        // Stats row — kills / accuracy / level reached (scale 2, canvas = 512×384)
        const int acc = shots > 0 ? (hits * 100 / shots) : 0;
        char buf[64];
        SDL_SetRenderScale(ren, 2.f, 2.f);
        SDL_SetRenderDrawColor(ren, 180, 220, 180, 255);
        SDL_snprintf(buf, sizeof(buf), "Kills: %d", kills);
        SDL_RenderDebugText(ren, (cx + 30.f)/2.f, (cy + 86.f)/2.f, buf);
        if (shots > 0) SDL_snprintf(buf, sizeof(buf), "Accuracy: %d%%", acc);
        else           SDL_snprintf(buf, sizeof(buf), "Accuracy: --");
        SDL_RenderDebugText(ren, (cx + 210.f)/2.f, (cy + 86.f)/2.f, buf);
        SDL_snprintf(buf, sizeof(buf), "Level: %d", _current_level);
        SDL_RenderDebugText(ren, (cx + 430.f)/2.f, (cy + 86.f)/2.f, buf);
        SDL_SetRenderScale(ren, 1.f, 1.f);

        // Score & best — scale 2.5, canvas = 409.6×307.2
        SDL_SetRenderScale(ren, 2.5f, 2.5f);
        SDL_SetRenderDrawColor(ren, 255, 215, 30, 255);
        SDL_snprintf(buf, sizeof(buf), "SCORE  %05d", _score);
        SDL_RenderDebugText(ren, (WIN_W/2.5f - 11.f*8.f)/2.f, (cy + 132.f)/2.5f, buf);
        SDL_SetRenderScale(ren, 1.f, 1.f);

        SDL_SetRenderScale(ren, 2.f, 2.f);
        SDL_SetRenderDrawColor(ren, 150, 150, 150, 255);
        SDL_snprintf(buf, sizeof(buf), "Best: %05d", _high_score);
        SDL_RenderDebugText(ren, (WIN_W/2.f - 10.f*8.f)/2.f, (cy + 170.f)/2.f, buf);
        SDL_SetRenderScale(ren, 1.f, 1.f);

        // NEW RECORD flash
        if (newRecord && (SDL_GetTicks() / 400) % 2 == 0) {
            SDL_SetRenderScale(ren, 2.5f, 2.5f);
            SDL_SetRenderDrawColor(ren, 255, 180, 0, 255);
            SDL_RenderDebugText(ren, (WIN_W/2.5f - 10.f*8.f)/2.f, (cy + 210.f)/2.5f, "NEW RECORD!");
            SDL_SetRenderScale(ren, 1.f, 1.f);
        }

        // Divider
        SDL_SetRenderDrawColor(ren, gameOver ? 90:30, gameOver ? 20:90, gameOver ? 20:30, 255);
        SDL_FRect div2 = {cx + 20.f, cy + 268.f, 580.f, 1.f};
        SDL_RenderFillRect(ren, &div2);

        // Restart prompt (blinking)
        if ((SDL_GetTicks() / 600) % 2 == 0) {
            SDL_SetRenderScale(ren, 2.f, 2.f);
            SDL_SetRenderDrawColor(ren, gameOver ? 200 : 100, gameOver ? 100 : 200, 100, 255);
            SDL_RenderDebugText(ren, (WIN_W/2.f - 18.f*8.f)/2.f, (cy + 298.f)/2.f, "Press  R  to play again");
            SDL_SetRenderScale(ren, 1.f, 1.f);
        }
    }

    // ─────────────────────────────── Main loop ───────────────────────────────────
    void Game::run()
    {
        auto start = SDL_GetTicks();
        bool quit  = false;

        static const Mask gsMask = MaskBuilder().set<GameStatus>().build();

        while (!quit) {
            bool gameOver = false, won = false;

            if (_state == GameState::Select) {
                select_input();
                _select_scroll += 0.5f;  // animate starfield
                if (_select_scroll > 10000.f) _select_scroll = 0.f;
            } else if (_state == GameState::Playing) {
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
                    explosion_system();
                    pickup_system();
                    combo_system();
                    splash_system();
                    floating_text_system();
                    hop_system();
                    animate_system();
                    blink_system();

                    // Camera scroll (disabled for level 4)
                    if (_current_level != 4) {
                        if (_camera_grace > 0) --_camera_grace;
                        else _camera_scroll += TILE / (5.f * FPS);
                    }

                    for (Entity e = Entity::first(); !e.eof(); e.next())
                        if (e.test(gsMask)) { gameOver = e.get<GameStatus>().gameOver; won = e.get<GameStatus>().won; break; }

                    // Save terminal stats so endgame_draw() always has correct values.
                    // Runs once (next frame the !gameOver&&!won gate stops systems).
                    if (gameOver || (won && _current_level == 4)) {
                        for (Entity e = Entity::first(); !e.eof(); e.next()) {
                            if (!e.test(gsMask)) continue;
                            const auto& gs2 = e.get<GameStatus>();
                            _prev_kills     = gs2.kills;
                            _prev_shots     = gs2.shots;
                            _prev_hits      = gs2.hits;
                            _total_kills   += gs2.kills;   // add the final level's kills
                            _prev_wave_secs = (int)((SDL_GetTicks() - gs2.waveStartTime) / 1000);
                            break;
                        }
                    }

                    if (won && _current_level < 4) {
                        // ── Save stats NOW, before spawn_enemy_wave() zeros them ──
                        for (Entity e = Entity::first(); !e.eof(); e.next()) {
                            if (!e.test(gsMask)) continue;
                            const auto& gs2 = e.get<GameStatus>();
                            _prev_kills     = gs2.kills;
                            _prev_shots     = gs2.shots;
                            _prev_hits      = gs2.hits;
                            _total_kills   += gs2.kills;   // accumulate across levels
                            Uint64 endT     = gs2.waveEndTime > 0 ? gs2.waveEndTime : SDL_GetTicks();
                            _prev_wave_secs = (int)((endT - gs2.waveStartTime) / 1000);
                            break;
                        }
                        _current_level++;
                        clear_enemies_only();
                        spawn_enemy_wave();
                        // Capture scroll baseline so background starts at this level's section.
                        _level_start_scroll = _camera_scroll;

                        // ── Respawn hazards on the new level's actual road lanes ─────────
                        // Pixel analysis confirmed (imageRow = levelBase + worldY, constant):
                        //   Level 2 (Iran)  : lanes 6 & 7 → image rows 807-906
                        //   Level 3 (Israel): lanes 6 & 7 → image rows 190-326
                        //   Level 4         : same lanes (Level 3 background reused)
                        {
                            static const Mask hazMask = MaskBuilder().set<Hazard>().build();
                            for (Entity e = Entity::first(); !e.eof(); e.next())
                                if (e.test(hazMask)) e.destroy();

                            const float spd = 2.5f + (_current_level - 1) * 0.6f;
                            // tex2=Iranian tank (idx 3), tex2=Merkava (idx 2)
                            const int t0 = (_current_level == 2) ? 3 : 2;
                            const int t1 = (_current_level == 2) ? 3 : 1;
                            struct HazInfo { int lane; float dx; int tex; };
                            const HazInfo hz[] = { {7,  spd, t0}, {6, -spd, t1} };
                            for (const auto& h : hz) {
                                const float hy = WIN_H - TILE / 2.f - h.lane * TILE;
                                for (int k = 0; k < 2; k++)
                                    Entity::create().addAll(
                                        Transform{{(float)(std::rand() % WIN_W), hy}, 0.f},
                                        Drawable{{0,0,0,0},{HAZARD_W, HAZARD_H}},
                                        Velocity{h.dx, 0.f}, Hazard{}, HazardVisual{h.tex});
                            }
                        }

                        // fromIdx = city we just left, toIdx = city we're flying to
                        Entity::create().add(MapScreen{420, 0.f, _current_level - 2, _current_level - 1});
                        Entity::create().add(SoundEvent{4}); // level-clear sound (processed this frame)
                        _state = GameState::MapScreen;
                        won = false;
                    }

                    if (_score > _high_score) { _high_score = _score; save_high_score(); }
                }

                // One-shot end-of-game sfx (fires on the first game-over/win frame)
                if (gameOver || (won && _current_level == 4)) {
                    for (Entity e = Entity::first(); !e.eof(); e.next()) {
                        if (!e.test(gsMask)) continue;
                        auto& gs2 = e.get<GameStatus>();
                        if (!gs2.sfxPlayed) {
                            Entity::create().add(SoundEvent{gameOver ? 5 : 4});
                            gs2.sfxPlayed = true;
                        }
                        break;
                    }
                }

                sound_system();
            } else if (_state == GameState::MapScreen) {
                map_screen_system();
            }

            SDL_RenderClear(ren);
            if (_state == GameState::Select) {
                select_draw();
            } else if (_state == GameState::Paused) {
                draw_pause();
            } else if (_state == GameState::MapScreen) {
                draw_map_screen();
            } else {
                draw_system();
                {
                    static const Mask splashMask = MaskBuilder().set<LevelSplash>().build();
                    for (Entity e = Entity::first(); !e.eof(); e.next())
                        if (e.test(splashMask)) { draw_level_splash(); break; }
                }
                if (gameOver || (won && _current_level == 4)) endgame_draw();
            }
            SDL_RenderPresent(ren);

            const auto end = SDL_GetTicks();
            if (end - start < GAME_FRAME) SDL_Delay(GAME_FRAME - (end - start));
            start += GAME_FRAME;

            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_EVENT_QUIT) quit = true;
                if (ev.type == SDL_EVENT_KEY_DOWN) {
                    const SDL_Scancode sc = ev.key.scancode;
                    if (sc == SDL_SCANCODE_ESCAPE) {
                        if      (_state == GameState::Playing) _state = GameState::Paused;
                        else if (_state == GameState::Paused)  _state = GameState::Playing;
                        else                                    quit   = true;
                    }
                    if (sc == SDL_SCANCODE_P) {
                        if      (_state == GameState::Playing) _state = GameState::Paused;
                        else if (_state == GameState::Paused)  _state = GameState::Playing;
                    }
                    if (sc == SDL_SCANCODE_SPACE && !ev.key.repeat && _state == GameState::MapScreen) {
                        static const Mask msMask = MaskBuilder().set<MapScreen>().build();
                        for (Entity e = Entity::first(); !e.eof(); e.next())
                            if (e.test(msMask)) { e.get<MapScreen>().framesLeft = 1; break; }
                    }
                    if (sc == SDL_SCANCODE_SPACE && _state == GameState::Select) {
                        if (audio_stream) SDL_ClearAudioStream(audio_stream); // stop anthem
                        _current_level = 1;
                        spawn_entities();
                        Entity::create().add(MapScreen{420, 1.f, 0, 0}); // plane already at start city
                        _state = GameState::MapScreen;
                    }
                    if (sc == SDL_SCANCODE_R && (gameOver || won)) {
                        _current_level = 1;
                        reset();
                    }
                }
            }
        }
    }
} // namespace ci
