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

std::uint8_t dim_channel(std::uint8_t channel) noexcept
{
    return static_cast<std::uint8_t>(static_cast<unsigned int>(channel) * 2u / 5u);
}
}

CelestialBody::CelestialBody(CelestialBodyConfig config)
    : GameObject(elysia::core::DepthLayer::Terrain), _config(config)
{
    _config.radius = std::max(1.0f, std::isfinite(_config.radius) ? _config.radius : 64.0f);
    _config.radial_force.strength = std::max(
        0.0f, std::isfinite(_config.radial_force.strength) ? _config.radial_force.strength : 0.0f);
    _config.radial_force.maximum_range = std::max(
        0.0f, std::isfinite(_config.radial_force.maximum_range)
            ? _config.radial_force.maximum_range : 0.0f);
    _config.radial_force.minimum_distance = std::max(
        1.0f, std::isfinite(_config.radial_force.minimum_distance)
            ? _config.radial_force.minimum_distance : 1.0f);
    _config.radial_force.maximum_force = std::max(
        0.0f, std::isfinite(_config.radial_force.maximum_force)
            ? _config.radial_force.maximum_force : 0.0f);
    _config.projectile_restitution = std::max(
        0.0f, std::isfinite(_config.projectile_restitution) ? _config.projectile_restitution : 1.0f);
    _config.knockback_impulse = std::max(
        0.0f, std::isfinite(_config.knockback_impulse) ? _config.knockback_impulse : 0.0f);
    _config.motion.mass = std::max(
        0.1f, std::isfinite(_config.motion.mass) ? _config.motion.mass : 20.0f);
    _config.motion.linear_damping = std::max(
        0.0f, std::isfinite(_config.motion.linear_damping) ? _config.motion.linear_damping : 1.5f);

    _maximum_hit_points = std::max(0, _config.hit_points);
    _hit_points = _maximum_hit_points;
    set_world_rect({_config.center.x - _config.radius, _config.center.y - _config.radius,
                    2.0f * _config.radius, 2.0f * _config.radius});

    _collider.shape = elysia::physics::CircleShape{
        .local_center = {_config.radius, _config.radius}, .radius = _config.radius};
    _collider.filter.category = category_for(_config.faction);
    _collider.filter.mask = bullet_mask_for(_config.faction);
    _collider.response = _config.projectile_disposition == ProjectileDisposition::Continue
        ? elysia::physics::CollisionResponse::Overlap
        : elysia::physics::CollisionResponse::Block;
    _collider.material.restitution = 0.0f;
    _collider.tag = "planet";
}

void CelestialBody::submit_render_commands(std::vector<elysia::core::RenderCommand>& out_commands) const
{
    const auto color = is_defeated()
        ? elysia::core::Color{
            dim_channel(_config.color.r), dim_channel(_config.color.g),
            dim_channel(_config.color.b), _config.color.a}
        : _config.color;
    out_commands.push_back(elysia::core::make_world_fill_circle_command(center(), _config.radius, color));
    out_commands.push_back(elysia::core::make_world_draw_circle_command(
        center(), _config.radius, {220, 232, 255, 220}, 2.0f));
    if (_config.faction != CelestialFaction::Enemy) return;

    constexpr float pip_radius = 7.0f;
    constexpr float pip_spacing = 20.0f;
    const float start_x = center().x
        - pip_spacing * static_cast<float>(std::max(0, _maximum_hit_points - 1)) * 0.5f;
    const float y = center().y - _config.radius - 20.0f;
    for (int index = 0; index < _maximum_hit_points; ++index)
    {
        const bool filled = index < _hit_points;
        out_commands.push_back(filled
            ? elysia::core::make_world_fill_circle_command(
                {start_x + pip_spacing * static_cast<float>(index), y}, pip_radius, {255, 92, 92})
            : elysia::core::make_world_draw_circle_command(
                {start_x + pip_spacing * static_cast<float>(index), y}, pip_radius,
                {255, 180, 180}, 1.5f));
    }
}

elysia::physics::BodyDefinition CelestialBody::body_definition() const
{
    elysia::physics::BodyDefinition definition;
    definition.type = _config.motion.dynamic
        ? elysia::physics::BodyType::Dynamic : elysia::physics::BodyType::Static;
    definition.gravity_scale = 0.0f;
    definition.fixed_rotation = true;
    if (_config.motion.dynamic)
    {
        definition.mass_policy = elysia::physics::MassPolicy::ExplicitMass;
        definition.mass = _config.motion.mass;
        definition.linear_damping = _config.motion.linear_damping;
    }
    return definition;
}

std::span<const elysia::physics::Collider> CelestialBody::collider_definitions() const
{
    return std::span<const elysia::physics::Collider>(&_collider, 1);
}

void CelestialBody::fixed_update(double fixed_delta_seconds)
{
    (void)fixed_delta_seconds;
    if (_config.motion.dynamic && !is_defeated()) constrain_to_movement_bounds();
}

elysia::core::Vector2 CelestialBody::force_on(const ProjectileState& projectile) const noexcept
{
    const auto& force = _config.radial_force;
    if (is_defeated() || force.mode == RadialForceMode::None || force.strength <= 0.0f)
        return {};

    const auto offset = projectile.position - center();
    const float distance_squared = offset.length_squared();
    if (distance_squared <= elysia::core::Vector2::k_epsilon
        || distance_squared > force.maximum_range * force.maximum_range) return {};
    const float minimum_distance_squared = force.minimum_distance * force.minimum_distance;
    const float clamped_distance_squared = std::max(distance_squared, minimum_distance_squared);
    const float magnitude = std::min(
        force.maximum_force,
        force.strength * (minimum_distance_squared / clamped_distance_squared));
    const auto outward = offset.normalized();
    return force.mode == RadialForceMode::Attract ? -outward * magnitude : outward * magnitude;
}

ProjectileCollisionResult CelestialBody::on_projectile_hit(const ProjectileHitContext& hit)
{
    ProjectileCollisionResult result{
        .disposition = _config.projectile_disposition,
        .restitution = _config.projectile_restitution};
    if (is_defeated()) return result;

    const bool defeated_now = _config.receives_damage && apply_damage(hit.damage);
    if (defeated_now)
    {
        disable_defeated_body();
        return result;
    }

    if (_config.hit_reaction == HitReactionMode::Knockback && _config.motion.dynamic
        && physics_world() && !hit.projectile_velocity.is_zero())
    {
        (void)physics_world()->apply_impulse(
            physics_handle(), hit.projectile_velocity.normalized() * _config.knockback_impulse);
    }
    return result;
}

bool CelestialBody::apply_damage(int amount) noexcept
{
    if (amount <= 0 || _hit_points <= 0) return false;
    _hit_points = std::max(0, _hit_points - amount);
    return _hit_points == 0;
}

void CelestialBody::disable_defeated_body() noexcept
{
    if (!physics_world()) return;
    set_velocity({});
    (void)physics_world()->set_body_enabled(physics_handle(), false);
}

void CelestialBody::constrain_to_movement_bounds() noexcept
{
    if (!physics_world()) return;
    const auto& bounds = _config.motion.movement_bounds;
    const float max_x = bounds.right() - world_rect().width();
    const float max_y = bounds.bottom() - world_rect().height();
    if (max_x < bounds.left() || max_y < bounds.top()) return;

    const auto current = position();
    const elysia::core::Vector2 clamped{
        std::clamp(current.x, bounds.left(), max_x),
        std::clamp(current.y, bounds.top(), max_y)};
    if (clamped == current) return;

    auto current_velocity = velocity();
    if ((current.x < bounds.left() && current_velocity.x < 0.0f)
        || (current.x > max_x && current_velocity.x > 0.0f)) current_velocity.x = 0.0f;
    if ((current.y < bounds.top() && current_velocity.y < 0.0f)
        || (current.y > max_y && current_velocity.y > 0.0f)) current_velocity.y = 0.0f;
    (void)physics_world()->teleport_object(
        physics_handle(), clamped, elysia::physics::TeleportVelocityMode::Preserve);
    set_velocity(current_velocity);
}
}
