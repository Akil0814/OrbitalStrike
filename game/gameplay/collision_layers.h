#pragma once

#include "engine/physics/collision/collider.h"

namespace game::collision_layers
{
inline constexpr elysia::physics::CollisionBits EnemyShip = 1u << 0;
inline constexpr elysia::physics::CollisionBits Projectile = 1u << 1;
inline constexpr elysia::physics::CollisionBits MoonCell = 1u << 2;
}
