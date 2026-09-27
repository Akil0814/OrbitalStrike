#include "projectile.h"

#include "../collision_layers.h"
#include "engine/core/render/render_command.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <utility>

namespace game::projectile
{
Projectile::Projectile(ProjectileConfig config)
    : GameObject(elysia::core::DepthLayer::Item),
      _config(std::move(config)),
      _pre_collision_velocity(_config.velocity)
{
    if (!_config.texture)
        throw std::invalid_argument("Projectile requires a texture.");
    auto& definition = _config.definition;
    definition.radius = std::max(
        1.0f, std::isfinite(definition.radius) ? definition.radius : 8.0f);
    definition.visual_size.x = std::max(
        1.0f, std::isfinite(definition.visual_size.x) ? definition.visual_size.x : 80.0f);
    definition.visual_size.y = std::max(
        1.0f, std::isfinite(definition.visual_size.y) ? definition.visual_size.y : 53.3333f);
    definition.visual_anchor.x = std::clamp(
        std::isfinite(definition.visual_anchor.x) ? definition.visual_anchor.x : 0.65f,
        0.0f, 1.0f);
    definition.visual_anchor.y = std::clamp(
        std::isfinite(definition.visual_anchor.y) ? definition.visual_anchor.y : 0.5f,
        0.0f, 1.0f);
    definition.lifetime_seconds = std::max(
        0.1, std::isfinite(definition.lifetime_seconds) ? definition.lifetime_seconds : 8.0);
    const auto initial_direction = _config.velocity.normalized();
    if (!initial_direction.is_zero()) _visual_direction = initial_direction;
    set_world_rect({_config.position.x - definition.radius,
                    _config.position.y - definition.radius,
                    2.0f * definition.radius,
                    2.0f * definition.radius});
    _collider.shape = elysia::physics::CircleShape{
        .local_center = {definition.radius, definition.radius}, .radius = definition.radius};
    _collider.filter.category = game::collision_layers::Projectile;
    _collider.filter.mask = game::collision_layers::EnemyShip
        | game::collision_layers::MoonCell;
    _collider.detection_mode = elysia::physics::CollisionDetectionMode::Continuous;
    _collider.material.restitution = 0.0f;
    _collider.tag = "projectile";
}

Projectile::~Projectile() { detach_collision_listener(); }

void Projectile::submit_render_commands(
    std::vector<elysia::core::RenderCommand>& out_commands) const
{
    elysia::core::RenderCommand command;
    command.type = elysia::core::RenderCommandType::Texture;
    command.texture = _config.texture;
    command.command_rect = {
        center().x - _config.definition.visual_size.x * _config.definition.visual_anchor.x,
        center().y - _config.definition.visual_size.y * _config.definition.visual_anchor.y,
        _config.definition.visual_size.x,
        _config.definition.visual_size.y};
    command.rotation_degrees = std::atan2(_visual_direction.y, _visual_direction.x)
        * 180.0 / std::numbers::pi;
    command.rotation_origin = _config.definition.visual_anchor;
    out_commands.push_back(command);
}

void Projectile::update(double delta_seconds)
{
    if (_finished) return;
    _age_seconds += std::max(0.0, delta_seconds);
    if (_age_seconds >= _config.definition.lifetime_seconds)
    {
        finish(ProjectileEndReason::Expired);
        return;
    }
    const auto position = center();
    const auto& bounds = _config.despawn_bounds;
    if (position.x < bounds.left() || position.x > bounds.right()
        || position.y < bounds.top() || position.y > bounds.bottom())
        finish(ProjectileEndReason::OutOfBounds);
}

void Projectile::fixed_update(double fixed_delta_seconds)
{
    (void)fixed_delta_seconds;
    if (_finished) return;
    _pre_collision_velocity = velocity();
    const auto direction = _pre_collision_velocity.normalized();
    if (!direction.is_zero()) _visual_direction = direction;
}

void Projectile::on_collision_event(const elysia::physics::CollisionEvent& event)
{
    if (_finished || event.phase != elysia::physics::CollisionEventPhase::Begin) return;
    const auto self = physics_collider(0);
    const auto& pair = event.contact.pair;
    const auto other = pair.first.kind == elysia::physics::CollisionTargetKind::Collider
            && pair.first.collider == self
        ? pair.second
        : pair.second.kind == elysia::physics::CollisionTargetKind::Collider
                && pair.second.collider == self
            ? pair.first
            : elysia::physics::CollisionTarget{};
    if (other.kind != elysia::physics::CollisionTargetKind::Collider) return;

    const auto& manifold = event.contact.manifold;
    const auto contact_point = manifold.contact_point_count > 0
        ? manifold.contact_points[0] : center();
    const ProjectileImpact impact{
        .target_collider = other.collider,
        .contact_point = contact_point,
        .contact_normal = manifold.normal,
        .projectile_velocity = _pre_collision_velocity,
        .damage = _config.definition.damage};
    const auto resolution = _config.on_impact
        ? _config.on_impact(impact)
        : ProjectileImpactResolution{};
    const auto motion = resolve_projectile_motion(
        resolution, _pre_collision_velocity, manifold.normal);
    if (motion.should_finish)
    {
        finish(ProjectileEndReason::Hit);
        return;
    }
    set_velocity(motion.velocity);
    if (!motion.velocity.is_zero()) _visual_direction = motion.velocity.normalized();
}

elysia::physics::BodyDefinition Projectile::body_definition() const
{
    elysia::physics::BodyDefinition definition;
    definition.type = elysia::physics::BodyType::Dynamic;
    definition.velocity = _config.velocity;
    definition.gravity_scale = 0.0f;
    definition.fixed_rotation = true;
    definition.enable_sleep = false;
    definition.bullet = true;
    return definition;
}

std::span<const elysia::physics::Collider> Projectile::collider_definitions() const
{
    return std::span<const elysia::physics::Collider>(&_collider, 1);
}

void Projectile::attach_collision_listener()
{
    if (!_listening && physics_world()) _listening = physics_world()->add_listener(*this);
}

void Projectile::detach_collision_listener() noexcept
{
    if (_listening && physics_world()) (void)physics_world()->remove_listener(*this);
    _listening = false;
}

void Projectile::finish(ProjectileEndReason reason)
{
    if (_finished) return;
    _finished = true;
    if (_config.on_finished) _config.on_finished(reason);
    destroy();
}
}
