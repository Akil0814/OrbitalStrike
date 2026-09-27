#include "projectile_interaction.h"

#include <algorithm>

namespace game::projectile
{
elysia::core::Vector2 compute_radial_force(
    const RadialForceConfig& config,
    elysia::core::Vector2 source_position,
    const ProjectileState& projectile) noexcept
{
    if (config.mode == RadialForceMode::None || config.strength <= 0.0f
        || config.maximum_range <= 0.0f || config.maximum_force <= 0.0f)
        return {};

    const auto offset = projectile.position - source_position;
    const float distance_squared = offset.length_squared();
    if (distance_squared <= elysia::core::Vector2::k_epsilon
        || distance_squared > config.maximum_range * config.maximum_range)
        return {};

    const float minimum_distance = std::max(1.0f, config.minimum_distance);
    const float minimum_distance_squared = minimum_distance * minimum_distance;
    const float magnitude = std::min(
        config.maximum_force,
        config.strength * (minimum_distance_squared
            / std::max(distance_squared, minimum_distance_squared)));
    const auto outward = offset.normalized();
    return config.mode == RadialForceMode::Attract
        ? -outward * magnitude
        : outward * magnitude;
}

ProjectileMotionResolution resolve_projectile_motion(
    const ProjectileImpactResolution& resolution,
    elysia::core::Vector2 incoming_velocity,
    elysia::core::Vector2 contact_normal) noexcept
{
    switch (resolution.disposition)
    {
    case ProjectileDisposition::Continue:
        return {.velocity = incoming_velocity, .should_finish = false};
    case ProjectileDisposition::Reflect:
    {
        auto normal = contact_normal.normalized();
        if (normal.is_zero()) normal = -incoming_velocity.normalized();
        const auto reflected = incoming_velocity
            - normal * (2.0f * incoming_velocity.dot(normal));
        return {
            .velocity = reflected * std::max(0.0f, resolution.restitution),
            .should_finish = false};
    }
    case ProjectileDisposition::Teleport:
        return {
            .velocity = incoming_velocity,
            .teleport_position = resolution.teleport_position,
            .should_finish = !resolution.teleport_position.has_value()};
    case ProjectileDisposition::Destroy:
    default:
        return {.velocity = {}, .should_finish = true};
    }
}
}
