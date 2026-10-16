#include "game_actions.h"
#include <stdexcept>

namespace game::input
{
elysia::input::InputActionMap make_game_actions()
{
    elysia::input::InputActionMap map;
    using namespace elysia::input;
    const bool registered =
        map.register_action(
            {.id = actions::CameraPan, .value_type = InputActionValueType::Axis2D},
            {{actions::CameraPan, Button2DInputBinding{
                .left = RawInputControl::KeyA, .right = RawInputControl::KeyD,
                .up = RawInputControl::KeyW, .down = RawInputControl::KeyS}}})
        && map.register_action(
            {.id = actions::CameraPanStick, .value_type = InputActionValueType::Axis2D,
             .dead_zone = 0.22f},
            {{actions::CameraPanStick, Axis2DInputBinding{
                    .x_axis = RawInputAxis::GamepadLeftX,
                    .y_axis = RawInputAxis::GamepadLeftY}}
            })
        && map.register_action(
            {.id = actions::AimStick, .value_type = InputActionValueType::Axis2D,
             .dead_zone = 0.22f},
            {{actions::AimStick, Axis2DInputBinding{
                .x_axis = RawInputAxis::GamepadRightX,
                .y_axis = RawInputAxis::GamepadRightY}}})
        && map.register_action(
            {.id = actions::PowerAdjust, .value_type = InputActionValueType::Axis1D},
            {
                {actions::PowerAdjust, ButtonInputBinding{.control = RawInputControl::KeyQ, .scale = -1.0f}},
                {actions::PowerAdjust, ButtonInputBinding{.control = RawInputControl::KeyE, .scale = 1.0f}},
                {actions::PowerAdjust, AxisInputBinding{.axis = RawInputAxis::GamepadLeftTrigger, .scale = -1.0f}},
                {actions::PowerAdjust, AxisInputBinding{.axis = RawInputAxis::GamepadRightTrigger, .scale = 1.0f}}
            })
        && map.register_action(
            {.id = actions::ZoomContinuous, .value_type = InputActionValueType::Axis1D},
            {
                {actions::ZoomContinuous, ButtonInputBinding{
                    .control = RawInputControl::GamepadLeftShoulder, .scale = -1.0f}},
                {actions::ZoomContinuous, ButtonInputBinding{
                    .control = RawInputControl::GamepadRightShoulder, .scale = 1.0f}}
            })
        && map.register_action(
            {.id = actions::ZoomWheel, .value_type = InputActionValueType::Axis1D,
             .semantics = InputValueSemantics::Delta},
            {{actions::ZoomWheel, PointerDeltaBinding{.axis = PointerDeltaAxis::WheelY}}})
        && map.register_action(
            {.id = actions::Fire, .value_type = InputActionValueType::Button},
            {
                {actions::Fire, ButtonInputBinding{.control = RawInputControl::MouseLeft}},
                {actions::Fire, ButtonInputBinding{.control = RawInputControl::GamepadSouth}}
            })
        && map.register_action(
            {.id = actions::Restart, .value_type = InputActionValueType::Button},
            {
                {actions::Restart, ButtonInputBinding{.control = RawInputControl::KeyR}},
                {actions::Restart, ButtonInputBinding{.control = RawInputControl::GamepadNorth}}
            });

    if (!registered || !map.valid())
        throw std::logic_error("Game input action map is invalid.");
    return map;
}
}
