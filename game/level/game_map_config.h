#pragma once

#include "../objects/starfield_config.h"
#include "engine/core/geometry/rect.h"

namespace game::level
{
enum class GameMapSize : unsigned char
{
    Small,
    Standard,
    Large
};

struct GameMapConfig
{
    elysia::core::Rect activity_bounds{};
    elysia::core::Rect projectile_bounds{};
    elysia::core::Rect backdrop_bounds{};
    game::objects::StarfieldConfig starfield{};
};

[[nodiscard]] GameMapConfig make_game_map_config(GameMapSize size);
}
