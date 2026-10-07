#include "moon_cell.h"

#include "../collision_layers.h"
#include "engine/core/render/render_command.h"
#include "engine/resources/resource_service.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <string>

namespace game::launcher
{
MoonCell::MoonCell(MoonCellConfig config)
    : GameObject(elysia::core::DepthLayer::Terrain, 5), _config(config)
{
    _base_texture = ELYSIA_RESOURCES->find_texture(_config.base_texture_key);
    if (!_base_texture)
        throw std::runtime_error(
            "Required texture is unavailable: " + std::string(_config.base_texture_key));
    _cannon_texture = ELYSIA_RESOURCES->find_texture(_config.cannon_texture_key);
    if (!_cannon_texture)
        throw std::runtime_error(
            "Required texture is unavailable: " + std::string(_config.cannon_texture_key));

    _config.moon_radius = std::max(
        1.0f, std::isfinite(_config.moon_radius) ? _config.moon_radius : 1050.0f);
    _config.barrel_length = std::max(
        1.0f, std::isfinite(_config.barrel_length) ? _config.barrel_length : 140.0f);
    const auto sanitize_size = [](elysia::core::Vector2 value, elysia::core::Vector2 fallback) {
        value.x = std::max(1.0f, std::isfinite(value.x) ? value.x : fallback.x);
        value.y = std::max(1.0f, std::isfinite(value.y) ? value.y : fallback.y);
        return value;
    };
    const auto sanitize_anchor = [](elysia::core::Vector2 value, elysia::core::Vector2 fallback) {
        value.x = std::clamp(std::isfinite(value.x) ? value.x : fallback.x, 0.0f, 1.0f);
        value.y = std::clamp(std::isfinite(value.y) ? value.y : fallback.y, 0.0f, 1.0f);
        return value;
    };
    _config.base_visual_size = sanitize_size(_config.base_visual_size, {2100.0f, 1182.0f});
    _config.cannon_visual_size = sanitize_size(_config.cannon_visual_size, {406.0f, 399.0f});
    _config.base_visual_anchor = sanitize_anchor(_config.base_visual_anchor, {0.5f, 0.575f});
    _config.cannon_visual_anchor = sanitize_anchor(_config.cannon_visual_anchor, {0.5f, 0.703f});
    set_world_rect({
        _config.moon_center.x - _config.moon_radius,
        _config.moon_center.y - _config.moon_radius,
        2.0f * _config.moon_radius,
        2.0f * _config.moon_radius});

    _collider.shape = elysia::physics::CircleShape{
        .local_center = {_config.moon_radius, _config.moon_radius},
        .radius = _config.moon_radius};
    _collider.filter.category = game::collision_layers::MoonCell;
    _collider.filter.mask = game::collision_layers::Projectile;
    _collider.response = elysia::physics::CollisionResponse::Block;
    _collider.tag = "moon_cell";
}

void MoonCell::set_aim_direction(elysia::core::Vector2 direction) noexcept
{
    direction = direction.normalized();
    if (direction.is_zero()) return;

    if (direction.x > elysia::core::Vector2::kEpsilon) _last_horizontal_sign = 1.0f;
    else if (direction.x < -elysia::core::Vector2::kEpsilon) _last_horizontal_sign = -1.0f;

    if (direction.y > 0.0f)
    {
        const float horizontal = std::fabs(direction.x) > elysia::core::Vector2::kEpsilon
            ? (direction.x > 0.0f ? 1.0f : -1.0f)
            : _last_horizontal_sign;
        direction = {horizontal, 0.0f};
    }
    _aim_direction = direction.normalized();
}

elysia::core::Vector2 MoonCell::muzzle_position() const noexcept
{
    return _config.cannon_pivot + _aim_direction * _config.barrel_length;
}

void MoonCell::submit_render_commands(std::vector<elysia::core::RenderCommand>& commands) const
{
    const auto texture_rect = [this](elysia::core::Vector2 size, elysia::core::Vector2 anchor) {
        return elysia::core::Rect{
            _config.cannon_pivot.x - size.x * anchor.x,
            _config.cannon_pivot.y - size.y * anchor.y,
            size.x,
            size.y};
    };

    elysia::core::RenderCommand base;
    base.type = elysia::core::RenderCommandType::Texture;
    base.texture = _base_texture;
    base.command_rect = texture_rect(_config.base_visual_size, _config.base_visual_anchor);
    base.rotation_origin = _config.base_visual_anchor;
    commands.push_back(base);

    elysia::core::RenderCommand cannon;
    cannon.type = elysia::core::RenderCommandType::Texture;
    cannon.texture = _cannon_texture;
    cannon.command_rect = texture_rect(_config.cannon_visual_size, _config.cannon_visual_anchor);
    cannon.rotation_degrees = std::atan2(_aim_direction.y, _aim_direction.x)
        * 180.0 / std::numbers::pi + 90.0;
    cannon.rotation_origin = _config.cannon_visual_anchor;
    commands.push_back(cannon);
}

elysia::physics::BodyDefinition MoonCell::body_definition() const
{
    elysia::physics::BodyDefinition definition;
    definition.type = elysia::physics::BodyType::Static;
    definition.gravity_scale = 0.0f;
    definition.fixed_rotation = true;
    return definition;
}

std::span<const elysia::physics::Collider> MoonCell::collider_definitions() const
{
    return std::span<const elysia::physics::Collider>(&_collider, 1);
}

elysia::core::Vector2 MoonCell::force_on(
    const game::projectile::ProjectileState& projectile) const noexcept
{
    return game::projectile::compute_radial_force(
        _config.radial_force, _config.moon_center, projectile);
}

game::projectile::ProjectileImpactResolution MoonCell::resolve_projectile_impact(
    const game::projectile::ProjectileImpact& impact)
{
    (void)impact;
    return {.disposition = game::projectile::ProjectileDisposition::Destroy};
}
}
