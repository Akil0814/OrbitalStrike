#pragma once

#include "../objects/enemy_ship.h"
#include "../objects/moon_cell.h"
#include "../objects/space_anomaly.h"

#include "engine/core/geometry/rect.h"

#include <vector>

namespace game::level
{
enum class GameLevelId : unsigned char { Prototype };

struct GameScenePayload
{
    GameLevelId level_id = GameLevelId::Prototype;
};

struct GameCameraConfig
{
    elysia::core::Rect bounds{};
    float initial_zoom = 0.6f;
    float minimum_zoom = 0.35f;
    float maximum_zoom = 1.6f;
    float pan_speed = 800.0f;
    float cannon_screen_offset_ratio = 0.3f;
};

struct GameLaunchConfig
{
    float minimum_power = 500.0f;
    float maximum_power = 1700.0f;
    float initial_power = 1050.0f;
    float adjustment_rate = 350.0f;
    double bullet_lifetime_seconds = 14.0;
};

struct GameLevelDefinition
{
    elysia::core::Rect activity_bounds{};
    GameCameraConfig camera{};
    GameLaunchConfig launch{};
    game::objects::MoonCellConfig moon_cell{};
    std::vector<game::objects::EnemyShipConfig> ships;
    std::vector<game::objects::SpaceAnomalyConfig> anomalies;
};
}
