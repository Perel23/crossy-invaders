#include "Game.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace std;
using namespace bagel;

namespace ci
{
    static constexpr int ENEMY_ROWS      = 2;
    static constexpr int ENEMY_COLS      = 8;
    static constexpr int BOSS_HP         = 40;
    static constexpr int ENEMY_START_COL = (Game::COLS - ENEMY_COLS) / 2;
    static constexpr int ENEMY_TOP_LANE  = Game::LANES - 2;

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
       std::srand(static_cast<unsigned>(SDL_GetTicks()));

       // Load character-select preview sprites (both needed simultaneously on the select screen).
       {
          SDL_Surface* s = IMG_Load("res/trump_pixel.png");
          if (s) { trump_select_tex = SDL_CreateTextureFromSurface(ren, s); SDL_DestroySurface(s); }
          s = IMG_Load("res/bibi_pixel.png");
          if (s) { bibi_select_tex  = SDL_CreateTextureFromSurface(ren, s); SDL_DestroySurface(s); }
          s = IMG_Load("res/miklat.png");
          if (s) { shelter_texture  = SDL_CreateTextureFromSurface(ren, s); SDL_DestroySurface(s); }
          s = IMG_Load("res/khamenai.png");
          if (s) { boss_texture     = SDL_CreateTextureFromSurface(ren, s); SDL_DestroySurface(s); }
          s = IMG_Load("res/hearts_spreadsheet.png");
          if (s) { hearts_texture      = SDL_CreateTextureFromSurface(ren, s); SDL_DestroySurface(s); }
          s = IMG_Load("res/iron_dome.png");
          if (s) { iron_dome_texture      = SDL_CreateTextureFromSurface(ren, s); SDL_DestroySurface(s); }
          s = IMG_Load("res/iron_dome_icon.png");
          if (s) { iron_dome_icon_texture = SDL_CreateTextureFromSurface(ren, s); SDL_DestroySurface(s); }
       }

       // SDL3 audio: open the default playback device as a stream.
       SDL_AudioSpec spec;
       spec.format   = SDL_AUDIO_S16;
       spec.channels = 1;
       spec.freq     = 44100;
       audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                                &spec, nullptr, nullptr);
       if (audio_stream) SDL_ResumeAudioStreamDevice(audio_stream);

       Entity::create().add(SelectState{0, false});
    }

    Game::~Game()
    {
       if (player_texture)    SDL_DestroyTexture(player_texture);
       if (enemy_texture)     SDL_DestroyTexture(enemy_texture);
       if (boss_texture)      SDL_DestroyTexture(boss_texture);
       if (shelter_texture)   SDL_DestroyTexture(shelter_texture);
       if (hearts_texture)    SDL_DestroyTexture(hearts_texture);
       if (iron_dome_texture)      SDL_DestroyTexture(iron_dome_texture);
       if (iron_dome_icon_texture) SDL_DestroyTexture(iron_dome_icon_texture);
       if (trump_select_tex)  SDL_DestroyTexture(trump_select_tex);
       if (bibi_select_tex)   SDL_DestroyTexture(bibi_select_tex);
       for (int i = 0; i < 4; i++) {
          if (haz_textures[i]) SDL_DestroyTexture(haz_textures[i]);
       }
       if (audio_stream) SDL_DestroyAudioStream(audio_stream);
       if (ren) SDL_DestroyRenderer(ren);
       if (win) SDL_DestroyWindow(win);
       SDL_Quit();
    }

    static constexpr float HAZARD_W = Game::TILE * 2.f - 4.f;   // 124 px wide
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
          if (surf) {
             haz_textures[i] = SDL_CreateTextureFromSurface(ren, surf);
             SDL_DestroySurface(surf);
          }
       }
       static const Mask ssMask = MaskBuilder().set<SelectState>().build();
       int selected = 0;
       for (Entity e = Entity::first(); !e.eof(); e.next())
          if (e.test(ssMask)) { selected = e.get<SelectState>().selected; break; }

       if (player_texture) { SDL_DestroyTexture(player_texture); player_texture = nullptr; }
       const char* path = (selected == 0) ? "res/trump_pixel.png" : "res/bibi_pixel.png";
       SDL_Surface* surf = IMG_Load(path);
       if (surf) {
          player_texture = SDL_CreateTextureFromSurface(ren, surf);
          SDL_DestroySurface(surf);
       }

       // Load the enemy sprite (shared by every enemy in the formation).
       if (enemy_texture) { SDL_DestroyTexture(enemy_texture); enemy_texture = nullptr; }
       SDL_Surface* enemySurf = IMG_Load("res/iranian_regime_pixel.png");
       if (enemySurf) {
          enemy_texture = SDL_CreateTextureFromSurface(ren, enemySurf);
          SDL_DestroySurface(enemySurf);
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
          Invincibility{0},
          DamageFlash{0},
          RapidFire{0, 0});

       if (_current_level == 3) {
          // LEVEL 3: Boss Spawning
          Entity::create().addAll(
             Transform{{TILE / 2.f + (COLS / 2) * TILE, WIN_H - TILE / 2.f - ENEMY_TOP_LANE * TILE}, 0.f},
             Drawable{{0, 0, 0, 0}, {TILE * 3.f, TILE * 3.f}}, // 3x3 Size
             LanePos{ENEMY_TOP_LANE, COLS / 2},
             EnemyTag{},
             BossTag{},
             Health{40} // High Boss HP
          );
       } else {
          // LEVEL 1 & 2: Grid Spawning
          int current_rows = ENEMY_ROWS + (_current_level - 1); // Extra row on level 2
          for (int row = 0; row < current_rows; row++) {
             for (int col = 0; col < ENEMY_COLS; col++) {
                const int lane = ENEMY_TOP_LANE - row;
                const int ecol = ENEMY_START_COL + col;
                if (_current_level == 2) {
                   // Level 2: each enemy moves independently — give it an IndividualMove timer
                   Entity::create().addAll(
                      Transform{{TILE / 2.f + ecol * TILE, WIN_H - TILE / 2.f - lane * TILE}, 0.f},
                      Drawable{{0, 0, 0, 0}, {TILE - 4.f, TILE - 4.f}},
                      LanePos{lane, ecol},
                      EnemyTag{},
                      Health{_current_level},
                      IndividualMove{SDL_GetTicks() + (Uint64)(std::rand() % 500)});
                } else {
                   Entity::create().addAll(
                      Transform{{TILE / 2.f + ecol * TILE, WIN_H - TILE / 2.f - lane * TILE}, 0.f},
                      Drawable{{0, 0, 0, 0}, {TILE - 4.f, TILE - 4.f}},
                      LanePos{lane, ecol},
                      EnemyTag{},
                      Health{_current_level});
                }
             }
          }
       }

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

       // Hazards: randomised start positions and level-scaled speed.
       const float base_speed = 2.5f + (_current_level - 1) * 0.6f;
       struct HazardDef { int lane; float dx; int tex; };
       const HazardDef hdefs[] = {
          { 4,  base_speed, 0 },
          { 6, -base_speed, 1 },
       };
       int h_idx = 0;
       for (const auto& def : hdefs) {
          const float y = WIN_H - TILE / 2.f - def.lane * TILE;
          for (int k = 0; k < 2; k++) {
             const float x = (float)(std::rand() % WIN_W);
             Entity::create().addAll(
                Transform{{x, y}, 0.f},
                Drawable{{0, 0, 0, 0}, {HAZARD_W, HAZARD_H}},
                Velocity{def.dx, 0.f},
                Hazard{},
                HazardVisual{h_idx % 4});
             h_idx++;
          }
       }

       Entity::create().addAll(
          Transform{{0.f, 0.f}, 0.f},
          FormationState{1, 0, 0});

       Entity::create().addAll(
          Transform{{0.f, 0.f}, 0.f},
          GameStatus{false, false, false},
          ScreenShake{0},
          ComboState{0, 0, 1});

       // Reset camera for the new level/run.
       _camera_scroll = 0.f;
       _camera_grace  = 180;

       // Count enemies spawned so the difficulty-ramp system has a baseline.
       _wave_enemy_count = 0;
       static const Mask eMask = MaskBuilder().set<EnemyTag>().build();
       for (Entity e = Entity::first(); !e.eof(); e.next())
          if (e.test(eMask)) _wave_enemy_count++;
    }

    void Game::reset() const
    {
       static const Mask anyMask = MaskBuilder().set<Transform>().build();
       for (Entity e = Entity::first(); !e.eof(); e.next()) {
          if (e.test(anyMask)) e.destroy();
       }

       if (player_texture) { SDL_DestroyTexture(player_texture); player_texture = nullptr; }
       if (enemy_texture)  { SDL_DestroyTexture(enemy_texture);  enemy_texture  = nullptr; }

       static const Mask ssMask = MaskBuilder().set<SelectState>().build();
       for (Entity e = Entity::first(); !e.eof(); e.next()) {
          if (e.test(ssMask)) { e.get<SelectState>().moved = false; break; }
       }

       _score         = 0;
       _camera_scroll = 0.f;
       _camera_grace  = 180;
       _state = GameState::Select;
    }

    void Game::explosion_system() const
    {
       static const Mask mask = MaskBuilder().set<Explosion>().build();
       for (Entity e = Entity::first(); !e.eof(); e.next()) {
          if (!e.test(mask)) continue;
          if (--e.get<Explosion>().frames <= 0) e.destroy();
       }
    }

    void Game::pickup_system() const
    {
       static const Mask pickupMask = MaskBuilder().set<Pickup>().set<Transform>().set<Drawable>().build();
       static const Mask playerMask = MaskBuilder()
          .set<PlayerTag>().set<Transform>().set<Drawable>().set<Health>().build();

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
             if (type == 0) {                              // rapid-fire
                pl.get<RapidFire>().frames  = 300;
                pl.get<RapidFire>().cooldown = 0;
             } else if (type == 1) {                       // +shield charge
                auto& sh = pl.get<Shield>();
                sh.charges = std::min(sh.charges + 1, 3);
             } else {                                      // +1 HP (cap at 5)
                auto& hp = pl.get<Health>().hp;
                hp = std::min(hp + 1, 5);
             }
             play_sfx(4);
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

    void Game::play_sfx(int type) const
    {
       if (!audio_stream) return;
       // Skip if the queue already has more than 150 ms of audio (prevents lag on rapid fire).
       if (SDL_GetAudioStreamQueued(audio_stream) > 44100 * 150 / 1000 * (int)sizeof(Sint16)) return;

       std::vector<Sint16> buf;
       const int SR = 44100;
       auto tone = [&](float freq, float dur, float vol = 0.28f, float endVol = 0.0f) {
          int n = (int)(SR * dur);
          buf.resize(n);
          for (int i = 0; i < n; i++) {
             float t    = (float)i / SR;
             float fade = vol + (endVol - vol) * ((float)i / n);
             buf[i] = (Sint16)(32767 * fade * std::sin(2 * M_PI * freq * t));
          }
       };
       switch (type) {
          case 0: tone(880.f,  0.055f, 0.22f, 0.0f); break;             // shoot
          case 1: tone(520.f,  0.08f,  0.20f, 0.0f); break;             // enemy hit
          case 2: { // enemy death: frequency sweep down
             int n = (int)(SR * 0.14f);  buf.resize(n);
             for (int i = 0; i < n; i++) {
                float f = 700.f - 550.f * ((float)i / n);
                float v = 0.26f * (1.f - (float)i / n);
                buf[i] = (Sint16)(32767 * v * std::sin(2 * M_PI * f * (float)i / SR));
             }
             break;
          }
          case 3: tone(90.f,   0.22f, 0.38f, 0.05f); break;             // player hit (low thud)
          case 4: { // pickup / level clear: quick ascending jingle
             int n = (int)(SR * 0.35f);  buf.resize(n);
             const float notes[] = {330.f, 440.f, 550.f, 660.f};
             for (int i = 0; i < n; i++) {
                int ni   = std::min((int)(4.f * i / n), 3);
                float v  = 0.22f * (1.f - (float)i / n);
                buf[i] = (Sint16)(32767 * v * std::sin(2 * M_PI * notes[ni] * (float)i / SR));
             }
             break;
          }
          case 5: { // game over: slow descending tone
             int n = (int)(SR * 0.9f);  buf.resize(n);
             for (int i = 0; i < n; i++) {
                float f = 280.f - 200.f * ((float)i / n);
                float v = 0.30f * (1.f - (float)i / n);
                buf[i] = (Sint16)(32767 * v * std::sin(2 * M_PI * f * (float)i / SR));
             }
             break;
          }
          default: return;
       }
       if (!buf.empty())
          SDL_PutAudioStreamData(audio_stream, buf.data(), (int)(buf.size() * sizeof(Sint16)));
    }

    void Game::draw_background() const
    {
       // IMPORTANT: use raw _camera_scroll — NOT fmodf.
       // Entity screen Y = worldY + _camera_scroll.
       // Lane stripe screen Y = WIN_H - (lane+1)*TILE + _camera_scroll.
       // Both use the same offset, so vehicles always sit inside their road stripe.
       // fmodf would only apply a fractional-tile shift to the background while
       // entities moved by the full amount — that's what caused the road mismatch.
       //
       // Draw LANES+35 rows so the viewport stays filled after many tiles of scrolling
       // (35 extra tiles covers ~37 s of camera travel at the base scroll rate).
       // Use modular lane index so the road/safe/field pattern repeats every LANES=12 lanes.
       // This means road bands appear at lanes 4, 16, 28 … and 6, 18, 30 … indefinitely,
       // matching where vehicles land after their vertical recycle (also every LANES*TILE).
       for (int lane = -2; lane < LANES + 35; lane++) {
          const float y = WIN_H - (lane + 1) * TILE + _camera_scroll;
          if (y + TILE < 0.f || y > (float)WIN_H) continue;
          SDL_FRect rect = {0.f, y, (float)WIN_W, (float)TILE};

          // tl: position within the repeating LANES-wide pattern (0 … LANES-1)
          const int tl = ((lane % LANES) + LANES) % LANES;

          if (_current_level == 1) {
             if      (tl <= 1)             SDL_SetRenderDrawColor(ren,  20,  70,  20, 255); // safe – green
             else if (tl == 4 || tl == 6)  SDL_SetRenderDrawColor(ren,  40,  40,  50, 255); // road – asphalt
             else if (tl == 5)             SDL_SetRenderDrawColor(ren,  28,  55,  28, 255); // median
             else if (tl >= LANES - 2)     SDL_SetRenderDrawColor(ren,  35,   8,   8, 255); // danger band
             else                          SDL_SetRenderDrawColor(ren,  24,  55,  24, 255); // field
          } else if (_current_level == 2) {
             if      (tl <= 1)             SDL_SetRenderDrawColor(ren, 160, 120,  50, 255); // safe – sand
             else if (tl == 4 || tl == 6)  SDL_SetRenderDrawColor(ren,  90,  75,  40, 255); // road – dirt
             else if (tl == 5)             SDL_SetRenderDrawColor(ren, 130, 100,  45, 255); // median
             else if (tl >= LANES - 2)     SDL_SetRenderDrawColor(ren,  80,  25,  10, 255); // danger band
             else                          SDL_SetRenderDrawColor(ren, 145, 110,  48, 255); // desert
          } else {
             if      (tl <= 1)             SDL_SetRenderDrawColor(ren,  25,  20,  35, 255); // safe – violet
             else if (tl == 4 || tl == 6)  SDL_SetRenderDrawColor(ren,  18,  15,  25, 255); // road – near-black
             else if (tl == 5)             SDL_SetRenderDrawColor(ren,  30,  20,  40, 255); // median
             else if (tl >= LANES - 2)     SDL_SetRenderDrawColor(ren,  50,   5,   5, 255); // danger band
             else                          SDL_SetRenderDrawColor(ren,  22,  18,  30, 255); // dark field
          }
          SDL_RenderFillRect(ren, &rect);
       }

       // Road dashes — drawn for every tiled occurrence of road lanes (tl == 4 or 6)
       if      (_current_level == 1) SDL_SetRenderDrawColor(ren, 180, 160,  30, 255);
       else if (_current_level == 2) SDL_SetRenderDrawColor(ren, 120, 100,  30, 255);
       else                          SDL_SetRenderDrawColor(ren,  60,  40,  80, 255);
       for (int lane = -2; lane < LANES + 35; lane++) {
          const int tl = ((lane % LANES) + LANES) % LANES;
          if (tl != 4 && tl != 6) continue;
          const float cy = WIN_H - lane * TILE - TILE / 2.f + _camera_scroll;
          if (cy < -(float)TILE || cy > (float)WIN_H + TILE) continue;
          for (int x = 0; x < WIN_W; x += 40) {
             SDL_FRect dash = {(float)x, cy - 3.f, 24.f, 6.f};
             SDL_RenderFillRect(ren, &dash);
          }
       }

       // Lane divider lines — same modular range
       if      (_current_level == 1) SDL_SetRenderDrawColor(ren,  55,  55,  55, 255);
       else if (_current_level == 2) SDL_SetRenderDrawColor(ren, 100,  80,  40, 255);
       else                          SDL_SetRenderDrawColor(ren,  40,  30,  55, 255);
       for (int lane = -1; lane <= LANES + 35; lane++) {
          const float y = WIN_H - lane * TILE + _camera_scroll;
          if (y < -1.f || y > (float)WIN_H + 1.f) continue;
          SDL_FRect line = {0.f, y, (float)WIN_W, 1.f};
          SDL_RenderFillRect(ren, &line);
       }
    }

    void Game::splash_system() const
    {
       // Just count down and destroy the banner entity.
       // Gameplay continues uninterrupted — levels are now continuous.
       static const Mask mask = MaskBuilder().set<LevelSplash>().build();
       for (Entity e = Entity::first(); !e.eof(); e.next()) {
          if (!e.test(mask)) continue;
          if (--e.get<LevelSplash>().framesLeft <= 0) e.destroy();
          return;
       }
    }

    void Game::draw_level_splash() const
    {
       SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
       SDL_SetRenderDrawColor(ren, 0, 0, 0, 170);
       SDL_FRect full = {0, 0, (float)WIN_W, (float)WIN_H};
       SDL_RenderFillRect(ren, &full);
       SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

       SDL_SetRenderScale(ren, 5.f, 5.f);
       SDL_SetRenderDrawColor(ren, 80, 255, 80, 255);
       char buf[32];
       SDL_snprintf(buf, sizeof(buf), "LEVEL %d", _current_level);
       SDL_RenderDebugText(ren, 65.f, 46.f, buf);
       SDL_SetRenderScale(ren, 1.f, 1.f);

       SDL_SetRenderScale(ren, 2.f, 2.f);
       SDL_SetRenderDrawColor(ren, 160, 160, 160, 255);
       SDL_RenderDebugText(ren, 182.f, 105.f, "Get ready...");
       SDL_SetRenderScale(ren, 1.f, 1.f);
    }

    void Game::clear_game_entities() const
    {
       static const Mask anyMask = MaskBuilder().set<Transform>().build();
       for (Entity e = Entity::first(); !e.eof(); e.next())
          if (e.test(anyMask)) e.destroy();
       if (player_texture) { SDL_DestroyTexture(player_texture); player_texture = nullptr; }
       if (enemy_texture)  { SDL_DestroyTexture(enemy_texture);  enemy_texture  = nullptr; }
       for (int i = 0; i < 4; i++) {
          if (haz_textures[i]) { SDL_DestroyTexture(haz_textures[i]); haz_textures[i] = nullptr; }
       }
    }

    // Destroys only enemy-related entities — player, hazards, and shelters survive.
    // Called when transitioning between levels in continuous-play mode.
    void Game::clear_enemies_only() const
    {
       static const Mask enemyMask  = MaskBuilder().set<EnemyTag>().build();
       static const Mask bulletMask = MaskBuilder().set<BulletTag>().build();
       static const Mask explMask   = MaskBuilder().set<Explosion>().build();
       static const Mask pickupMask = MaskBuilder().set<Pickup>().build();
       for (Entity e = Entity::first(); !e.eof(); e.next()) {
          if (e.test(enemyMask) || e.test(bulletMask) ||
              e.test(explMask)  || e.test(pickupMask)) {
             e.destroy();
          }
       }
       if (enemy_texture) { SDL_DestroyTexture(enemy_texture); enemy_texture = nullptr; }
    }

    // Spawns the next wave of enemies above the current viewport.
    // Called for level 2 and 3 transitions (scroll is already > 0).
    // The base lane is calculated so enemies appear ~4 tiles above the screen top,
    // giving the player breathing room while the camera scrolls them into view.
    void Game::spawn_enemy_wave() const
    {
       if (!enemy_texture) {
          SDL_Surface* s = IMG_Load("res/iranian_regime_pixel.png");
          if (s) { enemy_texture = SDL_CreateTextureFromSurface(ren, s); SDL_DestroySurface(s); }
       }

       // screen_top world Y = -_camera_scroll.
       // 4 tiles above that: worldY = -_camera_scroll - 4*TILE.
       // lane satisfying WIN_H - TILE/2 - lane*TILE = worldY:
       //   lane = (WIN_H - TILE/2 + _camera_scroll + 4*TILE) / TILE
       // We use ENEMY_TOP_LANE as base and add the scroll contribution.
       const int enemy_base_lane = ENEMY_TOP_LANE
                                  + static_cast<int>(_camera_scroll / TILE)
                                  + 4;

       if (_current_level == 3) {
          const int lane = enemy_base_lane;
          Entity::create().addAll(
             Transform{{TILE / 2.f + (COLS / 2) * TILE, WIN_H - TILE / 2.f - lane * TILE}, 0.f},
             Drawable{{0, 0, 0, 0}, {TILE * 3.f, TILE * 3.f}},
             LanePos{lane, COLS / 2},
             EnemyTag{},
             BossTag{},
             Health{BOSS_HP});
       } else {
          // Level 2: individual random movement
          const int rows = ENEMY_ROWS + (_current_level - 1);
          for (int row = 0; row < rows; row++) {
             for (int col = 0; col < ENEMY_COLS; col++) {
                const int lane = enemy_base_lane - row;
                const int ecol = ENEMY_START_COL + col;
                Entity::create().addAll(
                   Transform{{TILE / 2.f + ecol * TILE, WIN_H - TILE / 2.f - lane * TILE}, 0.f},
                   Drawable{{0, 0, 0, 0}, {TILE - 4.f, TILE - 4.f}},
                   LanePos{lane, ecol},
                   EnemyTag{},
                   Health{_current_level},
                   IndividualMove{SDL_GetTicks() + (Uint64)(std::rand() % 500)});
             }
          }
       }

       // Refresh enemy count for difficulty ramp.
       _wave_enemy_count = 0;
       {
          static const Mask eMask = MaskBuilder().set<EnemyTag>().build();
          for (Entity e = Entity::first(); !e.eof(); e.next())
             if (e.test(eMask)) _wave_enemy_count++;
       }

       // Reset formation direction and timers for a fresh level feel.
       {
          static const Mask fsMask = MaskBuilder().set<FormationState>().build();
          for (Entity e = Entity::first(); !e.eof(); e.next()) {
             if (!e.test(fsMask)) continue;
             auto& fs       = e.get<FormationState>();
             fs.dir         = 1;
             fs.moveTimer   = SDL_GetTicks();
             fs.shootTimer  = SDL_GetTicks();
             break;
          }
       }

       // Clear the won flag so the game doesn't immediately re-detect a win.
       {
          static const Mask gsMask = MaskBuilder().set<GameStatus>().build();
          for (Entity e = Entity::first(); !e.eof(); e.next()) {
             if (!e.test(gsMask)) continue;
             e.get<GameStatus>().won = false;
             break;
          }
       }
    }

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

       // Character sprites inside each box.
       {
          constexpr float IMG_PAD = 20.f;
          constexpr float IMG_H   = BOX_H - 55.f;
          if (trump_select_tex) {
             SDL_FRect td = {TRUMP_X + IMG_PAD, BOX_Y + IMG_PAD, BOX_W - IMG_PAD * 2, IMG_H};
             SDL_RenderTexture(ren, trump_select_tex, nullptr, &td);
          }
          if (bibi_select_tex) {
             SDL_FRect bd = {BIBI_X + IMG_PAD, BOX_Y + IMG_PAD, BOX_W - IMG_PAD * 2, IMG_H};
             SDL_RenderTexture(ren, bibi_select_tex, nullptr, &bd);
          }
       }

       SDL_SetRenderScale(ren, 3.f, 3.f);
       SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
       SDL_RenderDebugText(ren, 104.f, 90.f, "TRUMP");
       SDL_RenderDebugText(ren, 201.f, 90.f, "BIBI");
       SDL_SetRenderScale(ren, 1.f, 1.f);

       SDL_SetRenderScale(ren, 2.f, 2.f);
       SDL_SetRenderDrawColor(ren, 180, 180, 180, 255);
       // Instructions
       SDL_RenderDebugText(ren, 10.f, 280.f, "GOAL: Defeat all enemies and reach the top!");
       SDL_RenderDebugText(ren, 10.f, 310.f, "ARROWS:Move # SPACE:Shoot # I:Iron Dome # P/ESC:Pause/Quit");
       SDL_RenderDebugText(ren, 10.f, 325.f, "PICKUPS: Yellow=Rapid Fire  Blue=Iron Dome  Red=+Heart");
       SDL_RenderDebugText(ren, 10.f, 345.f, "hit SPACE to play");
       SDL_SetRenderScale(ren, 1.f, 1.f);

       // Controls reference.
       SDL_SetRenderScale(ren, 1.5f, 1.5f);
       SDL_SetRenderDrawColor(ren, 120, 120, 120, 255);
       SDL_SetRenderScale(ren, 1.f, 1.f);
    }

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
       static const Mask enemyMask = MaskBuilder().set<EnemyTag>().build();

       // Check if enemies exist
       bool anyEnemy = false;
       for (Entity e = Entity::first(); !e.eof(); e.next()) {
          if (e.test(enemyMask)) { anyEnemy = true; break; }
       }

       // max_lane must grow with the camera so the player can always move up.
       // The hardcoded LANES-1=11 cap was fine at scroll=0 but blocks movement
       // once the camera has scrolled further up the world.
       // Formula: the highest visible lane = (WIN_H - TILE/2 + scroll) / TILE.
       // Leave a 1-lane gap below the screen top so the player stays in view.
       const int scrollTopLane = static_cast<int>((WIN_H - TILE / 2.f + _camera_scroll) / TILE);
       int max_lane = scrollTopLane - (anyEnemy ? 1 : 0);

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

          lp.lane = clamp(lp.lane, 0, max_lane);
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

       // ── Level 2: each enemy moves independently left/right at random intervals ──
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
             if (lp.col < 0)       lp.col = 0;
             if (lp.col > COLS-1)  lp.col = COLS-1;
             t.p.x = TILE / 2.f + lp.col * TILE;
             // next move: 200–700 ms from now, randomised per-enemy
             im.nextMove = now + 200 + (Uint64)(std::rand() % 500);
          }
          return;
       }

       // ── Levels 1 & 3: classic Space-Invaders formation movement ──
       Entity fsEnt = Entity::first();
       for (Entity e = Entity::first(); !e.eof(); e.next())
          if (e.test(fsMask)) { fsEnt = e; break; }
       auto& fs = fsEnt.get<FormationState>();

       const Uint64 now = SDL_GetTicks();

       // Base speed scales per level (level 3 fastest).
       Uint64 move_ms = ENEMY_MOVE_MS;
       if (_current_level == 3) move_ms -= 300;

       // Difficulty ramp within the wave: fewer enemies = faster (Space Invaders feel).
       if (_wave_enemy_count > 0) {
          int remaining = 0;
          for (Entity e = Entity::first(); !e.eof(); e.next())
             if (e.test(mask)) remaining++;
          const float ratio = std::max(0.3f, (float)remaining / _wave_enemy_count);
          move_ms = (Uint64)(move_ms * ratio);
       }

       if (now - fs.moveTimer < move_ms) return;
       fs.moveTimer = now;

       // Check if any enemy is at a wall.
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

       // Flip direction on wall hit, then move all enemies sideways.
       // (No lane drop — enemies stay at their lanes now.)
       if (hitEdge) fs.dir = -fs.dir;

       for (Entity e = Entity::first(); !e.eof(); e.next()) {
          if (!e.test(mask)) continue;
          auto& lp = e.get<LanePos>();
          auto& t  = e.get<Transform>();
          lp.col += fs.dir;
          if (lp.col < 0)       lp.col = 0;
          if (lp.col > COLS-1)  lp.col = COLS-1;
          t.p.x = TILE / 2.f + lp.col * TILE;
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
             const SDL_FPoint p = e.get<Transform>().p;
             Entity::create().addAll(
                Transform{p, 0.f},
                Drawable{{0, 0, 0, 0}, {6.f, 14.f}},
                BulletTag{true},
                Velocity{0.f, -BULLET_SPEED});
             play_sfx(0);
          }
       }

       Entity fsEnt = Entity::first();
       for (Entity e = Entity::first(); !e.eof(); e.next())
          if (e.test(fsMask)) { fsEnt = e; break; }
       auto& fs = fsEnt.get<FormationState>();

       const Uint64 now = SDL_GetTicks();

       Uint64 shoot_ms = ENEMY_SHOOT_MS;
       if (_current_level == 2) shoot_ms -= 400;
       if (_current_level == 3) shoot_ms -= 800; // Boss shoots constantly
       if (now - fs.shootTimer < shoot_ms) return;
       fs.shootTimer = now;

       // Find minimum (closest-to-player) lane of all enemies.
       int minLane = LANES;
       for (Entity e = Entity::first(); !e.eof(); e.next()) {
          if (!e.test(enemyMask)) continue;
          const int lane = e.get<LanePos>().lane;
          if (lane < minLane) minLane = lane;
       }

       // Count enemies on that front row.
       int frontCount = 0;
       for (Entity e = Entity::first(); !e.eof(); e.next()) {
          if (e.test(enemyMask) && e.get<LanePos>().lane == minLane) frontCount++;
       }

       // Pick one at random and shoot from it.
       if (frontCount > 0) {
          int target = std::rand() % frontCount;
          int idx    = 0;
          for (Entity e = Entity::first(); !e.eof(); e.next()) {
             if (!e.test(enemyMask) || e.get<LanePos>().lane != minLane) continue;
             if (idx++ != target) continue;
             const SDL_FPoint p = e.get<Transform>().p;
             if (_current_level == 3) {
                // Boss: three-way spread shot.
                for (float dx : {-2.5f, 0.f, 2.5f})
                   Entity::create().addAll(Transform{p,0.f}, Drawable{{0,0,0,0},{6.f,14.f}},
                                           BulletTag{false}, Velocity{dx, BULLET_SPEED});
             } else {
                Entity::create().addAll(Transform{p, 0.f}, Drawable{{0,0,0,0},{6.f,14.f}},
                                        BulletTag{false}, Velocity{0.f, BULLET_SPEED});
             }
             break;
          }
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
          // Use screen Y for cull — world Y alone is wrong for level-2/3 enemies
          // whose spawn position is negative (above the normal lane grid).
          const float screenY = t.p.y + _camera_scroll;
          if (screenY < -(float)TILE || screenY > (float)WIN_H + TILE) {
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

          // Horizontal wrap (always)
          t.p.x += dx;
          if (t.p.x - hw >  WIN_W) t.p.x = -hw;
          if (t.p.x + hw <  0)     t.p.x =  WIN_W + hw;

          // Vertical recycle: when the camera scrolls a vehicle off the bottom,
          // jump it up by exactly LANES*TILE (one full background-pattern cycle).
          // Because the background tiles with period LANES*TILE, the vehicle will
          // land on the next road stripe above — same colour, same lane identity.
          const float screenY = t.p.y + _camera_scroll;
          if (screenY > (float)WIN_H + TILE) {
             t.p.y   -= LANES * TILE;                       // one pattern cycle up
             t.p.x    = (float)(std::rand() % WIN_W);       // fresh horizontal position
          }
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
       static const Mask gsMask = MaskBuilder().set<GameStatus>().set<ScreenShake>().set<ComboState>().build();
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

       // Decrement invincibility and DamageFlash on the player.
       for (Entity e = Entity::first(); !e.eof(); e.next()) {
          if (!e.test(playerMask)) continue;
          if (e.has<Invincibility>() && e.get<Invincibility>().frames > 0) --e.get<Invincibility>().frames;
          if (e.has<DamageFlash>()   && e.get<DamageFlash>().frames   > 0) --e.get<DamageFlash>().frames;
          break;
       }

       // Decrement screen shake
       if (gsEnt.has<ScreenShake>() && gsEnt.get<ScreenShake>().frames > 0) {
          --gsEnt.get<ScreenShake>().frames;
       }

       auto overlaps = [](SDL_FPoint p1, SDL_FPoint s1, SDL_FPoint p2, SDL_FPoint s2) {
          return std::abs(p1.x - p2.x) < (s1.x + s2.x) * 0.5f &&
                 std::abs(p1.y - p2.y) < (s1.y + s2.y) * 0.5f;
       };

       auto hurt_player = [&](Entity pl) {
          if (pl.get<Invincibility>().frames != 0) return;
          if (--pl.get<Health>().hp <= 0) gs.gameOver = true;
          pl.get<Invincibility>().frames = 90;
          pl.get<DamageFlash>().frames   = 22;
          gsEnt.get<ScreenShake>().frames = 8;
          play_sfx(3);
       };

       auto kill_enemy = [&](Entity en) {
          const SDL_FPoint pos = en.get<Transform>().p;
          const float sz       = en.get<Drawable>().size.x;
          en.destroy();

          // Explosion particle.
          Entity::create().addAll(Transform{pos, 0.f}, Explosion{12, 12, sz});

          // Update combo and score.
          auto& cs = gsEnt.get<ComboState>();
          cs.count      = (cs.timer > 0) ? cs.count + 1 : 1;
          cs.timer      = 60;
          cs.multiplier = std::min(cs.count, 5);
          _score += 10 * _current_level * cs.multiplier;

          // Random pickup drop (~30% chance, not for boss).
          // The pickup lands at a random lane height (1-7) instead of the enemy's position,
          // so it drifts into the playfield rather than staying near the top.
          if (sz < TILE * 2.f && std::rand() % 10 < 3) {
             const int   type     = std::rand() % 3;
             const int   dropLane = 1 + std::rand() % 7;            // lanes 1–7
             const float dropY    = WIN_H - TILE / 2.f - dropLane * TILE;
             Entity::create().addAll(Transform{{pos.x, dropY}, 0.f},
                                     Drawable{{0,0,0,0},{28.f, 28.f}},
                                     Pickup{type});
          }
          play_sfx(2);
       };

       // Bullet × shelter.
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

       // Player bullet × enemy.
       for (Entity b = Entity::first(); !b.eof(); b.next()) {
          if (!b.test(bulletMask) || !b.get<BulletTag>().fromPlayer) continue;
          const SDL_FPoint bp = b.get<Transform>().p;
          const SDL_FPoint bs = b.get<Drawable>().size;
          for (Entity en = Entity::first(); !en.eof(); en.next()) {
             if (!en.test(enemyMask)) continue;
             if (!overlaps(bp, bs, en.get<Transform>().p, en.get<Drawable>().size)) continue;
             b.destroy();
             const int maxHp = _current_level;
             if (--en.get<Health>().hp <= 0) kill_enemy(en);
             else play_sfx(1);
             (void)maxHp;
             break;
          }
       }

       // Enemy bullet × player.
       for (Entity b = Entity::first(); !b.eof(); b.next()) {
          if (!b.test(bulletMask) || b.get<BulletTag>().fromPlayer) continue;
          const SDL_FPoint bp = b.get<Transform>().p;
          const SDL_FPoint bs = b.get<Drawable>().size;
          for (Entity pl = Entity::first(); !pl.eof(); pl.next()) {
             if (!pl.test(playerMask)) continue;
             if (!overlaps(bp, bs, pl.get<Transform>().p, pl.get<Drawable>().size)) continue;
             if (pl.get<Shield>().timer > 0.f) { b.destroy(); break; }
             b.destroy();
             hurt_player(pl);
             break;
          }
       }

       // Hazard × player.
       static const Mask hazardMask = MaskBuilder().set<Hazard>().set<Transform>().set<Drawable>().build();
       for (Entity h = Entity::first(); !h.eof(); h.next()) {
          if (!h.test(hazardMask)) continue;
          for (Entity pl = Entity::first(); !pl.eof(); pl.next()) {
             if (!pl.test(playerMask)) continue;
             if (!overlaps(h.get<Transform>().p, h.get<Drawable>().size,
                           pl.get<Transform>().p, pl.get<Drawable>().size)) continue;
             hurt_player(pl);
             break;
          }
       }

       // Enemy body × player collision (contact kill).
       for (Entity en = Entity::first(); !en.eof(); en.next()) {
          if (!en.test(enemyMask)) continue;
          for (Entity pl = Entity::first(); !pl.eof(); pl.next()) {
             if (!pl.test(playerMask)) continue;
             if (!overlaps(en.get<Transform>().p, en.get<Drawable>().size,
                           pl.get<Transform>().p, pl.get<Drawable>().size)) continue;
             gs.gameOver = true;
             return;
          }
       }

       // Camera-scroll death: player scrolled off the bottom of the screen.
       for (Entity pl = Entity::first(); !pl.eof(); pl.next()) {
          if (!pl.test(playerMask)) continue;
          if (pl.get<Transform>().p.y + _camera_scroll > WIN_H + TILE / 2.f) {
             gs.gameOver = true;
             return;
          }
          break;
       }

       // Win condition: all enemies cleared.
       // (No walk-off-top required — continuous levels spawn the next wave above.)
       bool anyEnemy = false;
       for (Entity e = Entity::first(); !e.eof(); e.next())
          if (e.test(enemyMask)) { anyEnemy = true; break; }
       if (!anyEnemy) gs.won = true;
    }

    void Game::draw_system() const
    {
       // Screen shake: shift the viewport by a random pixel offset while frames remain.
       {
          static const Mask shMask = MaskBuilder().set<ScreenShake>().build();
          int ox = 0, oy = 0;
          for (Entity e = Entity::first(); !e.eof(); e.next()) {
             if (!e.test(shMask)) continue;
             if (e.get<ScreenShake>().frames > 0) {
                ox = (std::rand() % 9) - 4;
                oy = (std::rand() % 9) - 4;
             }
             break;
          }
          if (ox || oy) {
             SDL_Rect vp = {ox, oy, WIN_W, WIN_H};
             SDL_SetRenderViewport(ren, &vp);
          }
       }

       draw_background();

       static const Mask drawMask     = MaskBuilder().set<Transform>().set<Drawable>().build();
       static const Mask playerMask   = MaskBuilder().set<PlayerTag>().set<Shield>().build();
       static const Mask bossMask     = MaskBuilder().set<BossTag>().set<Health>().build();
       static const Mask enemyMask    = MaskBuilder().set<EnemyTag>().set<Health>().build();
       static const Mask bulletMask   = MaskBuilder().set<BulletTag>().build();
       static const Mask shelterDMsk  = MaskBuilder().set<Shelter>().set<Health>().build();
       static const Mask hazardVisMsk = MaskBuilder().set<Hazard>().set<HazardVisual>().build();
       static const Mask explMask     = MaskBuilder().set<Explosion>().set<Transform>().build();
       static const Mask pickupMask   = MaskBuilder().set<Pickup>().set<Transform>().build();

       for (Entity e = Entity::first(); !e.eof(); e.next()) {
          // Explosions (no Drawable component — drawn directly).
          if (e.test(explMask)) {
             const auto& ex  = e.get<Explosion>();
             const SDL_FPoint p = e.get<Transform>().p;
             float progress  = 1.0f - (float)ex.frames / ex.maxFrames;
             float size      = ex.startSize * (1.0f + progress * 1.8f);
             Uint8 alpha     = (Uint8)(255 * (1.0f - progress));
             SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
             SDL_SetRenderDrawColor(ren, 255, (Uint8)(200 - (Uint8)(100 * progress)), 30, alpha);
             SDL_FRect er = {p.x - size / 2, p.y - size / 2 + _camera_scroll, size, size};
             SDL_RenderFillRect(ren, &er);
             SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
             continue;
          }

          // Pickups.
          if (e.test(pickupMask)) {
             const int type = e.get<Pickup>().type;
             const SDL_FPoint p = e.get<Transform>().p;
             constexpr float PICK_SZ = 30.f;
             const SDL_FRect pr = {p.x - PICK_SZ / 2.f, p.y - PICK_SZ / 2.f + _camera_scroll, PICK_SZ, PICK_SZ};

             if (type == 2 && hearts_texture) {
                // +HP: full red heart (row 0, col 0 of the spritesheet).
                float tw = 0, th = 0;
                SDL_GetTextureSize(hearts_texture, &tw, &th);
                const SDL_FRect srcHeart = {0.f, 0.f, tw / 3.f, th / 3.f};
                SDL_RenderTexture(ren, hearts_texture, &srcHeart, &pr);
             } else if (type == 1 && iron_dome_icon_texture) {
                // +Shield: iron dome icon (530×470 RGBA).
                SDL_RenderTexture(ren, iron_dome_icon_texture, nullptr, &pr);
             } else {
                // type 0 (rapid-fire) or texture missing: coloured square fallback.
                if      (type == 0) SDL_SetRenderDrawColor(ren, 255, 220,  30, 255);
                else if (type == 1) SDL_SetRenderDrawColor(ren,  80, 160, 255, 255);
                else                SDL_SetRenderDrawColor(ren,  50, 220, 100, 255);
                SDL_RenderFillRect(ren, &pr);
                SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
                SDL_FRect inner = {p.x - 6.f, p.y - 6.f, 12.f, 12.f};
                SDL_RenderFillRect(ren, &inner);
             }
             continue;
          }

          if (!e.test(drawMask)) continue;

          const auto& t = e.get<Transform>();
          const auto& d = e.get<Drawable>();
          SDL_FRect dest = {t.p.x - d.size.x / 2,
                            t.p.y - d.size.y / 2 + _camera_scroll,
                            d.size.x, d.size.y};

          if (e.test(playerMask) && e.get<Shield>().timer > 0.f) {
             // Iron Dome image: 676×369 RGBA, rendered as a dome around the player.
             // Width ≈ 2× tile, height preserves aspect ratio (369/676 ≈ 0.546).
             // Slight alpha pulse so it visually "breathes" while active.
             constexpr float DOME_W = Game::TILE * 2.1f;
             constexpr float DOME_H = DOME_W * (369.f / 676.f);
             const SDL_FRect domeRect = {
                t.p.x - DOME_W / 2.f,
                t.p.y - DOME_H / 2.f + _camera_scroll,
                DOME_W, DOME_H
             };
             if (iron_dome_texture) {
                // Pulse: timer counts down from SHIELD_DURATION (300) to 0.
                const float phase = e.get<Shield>().timer / SHIELD_DURATION;
                const Uint8 alpha = static_cast<Uint8>(180 + 60 * std::abs(std::sin(phase * 12.f)));
                SDL_SetTextureAlphaMod(iron_dome_texture, alpha);
                SDL_RenderTexture(ren, iron_dome_texture, nullptr, &domeRect);
                SDL_SetTextureAlphaMod(iron_dome_texture, 255);
             } else {
                SDL_SetRenderDrawColor(ren, 80, 140, 255, 255);
                SDL_RenderFillRect(ren, &domeRect);
             }
          }

          if (e.test(playerMask)) {
             if (player_texture) SDL_RenderTexture(ren, player_texture, nullptr, &dest);
             else { SDL_SetRenderDrawColor(ren, 220, 200, 50, 255); SDL_RenderFillRect(ren, &dest); }
          } else if (e.test(bulletMask)) {
             SDL_SetRenderDrawColor(ren,
                e.get<BulletTag>().fromPlayer ? 255 : 255,
                e.get<BulletTag>().fromPlayer ? 255 : 140, 0, 255);
             SDL_RenderFillRect(ren, &dest);
          } else if (e.test(shelterDMsk)) {
             // Darken the shelter image as it takes damage (HP 5→1).
             const Uint8 v = static_cast<Uint8>(80 + e.get<Health>().hp * 35);
             if (shelter_texture) {
                SDL_SetTextureColorMod(shelter_texture, v, v, v);
                SDL_RenderTexture(ren, shelter_texture, nullptr, &dest);
                SDL_SetTextureColorMod(shelter_texture, 255, 255, 255);
             } else {
                SDL_SetRenderDrawColor(ren, v, v, v, 255);
                SDL_RenderFillRect(ren, &dest);
             }
          } else if (e.test(hazardVisMsk)) {
             const int idx = e.get<HazardVisual>().tex_index;
             if (haz_textures[idx]) SDL_RenderTexture(ren, haz_textures[idx], nullptr, &dest);
             else { SDL_SetRenderDrawColor(ren, 0, 200, 220, 255); SDL_RenderFillRect(ren, &dest); }
          } else if (e.test(bossMask)) {
             // Boss: khamenai texture, tints redder as HP drops.
             const int hp = e.get<Health>().hp;
             const Uint8 g = static_cast<Uint8>(255 * hp / BOSS_HP);
             if (boss_texture) {
                SDL_SetTextureColorMod(boss_texture, 255, g, g);
                SDL_RenderTexture(ren, boss_texture, nullptr, &dest);
                SDL_SetTextureColorMod(boss_texture, 255, 255, 255);
             } else {
                SDL_SetRenderDrawColor(ren, 200, 0, 0, 255);
                SDL_RenderFillRect(ren, &dest);
             }
          } else if (e.test(enemyMask)) {
             // Regular enemies: HP tinting.
             const int hp    = e.get<Health>().hp;
             const int maxHp = _current_level;
             if (enemy_texture) {
                if (hp < maxHp) SDL_SetTextureColorMod(enemy_texture, 255, 80, 80);
                SDL_RenderTexture(ren, enemy_texture, nullptr, &dest);
                if (hp < maxHp) SDL_SetTextureColorMod(enemy_texture, 255, 255, 255);
             } else {
                SDL_SetRenderDrawColor(ren, 220, 50, 50, 255);
                SDL_RenderFillRect(ren, &dest);
             }
          }
       }

       // Reset viewport before HUD (so HUD is never shaken).
       SDL_SetRenderViewport(ren, nullptr);

       // Damage flash: full-screen red overlay fading out.
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

       // HUD: HP icons.
       static const Mask hpMask = MaskBuilder().set<PlayerTag>().set<Health>().set<Shield>().build();
       for (Entity e = Entity::first(); !e.eof(); e.next()) {
          if (!e.test(hpMask)) continue;
          const int hp   = e.get<Health>().hp;
          const auto& sh = e.get<Shield>();
          const bool rf  = e.has<RapidFire>() && e.get<RapidFire>().frames > 0;

          //hearts drawing
          if (hearts_texture) {
             float tw = 0, th = 0;
             SDL_GetTextureSize(hearts_texture, &tw, &th);
             const float cw = tw / 3.f;
             const float ch = th / 3.f;
             const SDL_FRect srcFull  = {0.f,        0.f, cw, ch};
             const SDL_FRect srcEmpty = {2.f * cw,   ch,  cw, ch};

             for (int i = 0; i < 3; i++) {
                const SDL_FRect dst = {8.f + i * 30.f, 6.f, 26.f, 26.f};
                SDL_RenderTexture(ren, hearts_texture, (i < hp) ? &srcFull : &srcEmpty, &dst);
             }
          } else {
             // Fallback coloured squares.
             for (int i = 0; i < 5; i++) {
                SDL_FRect icon = {10.f + i * 26.f, 10.f, 20.f, 20.f};
                SDL_SetRenderDrawColor(ren, i < hp ? 0 : 55, i < hp ? 220 : 55, i < hp ? 80 : 55, 255);
                SDL_RenderFillRect(ren, &icon);
             }
          }
          // Iron Dome charge icons: full opacity = available, dimmed = used.
          for (int i = 0; i < 3; i++) {
             const bool avail = (i < sh.charges) || (i == 0 && sh.timer > 0.f);
             // icon is 530×470; render square-ish: preserve ratio → w=28, h=28*(470/530)≈25
             const SDL_FRect icon = {150.f + i * 32.f, 4.f, 28.f, 25.f};
             if (iron_dome_icon_texture) {
                SDL_SetTextureAlphaMod(iron_dome_icon_texture, avail ? 255 : 60);
                SDL_RenderTexture(ren, iron_dome_icon_texture, nullptr, &icon);
                SDL_SetTextureAlphaMod(iron_dome_icon_texture, 255);
             } else {
                SDL_SetRenderDrawColor(ren, avail ? 80 : 55, avail ? 140 : 55, avail ? 255 : 55, 255);
                SDL_RenderFillRect(ren, &icon);
             }
          }
          // Rapid-fire indicator.
          if (rf) {
             SDL_SetRenderDrawColor(ren, 255, 220, 30, 255);
             SDL_FRect rfIcon = {240.f, 10.f, 20.f, 20.f};
             SDL_RenderFillRect(ren, &rfIcon);
          }
          break;
       }

       // Combo display.
       {
          static const Mask comboMask = MaskBuilder().set<ComboState>().build();
          for (Entity e = Entity::first(); !e.eof(); e.next()) {
             if (!e.test(comboMask)) continue;
             const auto& cs = e.get<ComboState>();
             if (cs.count >= 2 && cs.timer > 0) {
                SDL_SetRenderScale(ren, 2.5f, 2.5f);
                SDL_SetRenderDrawColor(ren, 255, 200, 30, 255);
                char buf[16];
                SDL_snprintf(buf, sizeof(buf), "x%d COMBO!", cs.count);
                SDL_RenderDebugText(ren, (WIN_W / 2.5f - 50.f), 60.f, buf);
                SDL_SetRenderScale(ren, 1.f, 1.f);
             }
             break;
          }
       }

       // Level + score text.
       {
          SDL_SetRenderScale(ren, 2.f, 2.f);
          SDL_SetRenderDrawColor(ren, 200, 200, 200, 255);
          char buf[48];
          SDL_snprintf(buf, sizeof(buf), "LVL %d", _current_level);
          SDL_RenderDebugText(ren, 5.f, 20.f, buf);
          SDL_snprintf(buf, sizeof(buf), "SCORE %05d", _score);
          SDL_RenderDebugText(ren, 216.f, 5.f, buf);
          SDL_SetRenderScale(ren, 1.f, 1.f);
       }

       // Boss health bar (level 3 only).
       if (_current_level == 3) {
          static const Mask bossMask = MaskBuilder().set<EnemyTag>().set<Health>().build();
          for (Entity e = Entity::first(); !e.eof(); e.next()) {
             if (!e.test(bossMask)) continue;
             const int hp   = e.get<Health>().hp;
             const float bw = WIN_W * 0.55f;
             const float bx = (WIN_W - bw) / 2.f;
             SDL_SetRenderDrawColor(ren, 70, 0, 0, 255);
             SDL_FRect barBg   = {bx, 46.f, bw, 12.f};
             SDL_FRect barFill = {bx, 46.f, bw * hp / (float)BOSS_HP, 12.f};
             SDL_RenderFillRect(ren, &barBg);
             SDL_SetRenderDrawColor(ren, 230, 50, 50, 255);
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
       draw_system();

       SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
       SDL_SetRenderDrawColor(ren, 0, 0, 0, 145);
       SDL_FRect full = {0, 0, (float)WIN_W, (float)WIN_H};
       SDL_RenderFillRect(ren, &full);
       SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

       SDL_SetRenderScale(ren, 5.f, 5.f);
       SDL_SetRenderDrawColor(ren, 180, 200, 255, 255);
       SDL_RenderDebugText(ren, 65.f, 46.f, "PAUSED");
       SDL_SetRenderScale(ren, 1.f, 1.f);

       SDL_SetRenderScale(ren, 2.f, 2.f);
       SDL_SetRenderDrawColor(ren, 140, 140, 140, 255);
       SDL_RenderDebugText(ren, 168.f, 110.f, "P / ESC  to resume");
       SDL_SetRenderScale(ren, 1.f, 1.f);
    }

    void Game::endgame_draw() const
    {
       SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
       SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);
       SDL_FRect full = {0, 0, (float)WIN_W, (float)WIN_H};
       SDL_RenderFillRect(ren, &full);
       SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

       static const Mask gsMask = MaskBuilder().set<GameStatus>().build();
       bool gameOver = false;
       for (Entity e = Entity::first(); !e.eof(); e.next())
          if (e.test(gsMask)) { gameOver = e.get<GameStatus>().gameOver; break; }

       // Play end-game sound exactly once.
       for (Entity e = Entity::first(); !e.eof(); e.next()) {
          if (!e.test(gsMask)) continue;
          auto& gs2 = e.get<GameStatus>();
          if (!gs2.sfxPlayed) { play_sfx(gameOver ? 5 : 4); gs2.sfxPlayed = true; }
          break;
       }

       // SDL_RenderDebugText uses a fixed 8×8 px font.
       // At scale S the virtual canvas is WIN_W/S × WIN_H/S.
       // Centre formula: x = (canvas_w - char_count * 8) / 2
       //                 y = real_pixel_y / S
       //
       // Layout (real pixels, centred block of ~84 px):
       //   title  (scale 4, 32 px tall) starts at real_y = (WIN_H - 84) / 2 = 342
       //   score  (scale 2, 16 px tall) starts at real_y = 342 + 32 + 12     = 386
       //   restart(scale 2, 16 px tall) starts at real_y = 386 + 16 +  8     = 410

       // --- Title ---
       SDL_SetRenderScale(ren, 4.f, 4.f);
       // virtual canvas = 256 × 192
       if (gameOver) {
          // "GAME OVER" = 9 chars → 72 px → x = (256-72)/2 = 92
          SDL_SetRenderDrawColor(ren, 255, 80, 80, 255);
          SDL_RenderDebugText(ren, 92.f, 342.f / 4.f, "GAME OVER");
       } else {
          // "YOU WIN!" = 8 chars → 64 px → x = (256-64)/2 = 96
          SDL_SetRenderDrawColor(ren, 80, 255, 80, 255);
          SDL_RenderDebugText(ren, 96.f, 342.f / 4.f, "YOU WIN!");
       }
       SDL_SetRenderScale(ren, 1.f, 1.f);

       // --- Score + high score ---
       if (_score > _high_score) _high_score = _score;
       SDL_SetRenderScale(ren, 2.f, 2.f);
       // virtual canvas = 512 × 384
       // "SCORE 00000    BEST 00000" = 25 chars → 200 px → x = (512-200)/2 = 156
       SDL_SetRenderDrawColor(ren, 200, 200, 200, 255);
       char buf[64];
       SDL_snprintf(buf, sizeof(buf), "SCORE %05d    BEST %05d", _score, _high_score);
       SDL_RenderDebugText(ren, 156.f, 386.f / 2.f, buf);

       // "Press R to restart" = 18 chars → 144 px → x = (512-144)/2 = 184
       SDL_SetRenderDrawColor(ren, 160, 160, 160, 255);
       SDL_RenderDebugText(ren, 184.f, 410.f / 2.f, "Press R to restart");
       SDL_SetRenderScale(ren, 1.f, 1.f);
    }

    void Game::run()
    {
       auto start = SDL_GetTicks();
       bool quit  = false;

       static const Mask gsMask = MaskBuilder().set<GameStatus>().build();

       while (!quit) {
          bool gameOver = false, won = false;

          if (_state == GameState::Select) {
             select_input();
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
                splash_system();  // tick level-banner countdown during play

                // Camera auto-scroll: after 180-frame grace period, move ~1 tile per 3 s.
                if (_camera_grace > 0) --_camera_grace;
                else _camera_scroll += TILE / (3.f * FPS);

                for (Entity e = Entity::first(); !e.eof(); e.next())
                   if (e.test(gsMask)) { gameOver = e.get<GameStatus>().gameOver; won = e.get<GameStatus>().won; break; }

                if (won && _current_level < 3) {
                   // ── Continuous level transition ──────────────────────────────
                   // Keep the player, hazards and shelters alive.
                   // Spawn the next wave above the current viewport so the camera
                   // naturally scrolls the player toward the new enemies.
                   _current_level++;
                   clear_enemies_only();
                   spawn_enemy_wave();          // resets gs.won internally
                   Entity::create().add(LevelSplash{120});  // non-blocking banner
                   play_sfx(4);
                   won = false;
                }

                // Update session high score as soon as it's beaten.
                if (_score > _high_score) _high_score = _score;
             }
          }
          // Paused state: no system updates — just draw.

          SDL_RenderClear(ren);
          if (_state == GameState::Select) {
             select_draw();
          } else if (_state == GameState::Paused) {
             draw_pause();
          } else {
             draw_system();
             // Overlay the level-banner if one is active (non-blocking, shown during play).
             {
                static const Mask splashMask = MaskBuilder().set<LevelSplash>().build();
                for (Entity e = Entity::first(); !e.eof(); e.next())
                   if (e.test(splashMask)) { draw_level_splash(); break; }
             }
             if (gameOver || (won && _current_level == 3)) endgame_draw();
          }
          SDL_RenderPresent(ren);

          const auto end = SDL_GetTicks();
          if (end - start < GAME_FRAME)
             SDL_Delay(GAME_FRAME - (end - start));
          start += GAME_FRAME;

          SDL_Event e;
          while (SDL_PollEvent(&e)) {
             if (e.type == SDL_EVENT_QUIT) quit = true;

             if (e.type == SDL_EVENT_KEY_DOWN) {
                const SDL_Scancode sc = e.key.scancode;

                // ESC: pause during play, or quit from select/transition/endgame.
                if (sc == SDL_SCANCODE_ESCAPE) {
                   if (_state == GameState::Playing)         _state = GameState::Paused;
                   else if (_state == GameState::Paused)     _state = GameState::Playing;
                   else                                      quit   = true;
                }
                // P: toggle pause only during active play.
                if (sc == SDL_SCANCODE_P) {
                   if (_state == GameState::Playing)         _state = GameState::Paused;
                   else if (_state == GameState::Paused)     _state = GameState::Playing;
                }
                // SPACE: confirm character select.
                if (sc == SDL_SCANCODE_SPACE && _state == GameState::Select) {
                   _state = GameState::Playing;
                   spawn_entities();
                }
                // R: full restart after game over or win.
                if (sc == SDL_SCANCODE_R && (gameOver || won)) {
                   _current_level = 1;
                   reset();
                }
             }
          }
       }
    }
}