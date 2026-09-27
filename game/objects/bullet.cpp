#include "bullet.h"

#include "collision_layers.h"
#include "engine/core/render/render_command.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <utility>

namespace game::objects
{
Bullet::Bullet(BulletConfig config)
    : GameObject(elysia::core::DepthLayer::Item),
      _config(std::move(config)),
      _pre_collision_velocity(_config.velocity)
{
    if (!_config.texture)
        throw std::invalid_argument("Bullet requires a projectile texture.");
    _config.radius = std::max(1.0f, std::isfinite(_config.radius) ? _config.radius : 8.0f);
    _config.visual_size.x = std::max(
        1.0f, std::isfinite(_config.visual_size.x) ? _config.visual_size.x : 80.0f);
    _config.visual_size.y = std::max(
        1.0f, std::isfinite(_config.visual_size.y) ? _config.visual_size.y : 53.3333f);
    _config.visual_anchor.x = std::clamp(
        std::isfinite(_config.visual_anchor.x) ? _config.visual_anchor.x : 0.65f, 0.0f, 1.0f);
    _config.visual_anchor.y = std::clamp(
        std::isfinite(_config.visual_anchor.y) ? _config.visual_anchor.y : 0.5f, 0.0f, 1.0f);
    _config.lifetime_seconds = std::max(0.1, std::isfinite(_config.lifetime_seconds) ? _config.lifetime_seconds : 8.0);
    _config.damage = std::max(1, _config.damage);
    const auto initial_direction = _config.velocity.normalized();
    if (!initial_direction.is_zero()) _visual_direction = initial_direction;
    set_world_rect({_config.position.x - _config.radius, _config.position.y - _config.radius,
                    2.0f * _config.radius, 2.0f * _config.radius});
    _collider.shape = elysia::physics::CircleShape{
        .local_center = {_config.radius, _config.radius}, .radius = _config.radius};
    _collider.filter.category = collision_layers::Bullet;
    _collider.filter.mask = collision_layers::EnemyShip | collision_layers::MoonCell;
    _collider.detection_mode = elysia::physics::CollisionDetectionMode::Continuous;
    _collider.material.restitution = 0.0f;
    _collider.tag = "bullet";
}

Bullet::~Bullet() { detach_collision_listener(); }

void Bullet::submit_render_commands(std::vector<elysia::core::RenderCommand>& out_commands) const
{
    elysia::core::RenderCommand command;
    command.type = elysia::core::RenderCommandType::Texture;
    command.texture = _config.texture;
    command.command_rect = {
        center().x - _config.visual_size.x * _config.visual_anchor.x,
        center().y - _config.visual_size.y * _config.visual_anchor.y,
        _config.visual_size.x,
        _config.visual_size.y};
    command.rotation_degrees = std::atan2(_visual_direction.y, _visual_direction.x)
        * 180.0 / std::numbers::pi;
    command.rotation_origin = _config.visual_anchor;
    out_commands.push_back(command);
}

void Bullet::update(double delta_seconds)
{
    if (_finished) return;
    _age_seconds += std::max(0.0, delta_seconds);
    if (_age_seconds >= _config.lifetime_seconds) { finish(BulletEndReason::Expired); return; }
    const auto position = center();
    const auto& bounds = _config.despawn_bounds;
    if (position.x < bounds.left() || position.x > bounds.right() || position.y < bounds.top() || position.y > bounds.bottom())
        finish(BulletEndReason::OutOfBounds);
}

void Bullet::fixed_update(double fixed_delta_seconds)
{
    (void)fixed_delta_seconds;
    if (_finished) return;
    _pre_collision_velocity = velocity();
    const auto direction = _pre_collision_velocity.normalized();
    if (!direction.is_zero()) _visual_direction = direction;
}

void Bullet::on_collision_event(const elysia::physics::CollisionEvent& event)
{
    if (_finished || event.phase != elysia::physics::CollisionEventPhase::Begin) return;
    const auto self = physics_collider(0);
    const auto& pair = event.contact.pair;
    const auto other = pair.first.kind == elysia::physics::CollisionTargetKind::Collider && pair.first.collider == self
        ? pair.second
        : pair.second.kind == elysia::physics::CollisionTargetKind::Collider && pair.second.collider == self
            ? pair.first : elysia::physics::CollisionTarget{};
    if (other.kind != elysia::physics::CollisionTargetKind::Collider) return;

    const auto& manifold = event.contact.manifold;
    const auto contact_point = manifold.contact_point_count > 0
        ? manifold.contact_points[0] : center();
    const ProjectileHitContext hit{
        .target_collider = other.collider,
        .contact_point = contact_point,
        .contact_normal = manifold.normal,
        .projectile_velocity = _pre_collision_velocity,
        .damage = _config.damage};
    const auto result = _config.on_hit
        ? _config.on_hit(hit)
        : ProjectileCollisionResult{};

    switch (result.disposition)
    {
    case ProjectileDisposition::Continue:
        set_velocity(_pre_collision_velocity);
        if (!_pre_collision_velocity.is_zero())
            _visual_direction = _pre_collision_velocity.normalized();
        return;
    case ProjectileDisposition::Reflect:
    {
        auto normal = manifold.normal.normalized();
        if (normal.is_zero()) normal = -_pre_collision_velocity.normalized();
        const auto reflected = _pre_collision_velocity
            - normal * (2.0f * _pre_collision_velocity.dot(normal));
        const auto reflected_velocity = reflected * std::max(0.0f, result.restitution);
        set_velocity(reflected_velocity);
        if (!reflected_velocity.is_zero())
            _visual_direction = reflected_velocity.normalized();
        return;
    }
    case ProjectileDisposition::Destroy:
    default:
        finish(BulletEndReason::Hit);
        return;
    }
}

elysia::physics::BodyDefinition Bullet::body_definition() const
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

std::span<const elysia::physics::Collider> Bullet::collider_definitions() const
{
    return std::span<const elysia::physics::Collider>(&_collider, 1);
}

void Bullet::attach_collision_listener()
{
    if (!_listening && physics_world()) _listening = physics_world()->add_listener(*this);
}

void Bullet::detach_collision_listener() noexcept
{
    if (_listening && physics_world()) (void)physics_world()->remove_listener(*this);
    _listening = false;
}

void Bullet::finish(BulletEndReason reason)
{
    if (_finished) return;
    _finished = true;
    if (_config.on_finished) _config.on_finished(reason);
    destroy();
}
} // namespace game::objects
