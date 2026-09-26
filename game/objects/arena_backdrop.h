#pragma once

#include "engine/core/game_object.h"

namespace game::objects
{
class ArenaBackdrop final : public elysia::core::GameObject
{
public:
    explicit ArenaBackdrop(elysia::core::Rect bounds);
    void submit_render_commands(std::vector<elysia::core::RenderCommand>& commands) const override;
};
}
