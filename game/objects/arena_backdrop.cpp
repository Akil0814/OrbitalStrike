#include "arena_backdrop.h"

#include "engine/core/render/render_command.h"

namespace game::objects
{
ArenaBackdrop::ArenaBackdrop(elysia::core::Rect bounds)
    : GameObject(elysia::core::DepthLayer::Background)
{
    set_world_rect(bounds);
}

void ArenaBackdrop::submit_render_commands(std::vector<elysia::core::RenderCommand>& commands) const
{
    commands.push_back(elysia::core::make_world_fill_rect_command(world_rect(), {7, 11, 28}));
    constexpr elysia::core::Vector2 stars[] = {
        {95, 130}, {260, 260}, {410, 90}, {540, 780}, {750, 135}, {910, 640},
        {1040, 125}, {1190, 520}, {1420, 270}, {1510, 810}, {330, 900}, {780, 910}
    };
    for (const auto star : stars)
        commands.push_back(elysia::core::make_world_fill_circle_command(
            star, 2.0f, {170, 195, 255, 180}));
}
}
