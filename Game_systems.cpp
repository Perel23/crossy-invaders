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
    void Game::explosion_system() const
    {
        static const Mask mask = MaskBuilder().set<Explosion>().build();
        static const int  q    = World::createQuery(mask);
        for (Entity e = Entity::firstQ(q); !e.eofQ(q); e.nextQ(q))
            if (--e.get<Explosion>().frames <= 0) e.destroy();
    }

    void Game::pickup_system() const
    {
        static const Mask pickupMask = MaskBuilder().set<Pickup>().set<Transform>().set<Drawable>().build();
        static const Mask playerMask = MaskBuilder().set<PlayerTag>().set<Transform>().set<Drawable>().set<Health>().build();
        static const int  qPickup    = World::createQuery(pickupMask);
        static const int  qPlayer    = World::createQuery(playerMask);

        auto overlaps = [](SDL_FPoint p1, SDL_FPoint s1, SDL_FPoint p2, SDL_FPoint s2) {
            return std::abs(p1.x - p2.x) < (s1.x + s2.x) * 0.5f &&
                   std::abs(p1.y - p2.y) < (s1.y + s2.y) * 0.5f;
        };

        for (Entity pk = Entity::firstQ(qPickup); !pk.eofQ(qPickup); pk.nextQ(qPickup)) {
            const SDL_FPoint pp = pk.get<Transform>().p;
            const SDL_FPoint ps = pk.get<Drawable>().size;
            bool collected = false;
            for (Entity pl = Entity::firstQ(qPlayer); !pl.eofQ(qPlayer); pl.nextQ(qPlayer)) {
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
                } else {
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
        static const int  q    = World::createQuery(mask);
        for (Entity e = Entity::firstQ(q); !e.eofQ(q); e.nextQ(q)) {
            auto& cs = e.get<ComboState>();
            if (cs.timer > 0) cs.timer--;
            return;
        }
    }

    void Game::floating_text_system() const
    {
        static const Mask mask = MaskBuilder().set<FloatingText>().set<Transform>().build();
        static const int  q    = World::createQuery(mask);
        for (Entity e = Entity::firstQ(q); !e.eofQ(q); e.nextQ(q)) {
            e.get<Transform>().p.y -= 1.2f;
            if (--e.get<FloatingText>().frames <= 0) e.destroy();
        }
    }

    void Game::hop_system() const
    {
        static const Mask mask = MaskBuilder().set<HopState>().build();
        static const int  q    = World::createQuery(mask);
        for (Entity e = Entity::firstQ(q); !e.eofQ(q); e.nextQ(q))
            if (e.get<HopState>().frames > 0) e.get<HopState>().frames--;
    }

    void Game::animate_system() const
    {
        static const Mask mask = MaskBuilder().set<BreatheState>().build();
        static const int  q    = World::createQuery(mask);
        constexpr float TWO_PI = 6.28318530f;
        for (Entity e = Entity::firstQ(q); !e.eofQ(q); e.nextQ(q)) {
            auto& bs = e.get<BreatheState>();
            bs.phase += bs.speed;
            if (bs.phase >= TWO_PI) bs.phase -= TWO_PI;
        }
    }

    void Game::blink_system() const
    {
        static const Mask mask = MaskBuilder().set<BlinkPhase>().build();
        static const int  q    = World::createQuery(mask);
        for (Entity e = Entity::firstQ(q); !e.eofQ(q); e.nextQ(q)) {
            auto& bp = e.get<BlinkPhase>();
            if (++bp.counter >= bp.period) {
                bp.counter = 0;
                bp.visible = !bp.visible;
            }
        }
    }

    void Game::input_system() const
    {
        static const Mask mask = MaskBuilder().set<PlayerTag>().set<InputState>().build();
        static const int  q    = World::createQuery(mask);
        const bool* keys = SDL_GetKeyboardState(nullptr);
        for (Entity e = Entity::firstQ(q); !e.eofQ(q); e.nextQ(q)) {
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
        static const Mask mask      = MaskBuilder()
            .set<PlayerTag>().set<InputState>().set<LanePos>().set<Transform>().build();
        static const Mask enemyMask = MaskBuilder().set<EnemyTag>().build();
        static const int  qPlayer   = World::createQuery(mask);
        static const int  qEnemy    = World::createQuery(enemyMask);

        bool anyEnemy = false;
        for (Entity e = Entity::firstQ(qEnemy); !e.eofQ(qEnemy); e.nextQ(qEnemy))
            { anyEnemy = true; break; }

        const int scrollTopLane = static_cast<int>((WIN_H - TILE / 2.f + _camera_scroll) / TILE);
        int max_lane = scrollTopLane - (anyEnemy ? 1 : 0);

        for (Entity e = Entity::firstQ(qPlayer); !e.eofQ(qPlayer); e.nextQ(qPlayer)) {
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
        static const int  q       = World::createQuery(mask);
        static const int  qFS     = World::createQuery(fsMask);
        static const int  qSS     = World::createQuery(ssMask);

        float speedScale = 1.0f;
        for (Entity e = Entity::firstQ(qSS); !e.eofQ(qSS); e.nextQ(qSS))
            { speedScale = e.get<SpeedScale>().active; break; }

        // ── Level 2: independent per-enemy random movement ──
        if (_current_level == 2) {
            static const Mask indvMask = MaskBuilder()
                .set<EnemyTag>().set<LanePos>().set<Transform>().set<IndividualMove>().build();
            static const int  qIndv    = World::createQuery(indvMask);
            const Uint64 now = SDL_GetTicks();
            for (Entity e = Entity::firstQ(qIndv); !e.eofQ(qIndv); e.nextQ(qIndv)) {
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
        Entity fsEnt = Entity::firstQ(qFS);
        auto& fs = fsEnt.get<FormationState>();

        const Uint64 now = SDL_GetTicks();
        Uint64 move_ms = (Uint64)(ENEMY_MOVE_MS / speedScale);
        if (_current_level == 3) move_ms = (Uint64)(move_ms * 0.5f);
        if (_current_level == 4) move_ms = (Uint64)(move_ms * 0.8f);

        // Difficulty ramp within wave
        if (_wave_enemy_count > 0) {
            int remaining = 0;
            for (Entity e = Entity::firstQ(q); !e.eofQ(q); e.nextQ(q)) remaining++;
            move_ms = (Uint64)(move_ms * std::max(0.3f, (float)remaining / _wave_enemy_count));
        }

        if (now - fs.moveTimer < move_ms) return;
        fs.moveTimer = now;

        // Check wall hit
        bool hitEdge = false;
        for (Entity e = Entity::firstQ(q); !e.eofQ(q); e.nextQ(q)) {
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
                for (Entity e = Entity::firstQ(q); !e.eofQ(q); e.nextQ(q)) {
                    e.get<LanePos>().lane++;
                    e.get<Transform>().p.y -= TILE;
                }
            }
        }

        for (Entity e = Entity::firstQ(q); !e.eofQ(q); e.nextQ(q)) {
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
        static const int  qPlayer    = World::createQuery(playerMask);
        static const int  qEnemy     = World::createQuery(enemyMask);
        static const int  qFS        = World::createQuery(fsMask);
        static const int  qGS        = World::createQuery(gsMask);

        // Level 4: player shoots DOWN, enemies shoot UP; levels 1-3: opposite
        const float playerBulletDY = (_current_level == 4) ?  BULLET_SPEED : -BULLET_SPEED;
        const float enemyBulletDY  = (_current_level == 4) ? -BULLET_SPEED :  BULLET_SPEED;

        // Player shooting
        for (Entity e = Entity::firstQ(qPlayer); !e.eofQ(qPlayer); e.nextQ(qPlayer)) {
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
                for (Entity g = Entity::firstQ(qGS); !g.eofQ(qGS); g.nextQ(qGS))
                    { g.get<GameStatus>().shots++; break; }

                const SDL_FPoint p = e.get<Transform>().p;
                const int charId   = e.has<CharacterID>() ? e.get<CharacterID>().id : -1;
                const bool spread  = e.has<SpreadShot>() && e.get<SpreadShot>().frames > 0;

                if (charId == 0) {
                    // Trump — B2 Spirit bomber; respects SpreadShot pickup
                    if (spread) {
                        for (float dx : {-2.5f, 0.f, 2.5f}) {
                            Entity b = Entity::create();
                            b.addAll(Transform{p, 0.f}, Drawable{{0,0,0,0},{36.f,12.f}},
                                     BulletTag{true}, Velocity{dx, playerBulletDY});
                            b.add(TrumpBulletTag{});
                        }
                    } else {
                        Entity b = Entity::create();
                        b.addAll(Transform{p, 0.f}, Drawable{{0,0,0,0},{36.f,12.f}},
                                 BulletTag{true}, Velocity{0.f, playerBulletDY});
                        b.add(TrumpBulletTag{});
                    }
                } else if (charId == 4) {
                    // Putin — nuclear missile; respects SpreadShot and RapidFire
                    if (spread) {
                        for (float dx : {-2.5f, 0.f, 2.5f}) {
                            Entity b = Entity::create();
                            b.addAll(Transform{p, 0.f}, Drawable{{0,0,0,0},{10.f,20.f}},
                                     BulletTag{true}, Velocity{dx, playerBulletDY});
                            b.add(PutinBulletTag{});
                        }
                    } else {
                        Entity b = Entity::create();
                        b.addAll(Transform{p, 0.f}, Drawable{{0,0,0,0},{10.f,20.f}},
                                 BulletTag{true}, Velocity{0.f, playerBulletDY});
                        b.add(PutinBulletTag{});
                    }
                } else if (charId == 11) {
                    // Sara — snowspray.png image; respects SpreadShot and RapidFire
                    if (spread) {
                        for (float dx : {-2.5f, 0.f, 2.5f}) {
                            Entity b = Entity::create();
                            b.addAll(Transform{p, 0.f}, Drawable{{0,0,0,0},{36.f,36.f}},
                                     BulletTag{true}, Velocity{dx, playerBulletDY});
                            b.add(SaraBulletTag{});
                        }
                    } else {
                        Entity b = Entity::create();
                        b.addAll(Transform{p, 0.f}, Drawable{{0,0,0,0},{36.f,36.f}},
                                 BulletTag{true}, Velocity{0.f, playerBulletDY});
                        b.add(SaraBulletTag{});
                    }
                } else if (charId == 1) {
                    // Bibi — mahal.jpg image; respects SpreadShot and RapidFire
                    if (spread) {
                        for (float dx : {-2.5f, 0.f, 2.5f}) {
                            Entity b = Entity::create();
                            b.addAll(Transform{p, 0.f}, Drawable{{0,0,0,0},{36.f,36.f}},
                                     BulletTag{true}, Velocity{dx, playerBulletDY});
                            b.add(BibiBulletTag{});
                        }
                    } else {
                        Entity b = Entity::create();
                        b.addAll(Transform{p, 0.f}, Drawable{{0,0,0,0},{36.f,36.f}},
                                 BulletTag{true}, Velocity{0.f, playerBulletDY});
                        b.add(BibiBulletTag{});
                    }
                } else if (charId == 2) {
                    // Ben Gvir — Torah scroll; respects SpreadShot and RapidFire
                    if (spread) {
                        for (float dx : {-2.5f, 0.f, 2.5f}) {
                            Entity b = Entity::create();
                            b.addAll(Transform{p, 0.f}, Drawable{{0,0,0,0},{12.f,16.f}},
                                     BulletTag{true}, Velocity{dx, playerBulletDY});
                            b.add(BenGvirBulletTag{});
                        }
                    } else {
                        Entity b = Entity::create();
                        b.addAll(Transform{p, 0.f}, Drawable{{0,0,0,0},{12.f,16.f}},
                                 BulletTag{true}, Velocity{0.f, playerBulletDY});
                        b.add(BenGvirBulletTag{});
                    }
                } else if (spread) {
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
        Entity fsEnt = Entity::firstQ(qFS);
        auto& fs = fsEnt.get<FormationState>();

        const Uint64 now = SDL_GetTicks();
        static const Mask ssMask2 = MaskBuilder().set<SpeedScale>().build();
        static const int  qSS2    = World::createQuery(ssMask2);
        float shootSpeedScale = 1.0f;
        for (Entity e = Entity::firstQ(qSS2); !e.eofQ(qSS2); e.nextQ(qSS2))
            { shootSpeedScale = e.get<SpeedScale>().active; break; }

        Uint64 shoot_ms = (Uint64)(ENEMY_SHOOT_MS / shootSpeedScale);
        if (_current_level == 2) shoot_ms = (Uint64)(shoot_ms * 0.67f);
        if (_current_level == 3) shoot_ms = (Uint64)(shoot_ms * 0.33f);
        if (_current_level == 4) shoot_ms = (Uint64)(shoot_ms * 0.75f);
        if (now - fs.shootTimer < shoot_ms) return;
        fs.shootTimer = now;

        int targetLane = (_current_level == 4) ? 0 : LANES;
        for (Entity e = Entity::firstQ(qEnemy); !e.eofQ(qEnemy); e.nextQ(qEnemy)) {
            const int lane = e.get<LanePos>().lane;
            if (_current_level == 4) { if (lane > targetLane) targetLane = lane; }
            else                     { if (lane < targetLane) targetLane = lane; }
        }

        struct ShooterSlot { SDL_FPoint pos; };
        std::vector<ShooterSlot> pool;
        for (Entity e = Entity::firstQ(qEnemy); !e.eofQ(qEnemy); e.nextQ(qEnemy)) {
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
        static const int  q      = World::createQuery(mask);
        static const int  qSS    = World::createQuery(ssMask);

        float speedScale = 1.0f;
        for (Entity e = Entity::firstQ(qSS); !e.eofQ(qSS); e.nextQ(qSS))
            { speedScale = e.get<SpeedScale>().active; break; }

        for (Entity e = Entity::firstQ(q); !e.eofQ(q); e.nextQ(q)) {
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
        static const int  q      = World::createQuery(mask);
        static const int  qSS    = World::createQuery(ssMask);

        float speedScale = 1.0f;
        for (Entity e = Entity::firstQ(qSS); !e.eofQ(qSS); e.nextQ(qSS))
            { speedScale = e.get<SpeedScale>().active; break; }

        for (Entity e = Entity::firstQ(q); !e.eofQ(q); e.nextQ(q)) {
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
        static const Mask mask     = MaskBuilder().set<PlayerTag>().set<InputState>().set<Shield>().build();
        static const Mask smMask   = MaskBuilder().set<SlowMo>().build();
        static const Mask wallMask = MaskBuilder().set<WallTag>().build();
        static const int  q      = World::createQuery(mask);
        static const int  qSM    = World::createQuery(smMask);
        static const int  qWall  = World::createQuery(wallMask);

        // Decay Trump's walls
        for (Entity w = Entity::firstQ(qWall); !w.eofQ(qWall); w.nextQ(qWall))
            if (--w.get<WallTag>().frames <= 0) w.destroy();

        for (Entity e = Entity::firstQ(q); !e.eofQ(q); e.nextQ(q)) {
            auto& s  = e.get<InputState>();
            auto& sh = e.get<Shield>();

            // Shield (Iron Dome)
            if (s.activate && !s.activateFired && sh.charges > 0 && sh.timer <= 0.f) {
                sh.timer = SHIELD_DURATION; sh.charges--;
                s.activateFired = true;
                // Trigger flash on the icon slot that just disappeared
                if (e.has<ShieldFlash>()) e.get<ShieldFlash>().frames = 45;

                // Trump: "Build the Wall" — spawn a brick wall above the player
                if (e.has<CharacterID>() && e.get<CharacterID>().id == 0) {
                    const SDL_FPoint p = e.get<Transform>().p;
                    const float wallOffY = (_current_level == 4) ? 96.f : -96.f;
                    Entity::create().addAll(
                        Transform{{p.x, p.y + wallOffY}, 0.f},
                        Drawable{{0,0,0,0}, {64.f, 128.f}},
                        WallTag{300});
                    Entity::create().add(SoundEvent{20}); // wall-spawn thud
                }
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
            for (Entity sm = Entity::firstQ(qSM); !sm.eofQ(qSM); sm.nextQ(qSM)) {
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
        static const Mask playerMask = MaskBuilder().set<PlayerTag>().set<Transform>().set<Drawable>().set<Health>().build();
        static const Mask enemyMask  = MaskBuilder().set<EnemyTag>().set<Transform>().set<Drawable>().set<Health>().build();
        static const Mask bulletMask = MaskBuilder().set<BulletTag>().set<Transform>().set<Drawable>().build();
        static const Mask shelterMask= MaskBuilder().set<Shelter>().set<Transform>().set<Drawable>().set<Health>().build();
        static const int  qGS        = World::createQuery(gsMask);
        static const int  qPlayer    = World::createQuery(playerMask);
        static const int  qEnemy     = World::createQuery(enemyMask);
        static const int  qBullet    = World::createQuery(bulletMask);
        static const int  qShelter   = World::createQuery(shelterMask);

        Entity gsEnt = Entity::firstQ(qGS);
        auto& gs = gsEnt.get<GameStatus>();
        if (gs.gameOver || gs.won) return;

        // Decrement invincibility + DamageFlash
        for (Entity e = Entity::firstQ(qPlayer); !e.eofQ(qPlayer); e.nextQ(qPlayer)) {
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
        for (Entity b = Entity::firstQ(qBullet); !b.eofQ(qBullet); b.nextQ(qBullet)) {
            const SDL_FPoint bp = b.get<Transform>().p;
            const SDL_FPoint bs = b.get<Drawable>().size;
            for (Entity sh = Entity::firstQ(qShelter); !sh.eofQ(qShelter); sh.nextQ(qShelter)) {
                if (!overlaps(bp, bs, sh.get<Transform>().p, sh.get<Drawable>().size)) continue;
                b.destroy();
                if (--sh.get<Health>().hp <= 0) sh.destroy();
                break;
            }
        }

        // Enemy bullet × Trump's wall
        static const Mask wallColMask = MaskBuilder().set<WallTag>().set<Transform>().set<Drawable>().build();
        static const int  qWall       = World::createQuery(wallColMask);
        for (Entity b = Entity::firstQ(qBullet); !b.eofQ(qBullet); b.nextQ(qBullet)) {
            if (b.get<BulletTag>().fromPlayer) continue;
            const SDL_FPoint bp = b.get<Transform>().p;
            const SDL_FPoint bs = b.get<Drawable>().size;
            for (Entity w = Entity::firstQ(qWall); !w.eofQ(qWall); w.nextQ(qWall)) {
                if (!overlaps(bp, bs, w.get<Transform>().p, w.get<Drawable>().size)) continue;
                b.destroy();
                Entity::create().add(SoundEvent{19});
                break;
            }
        }

        // Player bullet × enemy
        for (Entity b = Entity::firstQ(qBullet); !b.eofQ(qBullet); b.nextQ(qBullet)) {
            if (!b.get<BulletTag>().fromPlayer) continue;  // value check, not mask
            const SDL_FPoint bp = b.get<Transform>().p;
            const SDL_FPoint bs = b.get<Drawable>().size;
            for (Entity en = Entity::firstQ(qEnemy); !en.eofQ(qEnemy); en.nextQ(qEnemy)) {
                if (!overlaps(bp, bs, en.get<Transform>().p, en.get<Drawable>().size)) continue;
                b.destroy();
                gs.hits++;
                if (--en.get<Health>().hp <= 0) kill_enemy(en);
                else Entity::create().add(SoundEvent{1});
                break;
            }
        }

        // Enemy bullet × player  (tight hitbox)
        for (Entity b = Entity::firstQ(qBullet); !b.eofQ(qBullet); b.nextQ(qBullet)) {
            if (b.get<BulletTag>().fromPlayer) continue;  // value check
            const SDL_FPoint bp = b.get<Transform>().p;
            const SDL_FPoint bs = b.get<Drawable>().size;
            for (Entity pl = Entity::firstQ(qPlayer); !pl.eofQ(qPlayer); pl.nextQ(qPlayer)) {
                if (!overlaps_tight(bp, bs, pl.get<Transform>().p, pl.get<Drawable>().size)) continue;
                if (pl.get<Shield>().timer > 0.f) { b.destroy(); break; }
                b.destroy();
                hurt_player(pl);
                break;
            }
        }

        // Hazard × player  (tight hitbox)
        static const Mask hazardMask = MaskBuilder().set<Hazard>().set<Transform>().set<Drawable>().build();
        static const int  qHazard    = World::createQuery(hazardMask);
        for (Entity h = Entity::firstQ(qHazard); !h.eofQ(qHazard); h.nextQ(qHazard)) {
            for (Entity pl = Entity::firstQ(qPlayer); !pl.eofQ(qPlayer); pl.nextQ(qPlayer)) {
                if (!overlaps_tight(h.get<Transform>().p, h.get<Drawable>().size,
                                    pl.get<Transform>().p, pl.get<Drawable>().size)) continue;
                hurt_player(pl);
                break;
            }
        }

        // Enemy body × player
        for (Entity en = Entity::firstQ(qEnemy); !en.eofQ(qEnemy); en.nextQ(qEnemy)) {
            for (Entity pl = Entity::firstQ(qPlayer); !pl.eofQ(qPlayer); pl.nextQ(qPlayer)) {
                if (!overlaps(en.get<Transform>().p, en.get<Drawable>().size,
                              pl.get<Transform>().p, pl.get<Drawable>().size)) continue;
                gs.gameOver = true; return;
            }
        }

        // Camera-scroll death (levels 1-3)
        if (_current_level != 4) {
            for (Entity pl = Entity::firstQ(qPlayer); !pl.eofQ(qPlayer); pl.nextQ(qPlayer)) {
                if (pl.get<Invincibility>().frames > 0) break;
                const SDL_FPoint& pPos = pl.get<Transform>().p;
                if (pPos.y + _camera_scroll + (pPos.x - WIN_W * 0.5f) * TILT > WIN_H + TILE / 2.f) {
                    gs.gameOver = true; return;
                }
                break;
            }
        }

        // Level 4: game over if any enemy reaches lane >= 8
        if (_current_level == 4) {
            static const Mask lpEMask = MaskBuilder().set<EnemyTag>().set<LanePos>().build();
            static const int  qLPE    = World::createQuery(lpEMask);
            for (Entity en = Entity::firstQ(qLPE); !en.eofQ(qLPE); en.nextQ(qLPE)) {
                if (en.get<LanePos>().lane >= 8) { gs.gameOver = true; return; }
            }
        }

        // Win condition
        bool anyEnemy = false;
        for (Entity e = Entity::firstQ(qEnemy); !e.eofQ(qEnemy); e.nextQ(qEnemy))
            { anyEnemy = true; break; }
        if (!anyEnemy) { gs.won = true; gs.waveEndTime = SDL_GetTicks(); }
    }

} // namespace ci
