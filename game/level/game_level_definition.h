#pragma once

#include "../objects/celestial_body.h"

#include "engine/core/geometry/rect.h"

#include <vector>

namespace game::level
{
enum class GameLevelId : unsigned char
{
    Prototype
};

struct GameScenePayload
{
    GameLevelId level_id = GameLevelId::Prototype;
};

struct GameLevelDefinition
{
    elysia::core::Rect activity_bounds{};
    float initial_zoom = 0.8f;
    float initial_power = 700.0f;
    game::objects::CelestialBodyConfig player{};
    std::vector<game::objects::CelestialBodyConfig> bodies;
};
}
