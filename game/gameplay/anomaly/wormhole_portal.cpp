#include "wormhole_portal.h"

#include "../collision_layers.h"
#include "engine/core/render/render_command.h"

#include <algorithm>
#include <cmath>

namespace game::anomaly
{
WormholePortal::WormholePortal(WormholePortalConfig config)
    : SpaceAnomaly(config.center, config.portal_radius), _config(config)
{
    _config.portal_radius = world_rect().width() * 0.5f;
    _config.exit_offset = std::max(
        _config.portal_radius + 12.0f,
        std::isfinite(_config.exit_offset) ? _config.exit_offset : 96.0f);
    _collider.shape = elysia::physics::CircleShape{
        .local_center = {_config.portal_radius, _config.portal_radius},
        .radius = _config.portal_radius};
    _collider.filter.category = game::collision_layers::SpaceAnomaly;
    _collider.filter.mask = game::collision_layers::Projectile;
    _collider.response = elysia::physics::CollisionResponse::Overlap;
    _collider.tag = "wormhole";
}

void WormholePortal::submit_render_commands(
    std::vector<elysia::core::RenderCommand>& commands) const
{
    const auto draw_center = render_rect().center();
    const float phase = static_cast<float>(elapsed_seconds());
    auto glow = _config.color;
    glow.a = 45;
    commands.push_back(elysia::core::make_world_fill_circle_command(
        draw_center, _config.portal_radius * 0.92f, glow));
    for (int ring = 0; ring < 3; ++ring)
    {
        auto color = _config.color;
        color.a = static_cast<std::uint8_t>(220 - ring * 45);
        const float wave = 0.5f + 0.5f * std::sin(phase * 2.8f + ring * 1.7f);
        commands.push_back(elysia::core::make_world_draw_circle_command(
            draw_center, _config.portal_radius * (0.45f + 0.16f * ring + 0.1f * wave),
            color, 3.0f));
    }
}

game::projectile::ProjectileImpactResolution WormholePortal::resolve_projectile_impact(
    const game::projectile::ProjectileImpact& impact)
{
    auto direction = impact.projectile_velocity.normalized();
    if (direction.is_zero()) direction = {0.0f, -1.0f};
    return {
        .disposition = game::projectile::ProjectileDisposition::Teleport,
        .teleport_position = _config.destination_center + direction * _config.exit_offset};
}

elysia::physics::ColliderId WormholePortal::collider_id() const noexcept
{
    return physics_collider(0);
}

elysia::physics::BodyDefinition WormholePortal::body_definition() const
{
    elysia::physics::BodyDefinition definition;
    definition.type = elysia::physics::BodyType::Static;
    definition.gravity_scale = 0.0f;
    return definition;
}

std::span<const elysia::physics::Collider> WormholePortal::collider_definitions() const
{
    return std::span<const elysia::physics::Collider>(&_collider, 1);
}
}
