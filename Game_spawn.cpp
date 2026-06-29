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
            "res/yoamashit_pixel.png", "res/stalin_pixel.png", "res/sara_pixel.png"
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
            BlinkPhase{true, 0, 8},
            CharacterID{selected});

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

    void Game::switch_character() const
    {
        static const char* char_files[NUM_CHARS] = {
            "res/trump_pixel.png", "res/bibi_pixel.png", "res/bengvir_pixel.png",
            "res/zelensky_pixel.png", "res/putin_pixel.png", "res/obama_pixel.png",
            "res/eminem_pixel.png", "res/madonna_pixel.png", "res/michaeljackson_pixel.png",
            "res/yoamashit_pixel.png", "res/stalin_pixel.png", "res/sara_pixel.png"
        };
        static const int char_sfx[NUM_CHARS] = {
            8, 7, 9, 10, 11, 18, 12, 13, 14, 17, 16, 15
        };

        // Advance character on player entity
        static const Mask playerMask = MaskBuilder().set<PlayerTag>().set<CharacterID>().build();
        int newId = 0;
        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (!e.test(playerMask)) continue;
            newId = (e.get<CharacterID>().id + 1) % NUM_CHARS;
            e.get<CharacterID>().id = newId;
            break;
        }

        // Sync SelectState so anthem / select screen stay consistent
        static const Mask ssMask = MaskBuilder().set<SelectState>().build();
        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (!e.test(ssMask)) continue;
            e.get<SelectState>().selected = newId;
            break;
        }

        // Reload player sprite
        if (player_texture) { SDL_DestroyTexture(player_texture); player_texture = nullptr; }
        SDL_Surface* ps = IMG_Load(char_files[newId]);
        if (ps) { player_texture = SDL_CreateTextureFromSurface(ren, ps); SDL_DestroySurface(ps); }

        // Play new character's anthem
        if (audio_stream) SDL_ClearAudioStream(audio_stream);
        _select_last_char = newId;
        const int sfx = char_sfx[newId];
        if (sfx != 0) play_sfx(sfx);
    }

} // namespace ci
