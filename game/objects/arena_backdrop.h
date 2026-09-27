#pragma once

#include "starfield_config.h"
#include "engine/core/game_object.h"
#include "engine/core/interface/updatable.h"

#include <cstdint>
#include <vector>

namespace game::objects
{
class ArenaBackdrop final : public elysia::core::GameObject, public elysia::core::Updatable
{
public:
    ArenaBackdrop(elysia::core::Rect bounds, StarfieldConfig config);
    void update(double delta_seconds) override;
    void set_visible_bounds(elysia::core::Rect bounds) noexcept;
    void submit_render_commands(std::vector<elysia::core::RenderCommand>& commands) const override;

private:
    struct Star
    {
        elysia::core::Vector2 position{};
        float radius = 1.0f;
        float phase = 0.0f;
        float twinkle_speed = 1.0f;
        float twinkle_amplitude = 0.0f;
        std::uint8_t red = 220;
        std::uint8_t green = 230;
        std::uint8_t blue = 255;
        std::uint8_t base_alpha = 160;
        bool twinkles = false;
    };

    std::vector<Star> _stars;
    elysia::core::Rect _visible_bounds{};
    double _elapsed_seconds = 0.0;
};
}
