#include "enemy_ship.h"

#include "../collision_layers.h"
#include "engine/core/render/render_command.h"

#include <algorithm>
#include <cmath>

namespace game::fleet
{
namespace
{
std::uint8_t dim_channel(std::uint8_t channel) noexcept
{
    return static_cast<std::uint8_t>(static_cast<unsigned int>(channel) * 2u / 5u);
}
}

EnemyShip::EnemyShip(EnemyShipConfig config)
    : GameObject(elysia::core::DepthLayer::Item), _config(config)
{
    _config.length = std::max(8.0f, std::isfinite(_config.length) ? _config.length : 120.0f);
    _config.width = std::max(8.0f, std::isfinite(_config.width) ? _config.width : 90.0f);
    _config.collision_radius = std::max(
        4.0f, std::isfinite(_config.collision_radius) ? _config.collision_radius : 58.0f);
    _config.motion.mass = std::max(
        0.1f, std::isfinite(_config.motion.mass) ? _config.motion.mass : 25.0f);
    _config.motion.linear_damping = std::max(
        0.0f, std::isfinite(_config.motion.linear_damping) ? _config.motion.linear_damping : 1.5f);
    _config.knockback_impulse = std::max(
        0.0f, std::isfinite(_config.knockback_impulse) ? _config.knockback_impulse : 0.0f);
    _config.projectile_restitution = std::max(
        0.0f, std::isfinite(_config.projectile_restitution) ? _config.projectile_restitution : 1.0f);
    _config.facing = _config.facing.normalized();
    if (_config.facing.is_zero()) _config.facing = {0.0f, 1.0f};

    _maximum_hit_points = std::max(1, _config.hit_points);
    _hit_points = _maximum_hit_points;
    set_world_rect({_config.center.x - _config.collision_radius,
                    _config.center.y - _config.collision_radius,
                    2.0f * _config.collision_radius,
                    2.0f * _config.collision_radius});

    _collider.shape = elysia::physics::CircleShape{
        .local_center = {_config.collision_radius, _config.collision_radius},
        .radius = _config.collision_radius};
    _collider.filter.category = game::collision_layers::EnemyShip;
    _collider.filter.mask = game::collision_layers::Projectile;
    _collider.response = elysia::physics::CollisionResponse::Block;
    _collider.tag = "enemy_ship";
}

void EnemyShip::submit_render_commands(std::vector<elysia::core::RenderCommand>& commands) const
{
    const auto draw_center = render_rect().center();
    const auto color = is_defeated()
        ? elysia::core::Color{dim_channel(_config.color.r), dim_channel(_config.color.g),
                             dim_channel(_config.color.b), _config.color.a}
        : _config.color;
    const auto perpendicular = elysia::core::Vector2{-_config.facing.y, _config.facing.x};
    const auto nose = draw_center + _config.facing * (_config.length * 0.56f);
    const auto tail_center = draw_center - _config.facing * (_config.length * 0.44f);
    const auto left = tail_center + perpendicular * (_config.width * 0.5f);
    const auto right = tail_center - perpendicular * (_config.width * 0.5f);

    commands.push_back(elysia::core::make_world_fill_triangle_command(nose, left, right, color));
    commands.push_back(elysia::core::make_world_draw_line_command(nose, left, {232, 238, 255}, 2.0f));
    commands.push_back(elysia::core::make_world_draw_line_command(left, right, {232, 238, 255}, 2.0f));
    commands.push_back(elysia::core::make_world_draw_line_command(right, nose, {232, 238, 255}, 2.0f));

    const float core_radius = _config.role == ShipRole::Flagship ? 18.0f : 10.0f;
    commands.push_back(elysia::core::make_world_fill_circle_command(
        draw_center, core_radius, is_defeated() ? elysia::core::Color{55, 55, 65} : elysia::core::Color{255, 210, 120}));
    if (_config.ability == ShipAbility::RepulsionField && !is_defeated())
        commands.push_back(elysia::core::make_world_draw_circle_command(
            draw_center, _config.radial_force.maximum_range, {224, 92, 232, 70}, 2.0f));
    if (_config.ability == ShipAbility::ShieldProjector && !is_defeated())
        commands.push_back(elysia::core::make_world_draw_circle_command(
            draw_center, _config.collision_radius + 14.0f, {100, 190, 255, 190}, 3.0f));
    if (_projectile_shielded && !is_defeated())
        commands.push_back(elysia::core::make_world_draw_circle_command(
            draw_center, _config.collision_radius + 22.0f, {105, 205, 255, 210}, 4.0f));

    constexpr float pip_radius = 6.0f;
    constexpr float pip_spacing = 18.0f;
    const float start_x = draw_center.x
        - pip_spacing * static_cast<float>(_maximum_hit_points - 1) * 0.5f;
    const float y = draw_center.y - _config.collision_radius - 24.0f;
    for (int index = 0; index < _maximum_hit_points; ++index)
    {
        const auto pip_center = elysia::core::Vector2{
            start_x + pip_spacing * static_cast<float>(index), y};
        commands.push_back(index < _hit_points
            ? elysia::core::make_world_fill_circle_command(pip_center, pip_radius, {255, 92, 92})
            : elysia::core::make_world_draw_circle_command(pip_center, pip_radius, {255, 180, 180}, 1.5f));
    }
}

elysia::physics::BodyDefinition EnemyShip::body_definition() const
{
    elysia::physics::BodyDefinition definition;
    definition.type = elysia::physics::BodyType::Dynamic;
    definition.gravity_scale = 0.0f;
    definition.fixed_rotation = true;
    definition.mass_policy = elysia::physics::MassPolicy::ExplicitMass;
    definition.mass = _config.motion.mass;
    definition.linear_damping = _config.motion.linear_damping;
    return definition;
}

std::span<const elysia::physics::Collider> EnemyShip::collider_definitions() const
{
    return std::span<const elysia::physics::Collider>(&_collider, 1);
}

void EnemyShip::fixed_update(double fixed_delta_seconds)
{
    (void)fixed_delta_seconds;
    if (!is_defeated()) constrain_to_movement_bounds();
}

elysia::core::Vector2 EnemyShip::force_on(
    const game::projectile::ProjectileState& projectile) const noexcept
{
    if (is_defeated()) return {};
    return game::projectile::compute_radial_force(_config.radial_force, center(), projectile);
}

game::projectile::ProjectileImpactResolution EnemyShip::resolve_projectile_impact(
    const game::projectile::ProjectileImpact& impact)
{
    game::projectile::ProjectileImpactResolution result{
        .disposition = _config.projectile_disposition,
        .restitution = _config.projectile_restitution};
    result.damage = receive_damage(impact.damage);
    if (!result.damage.blocked && !result.damage.defeated
        && physics_world() && !impact.projectile_velocity.is_zero())
        (void)physics_world()->apply_impulse(
            physics_handle(),
            impact.projectile_velocity.normalized() * _config.knockback_impulse);
    return result;
}

game::combat::DamageResult EnemyShip::receive_damage(
    const game::combat::DamageSpec& damage) noexcept
{
    game::combat::DamageResult result{
        .requested = damage.amount,
        .applied = 0,
        .blocked = false,
        .defeated = is_defeated()};
    if (damage.amount <= 0 || result.defeated) return result;

    result.applied = std::min(_hit_points, damage.amount);
    _hit_points -= result.applied;
    result.defeated = is_defeated();
    if (result.defeated) disable_defeated_body();
    return result;
}

void EnemyShip::disable_defeated_body() noexcept
{
    _projectile_shielded = false;
    if (!physics_world()) return;
    set_velocity({});
    (void)physics_world()->set_body_enabled(physics_handle(), false);
}

void EnemyShip::constrain_to_movement_bounds() noexcept
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
