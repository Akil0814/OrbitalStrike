#include "black_hole_anomaly.h"

#include "../collision_layers.h"
#include "engine/core/render/render_command.h"

#include <algorithm>
#include <cmath>

namespace game::anomaly
{
BlackHoleAnomaly::BlackHoleAnomaly(BlackHoleConfig config)
    : SpaceAnomaly(config.center, config.visual_radius), _config(config)
{
    _config.visual_radius = world_rect().width() * 0.5f;
    _config.event_horizon_radius = std::clamp(
        std::isfinite(_config.event_horizon_radius) ? _config.event_horizon_radius : 70.0f,
        8.0f, _config.visual_radius);
    _collider.shape = elysia::physics::CircleShape{
        .local_center = {_config.visual_radius, _config.visual_radius},
        .radius = _config.event_horizon_radius};
    _collider.filter.category = game::collision_layers::SpaceAnomaly;
    _collider.filter.mask = game::collision_layers::Projectile;
    _collider.response = elysia::physics::CollisionResponse::Overlap;
    _collider.tag = "black_hole";
}

void BlackHoleAnomaly::submit_render_commands(
    std::vector<elysia::core::RenderCommand>& commands) const
{
    const auto draw_center = render_rect().center();
    const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(elapsed_seconds()) * 2.4f);
    commands.push_back(elysia::core::make_world_fill_circle_command(
        draw_center, _config.visual_radius * (0.82f + pulse * 0.08f), {50, 24, 94, 80}));
    commands.push_back(elysia::core::make_world_draw_circle_command(
        draw_center, _config.visual_radius * (0.9f + pulse * 0.1f), _config.color, 4.0f));
    commands.push_back(elysia::core::make_world_fill_circle_command(
        draw_center, _config.event_horizon_radius, {2, 3, 10, 255}));
    commands.push_back(elysia::core::make_world_draw_circle_command(
        draw_center, _config.event_horizon_radius + 5.0f, {214, 128, 255, 230}, 3.0f));
}

elysia::core::Vector2 BlackHoleAnomaly::force_on(
    const game::projectile::ProjectileState& projectile) const noexcept
{
    return game::projectile::compute_radial_force(
        _config.radial_force, center(), projectile);
}

game::projectile::ProjectileImpactResolution BlackHoleAnomaly::resolve_projectile_impact(
    const game::projectile::ProjectileImpact& impact)
{
    (void)impact;
    return {.disposition = game::projectile::ProjectileDisposition::Destroy};
}

elysia::physics::ColliderId BlackHoleAnomaly::collider_id() const noexcept
{
    return physics_collider(0);
}

elysia::physics::BodyDefinition BlackHoleAnomaly::body_definition() const
{
    elysia::physics::BodyDefinition definition;
    definition.type = elysia::physics::BodyType::Static;
    definition.gravity_scale = 0.0f;
    return definition;
}

std::span<const elysia::physics::Collider> BlackHoleAnomaly::collider_definitions() const
{
    return std::span<const elysia::physics::Collider>(&_collider, 1);
}
}
