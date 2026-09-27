#pragma once

#include "game_map_config.h"
#include "../gameplay/anomaly/space_anomaly.h"
#include "../gameplay/fleet/enemy_ship.h"
#include "../gameplay/launcher/moon_cell.h"
#include "../gameplay/projectile/projectile_definition.h"

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
    elysia::core::Rect aiming_bounds{};
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
    game::projectile::ProjectileDefinition projectile{.lifetime_seconds = 20.0};
};

struct GameMissionConfig
{
    int maximum_rounds = 10;
    double flagship_warning_seconds = 1.25;
    double flagship_firing_seconds = 0.45;
};

struct GameLevelDefinition
{
    GameMapConfig map{};
    GameCameraConfig camera{};
    GameLaunchConfig launch{};
    GameMissionConfig mission{};
    game::launcher::MoonCellConfig moon_cell{};
    std::vector<game::fleet::EnemyShipConfig> ships;
    std::vector<game::anomaly::SpaceAnomalyDefinition> anomalies;
};
}
