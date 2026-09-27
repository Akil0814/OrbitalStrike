#pragma once

#include "engine/core/game_object.h"

namespace game::objects
{
class AimGuide final : public elysia::core::GameObject
{
public:
    AimGuide();

    void set_aim(elysia::core::Vector2 origin, elysia::core::Vector2 direction,
                 float power, bool visible) noexcept;
    void submit_render_commands(std::vector<elysia::core::RenderCommand>& commands) const override;

private:
    elysia::core::Vector2 _origin{};
    elysia::core::Vector2 _direction{};
    float _power = 0.0f;
    bool _visible = false;
};
}
