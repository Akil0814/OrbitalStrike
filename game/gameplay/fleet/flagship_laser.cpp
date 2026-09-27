#include "flagship_laser.h"

#include "engine/core/render/render_command.h"

#include <algorithm>
#include <cmath>

namespace game::fleet
{
FlagshipLaser::FlagshipLaser(
    elysia::core::Vector2 origin, elysia::core::Vector2 target)
    : GameObject(elysia::core::DepthLayer::EffectFront), _origin(origin), _target(target)
{
    const float left = std::min(origin.x, target.x);
    const float top = std::min(origin.y, target.y);
    set_world_rect({left - 180.0f, top - 180.0f,
                    std::fabs(target.x - origin.x) + 360.0f,
                    std::fabs(target.y - origin.y) + 360.0f});
}

void FlagshipLaser::update(double delta_seconds)
{
    if (std::isfinite(delta_seconds) && delta_seconds > 0.0)
        _elapsed_seconds = std::fmod(_elapsed_seconds + delta_seconds, 3600.0);
}

void FlagshipLaser::submit_render_commands(
    std::vector<elysia::core::RenderCommand>& commands) const
{
    const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(_elapsed_seconds) * 14.0f);
    if (_phase == FlagshipLaserPhase::Warning)
    {
        commands.push_back(elysia::core::make_world_fill_circle_command(
            _origin, 32.0f + pulse * 20.0f, {255, 52, 72, 90}));
        commands.push_back(elysia::core::make_world_draw_circle_command(
            _origin, 70.0f + pulse * 60.0f, {255, 88, 108, 230}, 5.0f));
        commands.push_back(elysia::core::make_world_draw_line_command(
            _origin, _target, {255, 70, 82, static_cast<std::uint8_t>(45 + pulse * 45.0f)}, 3.0f));
        return;
    }

    commands.push_back(elysia::core::make_world_draw_line_command(
        _origin, _target, {255, 30, 54, 80}, 92.0f + pulse * 18.0f));
    commands.push_back(elysia::core::make_world_draw_line_command(
        _origin, _target, {255, 104, 126, 220}, 46.0f + pulse * 8.0f));
    commands.push_back(elysia::core::make_world_draw_line_command(
        _origin, _target, {255, 245, 248, 255}, 14.0f));
    commands.push_back(elysia::core::make_world_fill_circle_command(
        _target, 70.0f + pulse * 40.0f, {255, 62, 80, 180}));
}

void FlagshipLaser::set_phase(FlagshipLaserPhase phase) noexcept
{
    _phase = phase;
    _elapsed_seconds = 0.0;
}
}
