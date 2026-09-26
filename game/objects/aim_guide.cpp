#include "aim_guide.h"

#include "engine/core/render/render_command.h"

namespace game::objects
{
AimGuide::AimGuide() : GameObject(elysia::core::DepthLayer::EffectFront) {}

void AimGuide::set_aim(elysia::core::Vector2 origin, elysia::core::Vector2 direction,
                       float power, bool visible) noexcept
{
    _origin = origin;
    _direction = direction;
    _power = power;
    _visible = visible;
}

void AimGuide::submit_render_commands(std::vector<elysia::core::RenderCommand>& commands) const
{
    if (!_visible || _direction.is_zero()) return;
    const auto end = _origin + _direction * (_power * 0.18f);
    commands.push_back(elysia::core::make_world_draw_line_command(
        _origin, end, {124, 220, 255}, 2.0f));
    commands.push_back(elysia::core::make_world_fill_circle_command(end, 5.0f, {124, 220, 255}));
}
}
