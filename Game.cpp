#include "Game.h"
#include "Game_constants.h"
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
                "res/yoamashit_pixel.png", "res/stalin_pixel.png", "res/sara_pixel.png"
            };
            for (int i = 0; i < NUM_CHARS; i++)
                char_select_tex[i] = loadTex(char_files[i]);
            shelter_texture       = loadTex("res/miklat.png");
            boss_texture          = loadTex("res/khamenai.png");
            hearts_texture        = loadTex("res/hearts_spreadsheet.png");
            iron_dome_texture     = loadTex("res/iron_dome.png");
            iron_dome_icon_texture= loadTex("res/iron_dome_icon.png");
            bibi_bullet_tex       = loadTex("res/mahal.jpg");
            sara_bullet_tex       = loadTex("res/snowspray.png");
        }

        // Audio
        SDL_AudioSpec spec{ SDL_AUDIO_S16, 1, 44100 };
        audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                                 &spec, nullptr, nullptr);
        if (audio_stream) SDL_ResumeAudioStreamDevice(audio_stream);

        Entity::create().add(SelectState{0, false, 1, 1}); // difficulty default = Normal, start_level=1
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
        if (bibi_bullet_tex)        SDL_DestroyTexture(bibi_bullet_tex);
        if (sara_bullet_tex)        SDL_DestroyTexture(sara_bullet_tex);
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

    // ─────────────────────────────── Main loop ───────────────────────────────────
    static constexpr float HAZARD_W = Game::TILE * 2.f - 4.f;
    static constexpr float HAZARD_H = Game::TILE       - 4.f;

    void Game::run()
    {
        auto start = SDL_GetTicks();
        bool quit  = false;

        static const Mask gsMask = MaskBuilder().set<GameStatus>().build();

        while (!quit) {
            // ── Reset per-frame performance counters ──────────────────────────────
            g_entity_checks     = 0;
            g_query_loop_starts = 0;

            bool gameOver = false, won = false;

            if (_state == GameState::Select) {
                select_input();
                _select_scroll += 0.5f;  // animate starfield
                if (_select_scroll > 10000.f) _select_scroll = 0.f;
            } else if (_state == GameState::Benchmark) {
                input_system();
                bench_system();
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
                floating_text_system();
                hop_system();
                animate_system();
                blink_system();
                sound_system();
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
            } else if (_state == GameState::Benchmark) {
                draw_system();
                if (_show_perf_hud) draw_benchmark_hud();
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
                if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN && _state == GameState::Paused) {
                    const float cx = (WIN_W - 480.f) / 2.f;
                    const float cy = (WIN_H - 300.f) / 2.f;
                    const float mx = ev.button.x;
                    const float my = ev.button.y;
                    // Mute checkbox
                    {
                        const float bx = cx + 30.f, by = cy + 232.f, bs = 14.f;
                        if (mx >= bx && mx <= bx + bs && my >= by && my <= by + bs) {
                            _muted = !_muted;
                            if (_muted && audio_stream) SDL_ClearAudioStream(audio_stream);
                        }
                    }
                    // Exit Game button
                    {
                        const float ebx = cx + 290.f, eby = cy + 228.f, ebw = 150.f, ebh = 22.f;
                        if (mx >= ebx && mx <= ebx + ebw && my >= eby && my <= eby + ebh) {
                            if (audio_stream) SDL_ClearAudioStream(audio_stream);
                            clear_game_entities();
                            _select_last_char = -1;
                            _state = GameState::Select;
                        }
                    }
                }
                if (ev.type == SDL_EVENT_KEY_DOWN) {
                    const SDL_Scancode sc = ev.key.scancode;
                    if (sc == SDL_SCANCODE_ESCAPE) {
                        if      (_state == GameState::Benchmark) {
                            clear_game_entities();
                            _select_last_char = -1;  // triggers anthem replay on next select_input()
                            _state = GameState::Select;
                        }
                        else if (_state == GameState::Playing) _state = GameState::Paused;
                        else if (_state == GameState::Paused)  _state = GameState::Playing;
                        else                                    quit   = true;
                    }
                    if (sc == SDL_SCANCODE_P) {
                        if      (_state == GameState::Playing) _state = GameState::Paused;
                        else if (_state == GameState::Paused)  _state = GameState::Playing;
                    }
                    if (sc == SDL_SCANCODE_RETURN && !ev.key.repeat && _state == GameState::MapScreen) {
                        static const Mask msMask = MaskBuilder().set<MapScreen>().build();
                        for (Entity e = Entity::first(); !e.eof(); e.next())
                            if (e.test(msMask)) { e.get<MapScreen>().framesLeft = 1; break; }
                    }
                    if (sc == SDL_SCANCODE_SPACE && _state == GameState::Select) {
                        if (audio_stream) SDL_ClearAudioStream(audio_stream);
                        static const Mask ssMask2 = MaskBuilder().set<SelectState>().build();
                        int startLvl = 1;
                        for (Entity e = Entity::first(); !e.eof(); e.next())
                            if (e.test(ssMask2)) { startLvl = e.get<SelectState>().start_level; break; }
                        _current_level = startLvl;
                        spawn_entities();
                        if (startLvl == 1) {
                            Entity::create().add(MapScreen{420, 1.f, 0, 0});
                            _state = GameState::MapScreen;
                        } else {
                            const int splashFrames = (startLvl == 4) ? 300 : 150;
                            Entity::create().add(LevelSplash{splashFrames});
                            _state = GameState::Playing;
                        }
                    }
                    // Keys 1–4 on select screen: choose starting level (for testing)
                    if (_state == GameState::Select &&
                        (sc == SDL_SCANCODE_1 || sc == SDL_SCANCODE_2 ||
                         sc == SDL_SCANCODE_3 || sc == SDL_SCANCODE_4)) {
                        static const Mask ssMask3 = MaskBuilder().set<SelectState>().build();
                        const int lvl = 1 + (sc - SDL_SCANCODE_1);
                        for (Entity e = Entity::first(); !e.eof(); e.next())
                            if (e.test(ssMask3)) { e.get<SelectState>().start_level = lvl; break; }
                    }
                    if (sc == SDL_SCANCODE_B && _state == GameState::Select) {
                        if (audio_stream) SDL_ClearAudioStream(audio_stream); // stop anthem
                        spawn_benchmark();
                        _state = GameState::Benchmark;
                    }
                    if (sc == SDL_SCANCODE_H)
                        _show_perf_hud = !_show_perf_hud;
                    if (sc == SDL_SCANCODE_C && _state == GameState::Playing)
                        switch_character();
                    if (sc == SDL_SCANCODE_R && (gameOver || won)) {
                        _current_level = 1;
                        reset();
                    }
                }
            }
        }
    }
} // namespace ci
