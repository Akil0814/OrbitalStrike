#pragma once

#include "engine/physics/collision/collider.h"

namespace game::objects::collision_layers
{
inline constexpr elysia::physics::CollisionBits EnemyShip = 1u << 0;
inline constexpr elysia::physics::CollisionBits Bullet = 1u << 1;
}
