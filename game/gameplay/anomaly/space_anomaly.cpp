#include "space_anomaly.h"

#include "engine/core/render/render_command.h"

#include <algorithm>
#include <cmath>

namespace game::anomaly
{
SpaceAnomaly::SpaceAnomaly(SpaceAnomalyConfig config)
    : GameObject(elysia::core::DepthLayer::Terrain), _config(config)
{
    _config.visual_radius = std::max(
        8.0f, std::isfinite(_config.visual_radius) ? _config.visual_radius : 110.0f);
    set_world_rect({_config.center.x - _config.visual_radius,
                    _config.center.y - _config.visual_radius,
                    2.0f * _config.visual_radius,
                    2.0f * _config.visual_radius});
}

void SpaceAnomaly::update(double delta_seconds)
{
    if (!std::isfinite(delta_seconds) || delta_seconds <= 0.0) return;
    _elapsed_seconds = std::fmod(_elapsed_seconds + delta_seconds, 3600.0);
}

void SpaceAnomaly::submit_render_commands(std::vector<elysia::core::RenderCommand>& commands) const
{
    const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(_elapsed_seconds) * 1.7f);
    const auto center = world_rect().center();
    auto fill = _config.color;
    fill.a = 35;
    commands.push_back(elysia::core::make_world_fill_circle_command(
        center, _config.visual_radius * 0.42f, fill));
    for (int ring = 0; ring < 3; ++ring)
    {
        auto color = _config.color;
        color.a = static_cast<std::uint8_t>(150 - ring * 35);
        const float phase = std::fmod(pulse + static_cast<float>(ring) / 3.0f, 1.0f);
        commands.push_back(elysia::core::make_world_draw_circle_command(
            center, _config.visual_radius * (0.45f + phase * 0.55f), color, 2.0f));
    }
}

elysia::core::Vector2 SpaceAnomaly::force_on(
    const game::projectile::ProjectileState& projectile) const noexcept
{
    return game::projectile::compute_radial_force(
        _config.radial_force, world_rect().center(), projectile);
}
}
