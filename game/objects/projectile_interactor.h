#pragma once

#include "engine/core/geometry/vector2.h"
#include "engine/physics/collision/collider.h"

namespace game::objects
{
enum class RadialForceMode : unsigned char
{
    None,
    Attract,
    Repel
};

enum class ProjectileDisposition : unsigned char
{
    Destroy,
    Continue,
    Reflect
};

enum class HitReactionMode : unsigned char
{
    None,
    Knockback
};

struct RadialForceConfig
{
    RadialForceMode mode = RadialForceMode::None;
    float strength = 0.0f;
    float maximum_range = 520.0f;
    float minimum_distance = 140.0f;
    float maximum_force = 80.0f;
};

struct ProjectileState
{
    elysia::core::Vector2 position{};
    elysia::core::Vector2 velocity{};
};

struct ProjectileHitContext
{
    elysia::physics::ColliderId target_collider = elysia::physics::InvalidColliderId;
    elysia::core::Vector2 contact_point{};
    elysia::core::Vector2 contact_normal{};
    elysia::core::Vector2 projectile_velocity{};
    int damage = 1;
};

struct ProjectileCollisionResult
{
    ProjectileDisposition disposition = ProjectileDisposition::Destroy;
    float restitution = 1.0f;
};

[[nodiscard]] elysia::core::Vector2 compute_radial_force(
    const RadialForceConfig& config,
    elysia::core::Vector2 source_position,
    const ProjectileState& projectile) noexcept;

class ProjectileInteractor
{
public:
    virtual ~ProjectileInteractor() = default;
    [[nodiscard]] virtual elysia::core::Vector2 force_on(
        const ProjectileState& projectile) const noexcept = 0;
    [[nodiscard]] virtual ProjectileCollisionResult on_projectile_hit(
        const ProjectileHitContext& hit) = 0;
    [[nodiscard]] virtual elysia::physics::ColliderId collider_id() const noexcept = 0;
};
}
