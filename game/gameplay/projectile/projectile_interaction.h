#pragma once

#include "../combat/damage.h"

#include "engine/core/geometry/vector2.h"
#include "engine/physics/collision/collider.h"

#include <optional>

namespace game::projectile
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
    Reflect,
    Teleport
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

struct ProjectileImpact
{
    elysia::physics::ColliderId target_collider = elysia::physics::InvalidColliderId;
    elysia::core::Vector2 contact_point{};
    elysia::core::Vector2 contact_normal{};
    elysia::core::Vector2 projectile_velocity{};
    game::combat::DamageSpec damage{};
};

struct ProjectileImpactResolution
{
    ProjectileDisposition disposition = ProjectileDisposition::Destroy;
    float restitution = 1.0f;
    std::optional<elysia::core::Vector2> teleport_position;
    game::combat::DamageResult damage{};
};

struct ProjectileMotionResolution
{
    elysia::core::Vector2 velocity{};
    std::optional<elysia::core::Vector2> teleport_position;
    bool should_finish = false;
};

[[nodiscard]] elysia::core::Vector2 compute_radial_force(
    const RadialForceConfig& config,
    elysia::core::Vector2 source_position,
    const ProjectileState& projectile) noexcept;

[[nodiscard]] ProjectileMotionResolution resolve_projectile_motion(
    const ProjectileImpactResolution& resolution,
    elysia::core::Vector2 incoming_velocity,
    elysia::core::Vector2 contact_normal) noexcept;

class ProjectileForceSource
{
public:
    virtual ~ProjectileForceSource() = default;
    [[nodiscard]] virtual elysia::core::Vector2 force_on(
        const ProjectileState& projectile) const noexcept = 0;
};

class ProjectileImpactTarget
{
public:
    virtual ~ProjectileImpactTarget() = default;
    [[nodiscard]] virtual ProjectileImpactResolution resolve_projectile_impact(
        const ProjectileImpact& impact) = 0;
    [[nodiscard]] virtual elysia::physics::ColliderId collider_id() const noexcept = 0;
};
}
