#include "arena_backdrop.h"

#include "engine/core/render/render_command.h"
#include "engine/tools/random_generator.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace game::objects
{
ArenaBackdrop::ArenaBackdrop(elysia::core::Rect bounds)
    : GameObject(elysia::core::DepthLayer::Background)
{
    set_world_rect(bounds);

    elysia::tools::RandomGenerator random;
    const auto area = std::max(0.0f, bounds.width()) * std::max(0.0f, bounds.height());
    const auto star_count = std::clamp(static_cast<int>(area / 12000.0f), 48, 180);
    _stars.reserve(static_cast<std::size_t>(star_count));

    for (int index = 0; index < star_count; ++index)
    {
        Star star;
        star.position = {
            static_cast<float>(random.real(bounds.left(), bounds.right())),
            static_cast<float>(random.real(bounds.top(), bounds.bottom()))
        };
        star.radius = static_cast<float>(random.real(0.7, 2.3));
        star.phase = static_cast<float>(random.real(0.0, 2.0 * std::numbers::pi));
        star.twinkle_speed = static_cast<float>(random.real(0.55, 2.25));
        star.twinkle_amplitude = static_cast<float>(random.real(18.0, 70.0));
        star.base_alpha = static_cast<std::uint8_t>(random.int_inclusive(95, 205));

        // A little temperature variation keeps the field from looking uniformly blue.
        const int temperature = random.int_inclusive(-18, 18);
        star.red = static_cast<std::uint8_t>(std::clamp(220 + temperature, 0, 255));
        star.green = static_cast<std::uint8_t>(std::clamp(230 + temperature / 3, 0, 255));
        star.blue = static_cast<std::uint8_t>(std::clamp(248 - temperature, 0, 255));
        _stars.push_back(star);
    }
}

void ArenaBackdrop::update(double delta_seconds)
{
    if (!std::isfinite(delta_seconds) || delta_seconds <= 0.0) return;
    _elapsed_seconds = std::fmod(_elapsed_seconds + delta_seconds, 3600.0);
}

void ArenaBackdrop::submit_render_commands(std::vector<elysia::core::RenderCommand>& commands) const
{
    commands.push_back(elysia::core::make_world_fill_rect_command(world_rect(), {7, 11, 28}));

    for (const auto& star : _stars)
    {
        const auto wave = std::sin(
            static_cast<float>(_elapsed_seconds) * star.twinkle_speed + star.phase);
        const auto alpha = static_cast<std::uint8_t>(std::clamp(
            static_cast<int>(std::lround(star.base_alpha + wave * star.twinkle_amplitude)),
            35, 255));
        commands.push_back(elysia::core::make_world_fill_circle_command(
            star.position, star.radius, {star.red, star.green, star.blue, alpha}));
    }
}
}
