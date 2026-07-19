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
    void Game::map_screen_system() const
    {
        static const Mask mask     = MaskBuilder().set<MapScreen>().build();
        static const int  q        = World::createQuery(mask);
        static const Mask plInvMask= MaskBuilder().set<PlayerTag>().set<Invincibility>().build();
        static const int  qPlInv   = World::createQuery(plInvMask);
        for (Entity e = Entity::firstQ(q); !e.eofQ(q); e.nextQ(q)) {
            auto& ms = e.get<MapScreen>();
            if (ms.planeT < 1.f)
                ms.planeT = std::min(1.f, ms.planeT + 1.f / 180.f);
            if (--ms.framesLeft <= 0) {
                e.destroy();
                const int splashFrames = (_current_level == 4 ? 300 : 150);
                Entity::create().add(LevelSplash{splashFrames});
                for (Entity p = Entity::firstQ(qPlInv); !p.eofQ(qPlInv); p.nextQ(qPlInv)) {
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
        const WayPoint trump_route[] = {       // 0
            {120.f, 390.f, "WASHINGTON"},
            {350.f, 270.f, "LONDON"},
            {640.f, 340.f, "DUBAI"},
            {890.f, 360.f, "TEHRAN"},
        };
        const WayPoint bibi_route[] = {        // 1
            {130.f, 410.f, "JERUSALEM"},
            {380.f, 385.f, "AMMAN"},
            {630.f, 360.f, "BAGHDAD"},
            {880.f, 345.f, "TEHRAN"},
        };
        const WayPoint bengvir_route[] = {     // 2
            {130.f, 410.f, "JERUSALEM"},
            {380.f, 385.f, "AMMAN"},
            {630.f, 360.f, "BAGHDAD"},
            {880.f, 345.f, "TEHRAN"},
        };
        const WayPoint zelensky_route[] = {    // 3
            {290.f, 285.f, "KYIV"},
            {440.f, 315.f, "ISTANBUL"},
            {650.f, 345.f, "BAGHDAD"},
            {880.f, 345.f, "TEHRAN"},
        };
        const WayPoint putin_route[] = {       // 4
            {185.f, 250.f, "MOSCOW"},
            {470.f, 290.f, "BAKU"},
            {650.f, 345.f, "BAGHDAD"},
            {880.f, 345.f, "TEHRAN"},
        };
        const WayPoint obama_route[] = {       // 5
            { 55.f, 435.f, "HAWAII"},
            {150.f, 375.f, "NEW YORK"},
            {350.f, 270.f, "LONDON"},
            {880.f, 345.f, "TEHRAN"},
        };
        const WayPoint eminem_route[] = {      // 6
            {115.f, 365.f, "DETROIT"},
            {350.f, 270.f, "LONDON"},
            {530.f, 320.f, "ISTANBUL"},
            {880.f, 345.f, "TEHRAN"},
        };
        const WayPoint madonna_route[] = {     // 7
            {150.f, 375.f, "NEW YORK"},
            {350.f, 270.f, "LONDON"},
            {640.f, 335.f, "DUBAI"},
            {880.f, 345.f, "TEHRAN"},
        };
        const WayPoint jackson_route[] = {     // 8
            { 70.f, 405.f, "NEVERLAND"},
            {150.f, 375.f, "NEW YORK"},
            {350.f, 270.f, "LONDON"},
            {880.f, 345.f, "TEHRAN"},
        };
        const WayPoint yoamashit_route[] = {   // 9
            {130.f, 410.f, "JERUSALEM"},
            {380.f, 385.f, "AMMAN"},
            {630.f, 360.f, "BAGHDAD"},
            {880.f, 345.f, "TEHRAN"},
        };
        const WayPoint stalin_route[] = {      // 10
            {185.f, 250.f, "MOSCOW"},
            {470.f, 290.f, "BAKU"},
            {650.f, 345.f, "BAGHDAD"},
            {880.f, 345.f, "TEHRAN"},
        };
        const WayPoint sara_route[] = {        // 11
            {130.f, 410.f, "JERUSALEM"},
            {380.f, 385.f, "AMMAN"},
            {630.f, 360.f, "BAGHDAD"},
            {880.f, 345.f, "TEHRAN"},
        };
        const WayPoint* const all_routes[Game::NUM_CHARS] = {
            trump_route, bibi_route, bengvir_route, zelensky_route,
            putin_route, obama_route, eminem_route, madonna_route,
            jackson_route, yoamashit_route, stalin_route, sara_route,
        };
        const WayPoint* route = all_routes[selected < Game::NUM_CHARS ? selected : 0];
        static const char* const char_names[Game::NUM_CHARS] = {
            "TRUMP", "BIBI", "BEN-GVIR", "ZELENSKY",
            "PUTIN", "OBAMA", "EMINEM", "MADONNA",
            "M. JACKSON", "SARA", "STALIN", "YOAMASHIT"
        };
        const char* charName = char_names[selected < Game::NUM_CHARS ? selected : 0];

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
            const float angle = std::atan2(ny - py, nx - px);
            const float ca = std::cos(angle), sa = std::sin(angle);
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
            SDL_RenderDebugText(ren, (WIN_W / 2.f - 84.f) / 1.5f, (WIN_H - 52.f) / 1.5f, "ENTER to skip");
            SDL_SetRenderScale(ren, 1.f, 1.f);
        }
    }

} // namespace ci
