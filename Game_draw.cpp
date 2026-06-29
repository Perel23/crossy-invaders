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
    void Game::draw_system() const
    {
        const bool bench = (_state == GameState::Benchmark);

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

        if (bench) {
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_FRect full = {0, 0, (float)WIN_W, (float)WIN_H};
            SDL_RenderFillRect(ren, &full);
        } else {
            draw_background();
        }

        // Compute minimum enemy lane for per-row depth tinting
        static const Mask em2 = MaskBuilder().set<EnemyTag>().set<LanePos>().build();
        int minEnemyLane = 99, maxEnemyLane = -1;
        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (!e.test(em2)) continue;
            int lane = e.get<LanePos>().lane;
            if (lane < minEnemyLane) minEnemyLane = lane;
            if (lane > maxEnemyLane) maxEnemyLane = lane;
        }

        static const Mask drawMask       = MaskBuilder().set<Transform>().set<Drawable>().build();
        static const Mask playerMask     = MaskBuilder().set<PlayerTag>().set<Shield>().build();
        static const Mask bossMask       = MaskBuilder().set<BossTag>().set<Health>().build();
        static const Mask enemyMask      = MaskBuilder().set<EnemyTag>().set<Health>().build();
        static const Mask bulletMask     = MaskBuilder().set<BulletTag>().build();
        static const Mask shelterDMsk    = MaskBuilder().set<Shelter>().set<Health>().build();
        static const Mask hazardVisMsk   = MaskBuilder().set<Hazard>().set<HazardVisual>().build();
        static const Mask explMask       = MaskBuilder().set<Explosion>().set<Transform>().build();
        static const Mask pickupMask     = MaskBuilder().set<Pickup>().set<Transform>().build();
        static const Mask ftMask         = MaskBuilder().set<FloatingText>().set<Transform>().build();
        static const Mask lpEMask2       = MaskBuilder().set<EnemyTag>().set<LanePos>().set<Health>().build();
        static const Mask worldMask      = MaskBuilder().set<Transform>().build();
        static const Mask wallDrwMask    = MaskBuilder().set<WallTag>().set<Transform>().set<Drawable>().build();
        static const Mask trumpBulletMsk   = MaskBuilder().set<TrumpBulletTag>().build();
        static const Mask putinBulletMsk   = MaskBuilder().set<PutinBulletTag>().build();
        static const Mask benGvirBulletMsk = MaskBuilder().set<BenGvirBulletTag>().build();
        static const Mask bibiBulletMsk    = MaskBuilder().set<BibiBulletTag>().build();
        static const Mask saraBulletMsk    = MaskBuilder().set<SaraBulletTag>().build();

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

                if (!bench && type == 2 && hearts_texture) {
                    float tw = 0, th = 0; SDL_GetTextureSize(hearts_texture, &tw, &th);
                    SDL_FRect srcHeart = {0.f, 0.f, tw/3.f, th/3.f};
                    SDL_RenderTexture(ren, hearts_texture, &srcHeart, &pr);
                } else if (!bench && type == 1 && iron_dome_icon_texture) {
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
                if (!bench && iron_dome_texture) {
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
                const bool spriteOn = !(dfFrames > 0 && (dfFrames / 3) % 2 == 1);
                if (spriteOn) {
                    if (!bench && player_texture) {
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
                if (!bench && fp && e.test(trumpBulletMsk)) {
                    // B2 Spirit bomber: wide stealth-grey chevron pointing in direction of travel
                    const float cx  = dest.x + dest.w * 0.5f;
                    const float tip = dest.y;
                    const float bas = dest.y + dest.h;
                    const float hw  = dest.w * 0.5f;
                    SDL_Vertex verts[3] = {
                        {{cx,        tip}, {55, 60, 70, 255}, {0,0}},
                        {{cx - hw,   bas}, {35, 40, 50, 255}, {0,0}},
                        {{cx + hw,   bas}, {35, 40, 50, 255}, {0,0}},
                    };
                    SDL_RenderGeometry(ren, nullptr, verts, 3, nullptr, 0);
                    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
                    SDL_SetRenderDrawColor(ren, 120, 200, 255, 160);
                    SDL_FRect glow = {cx - 3.f, bas - 3.f, 6.f, 4.f};
                    SDL_RenderFillRect(ren, &glow);
                    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
                } else if (!bench && fp && e.test(putinBulletMsk)) {
                    // Putin's nuclear missile: red warhead + olive body + fins + exhaust
                    const float cx  = dest.x + dest.w * 0.5f;
                    const float top = dest.y;
                    const float bot = dest.y + dest.h;
                    const float hw  = dest.w * 0.5f;
                    SDL_Vertex nose[3] = {
                        {{cx,        top},        {220, 30,  30, 255}, {0,0}},
                        {{cx - hw,   top + 5.f},  {180, 30,  30, 255}, {0,0}},
                        {{cx + hw,   top + 5.f},  {180, 30,  30, 255}, {0,0}},
                    };
                    SDL_RenderGeometry(ren, nullptr, nose, 3, nullptr, 0);
                    SDL_SetRenderDrawColor(ren, 80, 100, 50, 255);
                    SDL_FRect body = {cx - hw, top + 5.f, dest.w, bot - top - 9.f};
                    SDL_RenderFillRect(ren, &body);
                    SDL_Vertex lf[3] = {
                        {{cx - hw,        bot - 5.f}, {60, 80, 35, 255}, {0,0}},
                        {{cx - hw * 2.5f, bot},       {50, 70, 30, 255}, {0,0}},
                        {{cx - hw,        bot},        {60, 80, 35, 255}, {0,0}},
                    };
                    SDL_RenderGeometry(ren, nullptr, lf, 3, nullptr, 0);
                    SDL_Vertex rf[3] = {
                        {{cx + hw,        bot - 5.f}, {60, 80, 35, 255}, {0,0}},
                        {{cx + hw,        bot},        {60, 80, 35, 255}, {0,0}},
                        {{cx + hw * 2.5f, bot},        {50, 70, 30, 255}, {0,0}},
                    };
                    SDL_RenderGeometry(ren, nullptr, rf, 3, nullptr, 0);
                    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
                    SDL_SetRenderDrawColor(ren, 255, 130, 30, 180);
                    SDL_FRect exh = {cx - 3.f, bot - 2.f, 6.f, 5.f};
                    SDL_RenderFillRect(ren, &exh);
                    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
                } else if (!bench && fp && e.test(bibiBulletMsk)) {
                    if (bibi_bullet_tex)
                        SDL_RenderTexture(ren, bibi_bullet_tex, nullptr, &dest);
                    else {
                        SDL_SetRenderDrawColor(ren, 0, 100, 200, 255);
                        SDL_RenderFillRect(ren, &dest);
                    }
                } else if (!bench && fp && e.test(saraBulletMsk)) {
                    if (sara_bullet_tex)
                        SDL_RenderTexture(ren, sara_bullet_tex, nullptr, &dest);
                    else {
                        SDL_SetRenderDrawColor(ren, 180, 220, 255, 255);
                        SDL_RenderFillRect(ren, &dest);
                    }
                } else if (!bench && fp && e.test(benGvirBulletMsk)) {
                    const float cx  = dest.x + dest.w * 0.5f;
                    const float top = dest.y;
                    const float bot = dest.y + dest.h;
                    const float hw  = dest.w * 0.5f;
                    SDL_SetRenderDrawColor(ren, 101, 67, 33, 255);
                    SDL_FRect topHandle = {cx - hw * 1.8f, top,        hw * 3.6f, 3.f};
                    SDL_FRect botHandle = {cx - hw * 1.8f, bot - 3.f,  hw * 3.6f, 3.f};
                    SDL_RenderFillRect(ren, &topHandle);
                    SDL_RenderFillRect(ren, &botHandle);
                    SDL_SetRenderDrawColor(ren, 240, 210, 150, 255);
                    SDL_FRect scroll = {cx - hw, top + 3.f, dest.w, dest.h - 6.f};
                    SDL_RenderFillRect(ren, &scroll);
                    SDL_SetRenderDrawColor(ren, 70, 40, 10, 255);
                    for (int line = 0; line < 3; line++) {
                        SDL_FRect ln = {cx - hw + 1.f, top + 5.f + line * 3.5f, dest.w - 2.f, 1.f};
                        SDL_RenderFillRect(ren, &ln);
                    }
                } else {
                    // Benchmark plain bullet, or any unhandled bullet type
                    SDL_SetRenderDrawColor(ren, 255, fp ? 255 : 140, 0, 255);
                    SDL_RenderFillRect(ren, &dest);
                }
            } else if (e.test(wallDrwMask)) {
                if (bench) {
                    SDL_SetRenderDrawColor(ren, 140, 140, 140, 255);
                    SDL_RenderFillRect(ren, &dest);
                } else {
                    // Trump's "Build the Wall" — animated brick pattern with fade-out
                    const int wFrames = e.get<WallTag>().frames;
                    const Uint8 a = (Uint8)(std::min(1.f, wFrames / 30.f) * 220.f);
                    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
                    const float wx = dest.x, wy = dest.y, ww = dest.w, wh = dest.h;
                    const float brickH = 18.f;
                    const int numRows = (int)(wh / brickH) + 1;
                    for (int row = 0; row < numRows; row++) {
                        const float rowY = wy + row * brickH;
                        const float rowH = std::min(brickH - 2.f, wy + wh - rowY - 2.f);
                        if (rowH <= 0.f) break;
                        const float halfBrick = ww * 0.5f;
                        const float startX = (row % 2 == 0) ? wx : wx - halfBrick * 0.5f;
                        for (float bx = startX; bx < wx + ww; bx += halfBrick) {
                            const float bLeft  = std::max(bx,              wx);
                            const float bRight = std::min(bx + halfBrick - 2.f, wx + ww);
                            if (bRight <= bLeft) continue;
                            SDL_SetRenderDrawColor(ren, 190, 75, 25, a);
                            SDL_FRect brick = {bLeft, rowY, bRight - bLeft, rowH};
                            SDL_RenderFillRect(ren, &brick);
                        }
                    }
                    SDL_SetRenderDrawColor(ren, 230, 120, 60, a);
                    SDL_FRect hi = {wx, wy, 2.f, wh};
                    SDL_RenderFillRect(ren, &hi);
                    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
                }
            } else if (e.test(shelterDMsk)) {
                const Uint8 v = (Uint8)(80 + e.get<Health>().hp * 35);
                if (!bench && shelter_texture) {
                    SDL_SetTextureColorMod(shelter_texture, v, v, v);
                    SDL_RenderTexture(ren, shelter_texture, nullptr, &dest);
                    SDL_SetTextureColorMod(shelter_texture, 255, 255, 255);
                } else {
                    SDL_SetRenderDrawColor(ren, v, v, v, 255); SDL_RenderFillRect(ren, &dest);
                }
            } else if (e.test(hazardVisMsk)) {
                const int idx = e.get<HazardVisual>().tex_index;
                const float hcx = screenX;
                const float wobble = e.has<BreatheState>()
                    ? std::sin(e.get<BreatheState>().phase) * e.get<BreatheState>().amplitude
                    : 0.f;
                const float hcy = t.p.y + _camera_scroll + tiltOff - wobble;
                const float hw = d.size.x * 0.5f;
                const float hh = sqH * 0.5f;
                const float sh = hw * TILT;
                if (!bench && haz_textures[idx]) {
                    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
                    const SDL_FColor wh = {1.f, 1.f, 1.f, 1.f};
                    SDL_Vertex v[4] = {
                        {{hcx - hw, hcy - hh - sh}, wh, {0.f, 0.f}},
                        {{hcx + hw, hcy - hh + sh}, wh, {1.f, 0.f}},
                        {{hcx + hw, hcy + hh + sh}, wh, {1.f, 1.f}},
                        {{hcx - hw, hcy + hh - sh}, wh, {0.f, 1.f}},
                    };
                    const int hidx[6] = {0,1,2, 0,2,3};
                    SDL_RenderGeometry(ren, haz_textures[idx], v, 4, hidx, 6);
                    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
                } else { SDL_SetRenderDrawColor(ren, 0, 200, 220, 255); SDL_RenderFillRect(ren, &dest); }
            } else if (e.test(bossMask)) {
                const int hp = e.get<Health>().hp;
                const Uint8 g = (Uint8)(255 * hp / BOSS_HP);
                if (!bench && boss_texture) {
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
                if (hp < maxHp) { r = 255; g = 80; b = 80; }
                if (!bench && enemy_texture) {
                    SDL_SetTextureColorMod(enemy_texture, r, g, b);
                    SDL_RenderTexture(ren, enemy_texture, nullptr, &dest);
                    SDL_SetTextureColorMod(enemy_texture, 255, 255, 255);
                } else {
                    SDL_SetRenderDrawColor(ren, bench ? 200 : r, bench ? 200 : 50, bench ? 200 : b, 255);
                    SDL_RenderFillRect(ren, &dest);
                }
            } else if (e.test(enemyMask)) {
                if (!bench && enemy_texture) SDL_RenderTexture(ren, enemy_texture, nullptr, &dest);
                else { SDL_SetRenderDrawColor(ren, bench ? 200 : 220, bench ? 200 : 50, bench ? 200 : 50, 255); SDL_RenderFillRect(ren, &dest); }
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

            if (!bench && hearts_texture) {
                float tw = 0, th = 0; SDL_GetTextureSize(hearts_texture, &tw, &th);
                const float cw = tw/3.f, ch = th/3.f;
                const SDL_FRect srcFull  = {0.f, 0.f, cw, ch};
                const SDL_FRect srcEmpty = {2.f*cw, ch, cw, ch};
                const int dfFrames = e.has<DamageFlash>() ? e.get<DamageFlash>().frames : 0;
                for (int i = 0; i < 3; i++) {
                    const SDL_FRect dst = {8.f + i*30.f, 6.f, 26.f, 26.f};
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
                if (!bench && iron_dome_icon_texture) {
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

} // namespace ci
