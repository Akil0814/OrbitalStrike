#include "space_anomaly.h"

#include "engine/core/render/render_command.h"

#include <algorithm>
#include <cmath>

namespace game::anomaly
{
SpaceAnomaly::SpaceAnomaly(elysia::core::Vector2 center, float visual_radius)
    : GameObject(elysia::core::DepthLayer::Terrain)
{
    const float radius = std::max(
        8.0f, std::isfinite(visual_radius) ? visual_radius : 110.0f);
    set_world_rect({center.x - radius, center.y - radius, 2.0f * radius, 2.0f * radius});
}

void SpaceAnomaly::update(double delta_seconds)
{
    if (!std::isfinite(delta_seconds) || delta_seconds <= 0.0) return;
    _elapsed_seconds = std::fmod(_elapsed_seconds + delta_seconds, 3600.0);
}

RadialFieldAnomaly::RadialFieldAnomaly(RadialFieldAnomalyConfig config)
    : SpaceAnomaly(config.center, config.visual_radius), _config(config)
{
    _config.visual_radius = world_rect().width() * 0.5f;
}

void RadialFieldAnomaly::submit_render_commands(
    std::vector<elysia::core::RenderCommand>& commands) const
{
    const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(elapsed_seconds()) * 1.7f);
    const auto draw_center = render_rect().center();
    auto fill = _config.color;
    fill.a = 35;
    commands.push_back(elysia::core::make_world_fill_circle_command(
        draw_center, _config.visual_radius * 0.42f, fill));
    for (int ring = 0; ring < 3; ++ring)
    {
        auto color = _config.color;
        color.a = static_cast<std::uint8_t>(150 - ring * 35);
        const float phase = std::fmod(pulse + static_cast<float>(ring) / 3.0f, 1.0f);
        commands.push_back(elysia::core::make_world_draw_circle_command(
            draw_center, _config.visual_radius * (0.45f + phase * 0.55f), color, 2.0f));
    }
}

elysia::core::Vector2 RadialFieldAnomaly::force_on(
    const game::projectile::ProjectileState& projectile) const noexcept
{
    return game::projectile::compute_radial_force(
        _config.radial_force, center(), projectile);
}
}
