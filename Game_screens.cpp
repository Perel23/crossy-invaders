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

} // namespace ci
