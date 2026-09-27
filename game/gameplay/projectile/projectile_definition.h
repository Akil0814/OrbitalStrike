#pragma once

#include "../combat/damage.h"
#include "../../resources/game_texture_keys.h"

#include "engine/core/geometry/vector2.h"

#include <string_view>

namespace game::projectile
{
struct ProjectileDefinition
{
    std::string_view texture_key = game::resources::texture_keys::Projectile;
    elysia::core::Vector2 visual_size{160.0f, 106.0f};
    elysia::core::Vector2 visual_anchor{0.65f, 0.5f};
    float radius = 8.0f;
    double lifetime_seconds = 8.0;
    game::combat::DamageSpec damage{};
};
}
