#include "bullet.h"

#include "engine/core/render/render_command.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace game::objects
{
namespace
{
constexpr elysia::physics::CollisionBits kEnemyPlanet = 1u << 1;
constexpr elysia::physics::CollisionBits kNeutralPlanet = 1u << 2;
constexpr elysia::physics::CollisionBits kBullet = 1u << 3;
}

Bullet::Bullet(BulletConfig config) : GameObject(elysia::core::DepthLayer::Item), _config(std::move(config))
{
    _config.radius = std::max(1.0f, std::isfinite(_config.radius) ? _config.radius : 8.0f);
    _config.lifetime_seconds = std::max(0.1, std::isfinite(_config.lifetime_seconds) ? _config.lifetime_seconds : 8.0);
    _config.damage = std::max(1, _config.damage);
    set_world_rect({_config.position.x - _config.radius, _config.position.y - _config.radius,
                    2.0f * _config.radius, 2.0f * _config.radius});
    _collider.shape = elysia::physics::CircleShape{
        .local_center = {_config.radius, _config.radius}, .radius = _config.radius};
    _collider.filter.category = kBullet;
    _collider.filter.mask = kEnemyPlanet | kNeutralPlanet;
    _collider.detection_mode = elysia::physics::CollisionDetectionMode::Continuous;
    _collider.material.restitution = 0.0f;
    _collider.tag = "bullet";
}

Bullet::~Bullet() { detach_collision_listener(); }

void Bullet::submit_render_commands(std::vector<elysia::core::RenderCommand>& out_commands) const
{
    out_commands.push_back(elysia::core::make_world_fill_circle_command(center(), _config.radius, {255, 235, 135}));
    out_commands.push_back(elysia::core::make_world_draw_circle_command(center(), _config.radius, {255, 255, 255}, 1.5f));
}

void Bullet::update(double delta_seconds)
{
    if (_finished) return;
    _age_seconds += std::max(0.0, delta_seconds);
    if (_age_seconds >= _config.lifetime_seconds) { finish(BulletEndReason::Expired); return; }
    const auto position = center();
    const auto& bounds = _config.flight_bounds;
    if (position.x < bounds.left() || position.x > bounds.right() || position.y < bounds.top() || position.y > bounds.bottom())
        finish(BulletEndReason::OutOfBounds);
}

void Bullet::fixed_update(double fixed_delta_seconds) { (void)fixed_delta_seconds; }

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
    if (_config.on_hit) _config.on_hit(other.collider, _config.damage);
    finish(BulletEndReason::Hit);
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
