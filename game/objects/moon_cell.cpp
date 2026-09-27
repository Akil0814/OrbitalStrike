#include "moon_cell.h"

#include "engine/core/render/render_command.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace game::objects
{
MoonCell::MoonCell(MoonCellConfig config)
    : GameObject(elysia::core::DepthLayer::Terrain, 5), _config(config)
{
    _config.moon_radius = std::max(
        1.0f, std::isfinite(_config.moon_radius) ? _config.moon_radius : 1050.0f);
    _config.barrel_length = std::max(
        1.0f, std::isfinite(_config.barrel_length) ? _config.barrel_length : 140.0f);
    _config.barrel_thickness = std::max(
        1.0f, std::isfinite(_config.barrel_thickness) ? _config.barrel_thickness : 20.0f);
    set_world_rect({
        _config.moon_center.x - _config.moon_radius,
        _config.moon_center.y - _config.moon_radius,
        2.0f * _config.moon_radius,
        2.0f * _config.moon_radius});
}

void MoonCell::set_aim_direction(elysia::core::Vector2 direction) noexcept
{
    direction = direction.normalized();
    if (direction.is_zero()) return;

    if (direction.x > elysia::core::Vector2::k_epsilon) _last_horizontal_sign = 1.0f;
    else if (direction.x < -elysia::core::Vector2::k_epsilon) _last_horizontal_sign = -1.0f;

    if (direction.y > 0.0f)
    {
        const float horizontal = std::fabs(direction.x) > elysia::core::Vector2::k_epsilon
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
    commands.push_back(elysia::core::make_world_fill_circle_command(
        _config.moon_center, _config.moon_radius, _config.moon_color));
    commands.push_back(elysia::core::make_world_draw_circle_command(
        _config.moon_center, _config.moon_radius, _config.moon_outline_color, 4.0f));

    const elysia::core::Rect base{
        _config.cannon_pivot.x - _config.cannon_base_size.x * 0.5f,
        _config.cannon_pivot.y,
        _config.cannon_base_size.x,
        _config.cannon_base_size.y};
    commands.push_back(elysia::core::make_world_fill_rect_command(base, _config.cannon_color));

    auto barrel = elysia::core::make_world_fill_rect_command(
        {_config.cannon_pivot.x,
         _config.cannon_pivot.y - _config.barrel_thickness * 0.5f,
         _config.barrel_length,
         _config.barrel_thickness},
        _config.cannon_color);
    barrel.rotation_degrees = std::atan2(_aim_direction.y, _aim_direction.x)
        * 180.0 / std::numbers::pi;
    barrel.rotation_origin = {0.0f, 0.5f};
    commands.push_back(barrel);
    commands.push_back(elysia::core::make_world_fill_circle_command(
        _config.cannon_pivot, _config.barrel_thickness * 0.8f, {92, 116, 150}));
}

elysia::core::Vector2 MoonCell::force_on(const ProjectileState& projectile) const noexcept
{
    return compute_radial_force(_config.radial_force, _config.moon_center, projectile);
}

ProjectileCollisionResult MoonCell::on_projectile_hit(const ProjectileHitContext& hit)
{
    (void)hit;
    return {.disposition = ProjectileDisposition::Continue};
}
}
