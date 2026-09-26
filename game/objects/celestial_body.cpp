#include "celestial_body.h"

#include "engine/core/render/render_command.h"

#include <algorithm>
#include <cmath>

namespace game::objects
{
namespace
{
constexpr elysia::physics::CollisionBits kPlayerPlanet = 1u << 0;
constexpr elysia::physics::CollisionBits kEnemyPlanet = 1u << 1;
constexpr elysia::physics::CollisionBits kNeutralPlanet = 1u << 2;
constexpr elysia::physics::CollisionBits kBullet = 1u << 3;

elysia::physics::CollisionBits category_for(CelestialFaction faction)
{
    switch (faction)
    {
    case CelestialFaction::Player: return kPlayerPlanet;
    case CelestialFaction::Enemy: return kEnemyPlanet;
    case CelestialFaction::Neutral: return kNeutralPlanet;
    }
    return kNeutralPlanet;
}

elysia::physics::CollisionBits bullet_mask_for(CelestialFaction faction)
{
    return faction == CelestialFaction::Player ? 0u : kBullet;
}
} // namespace

CelestialBody::CelestialBody(CelestialBodyConfig config)
    : GameObject(elysia::core::DepthLayer::Terrain), _config(config)
{
    _config.radius = std::max(1.0f, std::isfinite(_config.radius) ? _config.radius : 64.0f);
    _config.gravity_strength = std::max(0.0f, std::isfinite(_config.gravity_strength)
        ? _config.gravity_strength : 0.0f);
    _hit_points = std::max(0, _config.hit_points);
    set_world_rect({_config.center.x - _config.radius, _config.center.y - _config.radius,
                    2.0f * _config.radius, 2.0f * _config.radius});

    _collider.shape = elysia::physics::CircleShape{
        .local_center = {_config.radius, _config.radius}, .radius = _config.radius};
    _collider.filter.category = category_for(_config.faction);
    _collider.filter.mask = bullet_mask_for(_config.faction);
    _collider.material.restitution = 0.0f;
    _collider.tag = "planet";
}

void CelestialBody::submit_render_commands(std::vector<elysia::core::RenderCommand>& out_commands) const
{
    out_commands.push_back(elysia::core::make_world_fill_circle_command(center(), _config.radius, _config.color));
    out_commands.push_back(elysia::core::make_world_draw_circle_command(
        center(), _config.radius, {220, 232, 255, 220}, 2.0f));
    if (_config.faction != CelestialFaction::Enemy) return;

    constexpr float pip_radius = 7.0f;
    constexpr float pip_spacing = 20.0f;
    const float start_x = center().x - pip_spacing;
    const float y = center().y - _config.radius - 20.0f;
    for (int index = 0; index < 3; ++index)
    {
        const bool filled = index < _hit_points;
        out_commands.push_back(filled
            ? elysia::core::make_world_fill_circle_command(
                {start_x + pip_spacing * static_cast<float>(index), y}, pip_radius, {255, 92, 92})
            : elysia::core::make_world_draw_circle_command(
                {start_x + pip_spacing * static_cast<float>(index), y}, pip_radius, {255, 180, 180}, 1.5f));
    }
}

elysia::physics::BodyDefinition CelestialBody::body_definition() const
{
    elysia::physics::BodyDefinition definition;
    definition.type = elysia::physics::BodyType::Static;
    return definition;
}

std::span<const elysia::physics::Collider> CelestialBody::collider_definitions() const
{
    return std::span<const elysia::physics::Collider>(&_collider, 1);
}

bool CelestialBody::apply_damage(int amount) noexcept
{
    if (amount <= 0 || _hit_points <= 0) return false;
    _hit_points = std::max(0, _hit_points - amount);
    return _hit_points == 0;
}
} // namespace game::objects
