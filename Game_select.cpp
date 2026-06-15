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
        int selected = 0, difficulty = 1, start_level = 1;
        for (Entity e = Entity::first(); !e.eof(); e.next())
            if (e.test(ssMask)) {
                selected    = e.get<SelectState>().selected;
                difficulty  = e.get<SelectState>().difficulty;
                start_level = e.get<SelectState>().start_level;
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

        // ── START LEVEL selector (Y: 630) — keys 1-4 ──
        {
            SDL_SetRenderScale(ren, 1.5f, 1.5f);
            SDL_SetRenderDrawColor(ren, 130, 130, 180, 255);
            SDL_RenderDebugText(ren, 16.f, 630.f / 1.5f, "START LEVEL:");
            SDL_SetRenderScale(ren, 1.f, 1.f);

            const float btnW = 46.f, btnH = 26.f, btnGap = 8.f;
            const float btnStartX = 212.f;
            const float btnY = 625.f;
            for (int lvl = 1; lvl <= 4; lvl++) {
                const bool sel = (lvl == start_level);
                // Border
                SDL_SetRenderDrawColor(ren, sel ? (Uint8)255 : (Uint8)60,
                                             sel ? (Uint8)255 : (Uint8)60,
                                             sel ? (Uint8)80  : (Uint8)90, 255);
                SDL_FRect bord = {btnStartX + (lvl-1)*(btnW+btnGap) - 2.f, btnY - 2.f, btnW + 4.f, btnH + 4.f};
                SDL_RenderFillRect(ren, &bord);
                // Fill
                SDL_SetRenderDrawColor(ren, sel ? (Uint8)40 : (Uint8)15,
                                             sel ? (Uint8)80 : (Uint8)15,
                                             sel ? (Uint8)140: (Uint8)30, 255);
                SDL_FRect btn = {btnStartX + (lvl-1)*(btnW+btnGap), btnY, btnW, btnH};
                SDL_RenderFillRect(ren, &btn);
                // Label
                char lbuf[4]; SDL_snprintf(lbuf, sizeof(lbuf), "%d", lvl);
                SDL_SetRenderScale(ren, 2.f, 2.f);
                SDL_SetRenderDrawColor(ren, sel ? (Uint8)120 : (Uint8)80,
                                             sel ? (Uint8)200 : (Uint8)80,
                                             sel ? (Uint8)255 : (Uint8)120, 255);
                SDL_RenderDebugText(ren,
                    (btnStartX + (lvl-1)*(btnW+btnGap) + btnW/2.f - 8.f) / 2.f,
                    (btnY + (btnH - 16.f) / 2.f) / 2.f, lbuf);
                SDL_SetRenderScale(ren, 1.f, 1.f);
            }
            // Hint
            SDL_SetRenderScale(ren, 1.2f, 1.2f);
            SDL_SetRenderDrawColor(ren, 70, 70, 100, 255);
            SDL_RenderDebugText(ren, (btnStartX + 4*(btnW+btnGap) + 6.f) / 1.2f, 630.f / 1.2f, "keys 1-4");
            SDL_SetRenderScale(ren, 1.f, 1.f);
        }

        // ── Blinking "PRESS SPACE TO PLAY"  (Y: 660) ──
        if ((SDL_GetTicks() / 500) % 2 == 0) {
            SDL_SetRenderScale(ren, 2.f, 2.f);
            SDL_SetRenderDrawColor(ren, 255, 200, 50, 255);
            SDL_RenderDebugText(ren, 168.f, 660.f / 2.f, "PRESS  SPACE  TO  PLAY");
            SDL_SetRenderScale(ren, 1.f, 1.f);
        }

        // ── "PRESS B → BENCHMARK" hint (Y: 688) ──
        {
            SDL_SetRenderScale(ren, 1.4f, 1.4f);
            SDL_SetRenderDrawColor(ren, 255, 120, 30, 255);
            // "B" key highlight
            SDL_RenderDebugText(ren, 296.f, 688.f / 1.4f, "B");
            SDL_SetRenderDrawColor(ren, 160, 160, 190, 255);
            SDL_RenderDebugText(ren, 308.f, 688.f / 1.4f, "= Performance Benchmark");
            SDL_SetRenderScale(ren, 1.f, 1.f);
        }
    }

} // namespace ci
