#pragma once

#include "engine/core/geometry/vector2.h"
#include "engine/input/action/input_action_map.h"
#include "engine/input/input_snapshot.h"

#include <optional>

namespace game::input
{
struct GameInputCommands
{
    float zoom_wheel_steps = 0.0f;
    bool fire_pressed = false;
    bool restart_pressed = false;
    bool focus_lost = false;
};

class GameInputController final
{
public:
    GameInputController();

    [[nodiscard]] GameInputCommands route(const elysia::input::InputSnapshot& input);
    void reset(elysia::core::Vector2 initial_aim = {1.0f, 0.0f}) noexcept;
    void update_aim(double delta, elysia::core::Vector2 origin,
                    std::optional<elysia::core::Vector2> mouse_world) noexcept;

    [[nodiscard]] elysia::core::Vector2 camera_pan() const noexcept { return _camera_pan; }
    [[nodiscard]] float power_adjustment() const noexcept { return _power_adjustment; }
    [[nodiscard]] float continuous_zoom() const noexcept { return _continuous_zoom; }
    [[nodiscard]] elysia::core::Vector2 aim_direction() const noexcept { return _aim_direction; }
    [[nodiscard]] std::optional<elysia::core::Vector2> mouse_screen_position() const noexcept;
    [[nodiscard]] elysia::input::InputDevice last_input_device() const noexcept
    {
        return _last_input_device;
    }

private:
    enum class AimInputMode : unsigned char { Mouse, Gamepad };

    void configure_actions();
    void clear_transient_state() noexcept;

    elysia::input::InputActionMap _actions;
    AimInputMode _aim_mode = AimInputMode::Mouse;
    elysia::input::InputDevice _last_input_device = elysia::input::InputDevice::Keyboard;
    bool _mouse_position_valid = false;
    bool _gamepad_aim_active = false;
    float _power_adjustment = 0.0f;
    float _continuous_zoom = 0.0f;
    elysia::core::Vector2 _camera_pan{};
    elysia::core::Vector2 _aim_direction{1.0f, 0.0f};
    elysia::core::Vector2 _gamepad_aim_target{1.0f, 0.0f};
    elysia::core::Vector2 _mouse_screen{};
};
}
