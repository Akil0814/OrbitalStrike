#include "arena_backdrop.h"

#include "engine/core/render/render_command.h"
#include "engine/tools/random_generator.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace game::presentation
{
ArenaBackdrop::ArenaBackdrop(elysia::core::Rect bounds, StarfieldConfig config)
    : GameObject(elysia::core::DepthLayer::Background)
{
    set_world_rect(bounds);
    config.stars_per_million_units = std::max(0.0f, config.stars_per_million_units);
    config.minimum_stars = std::max(0, config.minimum_stars);
    config.maximum_stars = std::max(config.minimum_stars, config.maximum_stars);
    config.twinkle_ratio = std::clamp(config.twinkle_ratio, 0.0f, 1.0f);

    const double area_in_millions = static_cast<double>(bounds.area()) / 1'000'000.0;
    const int requested_count = static_cast<int>(std::lround(
        area_in_millions * static_cast<double>(config.stars_per_million_units)));
    const int star_count = std::clamp(
        requested_count, config.minimum_stars, config.maximum_stars);
    _stars.reserve(static_cast<std::size_t>(star_count));

    if (bounds.is_empty()) return;
    elysia::tools::RandomGenerator random(config.seed);
    for (int index = 0; index < star_count; ++index)
    {
        Star star;
        star.position = {
            static_cast<float>(random.real(bounds.left(), bounds.right())),
            static_cast<float>(random.real(bounds.top(), bounds.bottom()))};

        const double size_class = random.real(0.0, 1.0);
        if (size_class < 0.70)
        {
            star.radius = static_cast<float>(random.real(0.6, 1.05));
            star.base_alpha = static_cast<std::uint8_t>(random.int_inclusive(70, 140));
        }
        else if (size_class < 0.95)
        {
            star.radius = static_cast<float>(random.real(1.05, 1.75));
            star.base_alpha = static_cast<std::uint8_t>(random.int_inclusive(105, 195));
        }
        else
        {
            star.radius = static_cast<float>(random.real(1.8, 2.8));
            star.base_alpha = static_cast<std::uint8_t>(random.int_inclusive(175, 240));
        }

        star.twinkles = random.chance(config.twinkle_ratio);
        star.phase = static_cast<float>(random.real(0.0, 2.0 * std::numbers::pi));
        star.twinkle_speed = static_cast<float>(random.real(0.55, 2.25));
        star.twinkle_amplitude = star.twinkles
            ? static_cast<float>(random.real(18.0, 70.0)) : 0.0f;

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

void ArenaBackdrop::set_visible_bounds(elysia::core::Rect bounds) noexcept
{
    _visible_bounds = bounds.expanded(64.0f);
}

void ArenaBackdrop::submit_render_commands(std::vector<elysia::core::RenderCommand>& commands) const
{
    commands.push_back(elysia::core::make_world_fill_rect_command(world_rect(), {7, 11, 28}));
    if (_visible_bounds.is_empty()) return;

    for (const auto& star : _stars)
    {
        if (!_visible_bounds.contains(star.position)) continue;
        std::uint8_t alpha = star.base_alpha;
        if (star.twinkles)
        {
            const auto wave = std::sin(
                static_cast<float>(_elapsed_seconds) * star.twinkle_speed + star.phase);
            alpha = static_cast<std::uint8_t>(std::clamp(
                static_cast<int>(std::lround(star.base_alpha + wave * star.twinkle_amplitude)),
                35, 255));
        }
        commands.push_back(elysia::core::make_world_fill_circle_command(
            star.position, star.radius, {star.red, star.green, star.blue, alpha}));
    }
}
}
