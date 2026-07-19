#pragma once
#include "Game.h"

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
    inline float diff_base_scale(int d) { return d == 0 ? 0.82f : d == 2 ? 1.18f : 1.0f; }
}
