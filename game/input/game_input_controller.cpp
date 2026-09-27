#include "game_input_controller.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace game::input
{
namespace
{
constexpr float kGamepadPanDeadZone = 0.22f;
constexpr float kGamepadPanResponseExponent = 1.6f;
constexpr float kGamepadPanMaximumScale = 0.75f;
constexpr float kCameraPanHalfLifeSeconds = 0.10f;
constexpr float kGamepadAimEnterDeadZone = 0.30f;
constexpr float kGamepadAimExitDeadZone = 0.22f;
constexpr float kGamepadAimResponseExponent = 1.35f;
constexpr float kGamepadAimMaximumAngularSpeed = 2.617993878f;
constexpr float kLn2 = 0.69314718056f;
constexpr float kTwoPi = 6.28318530718f;

const elysia::input::InputActionId kCameraPanAction{"game.camera_pan"};
const elysia::input::InputActionId kCameraPanStickAction{"game.camera_pan_stick"};
const elysia::input::InputActionId kAimStickAction{"game.aim_stick"};
const elysia::input::InputActionId kPowerAdjustAction{"game.power_adjust"};
const elysia::input::InputActionId kZoomContinuousAction{"game.zoom_continuous"};
const elysia::input::InputActionId kZoomWheelAction{"game.zoom_wheel"};
const elysia::input::InputActionId kFireAction{"game.fire"};
const elysia::input::InputActionId kRestartAction{"game.restart"};

elysia::core::Vector2 apply_radial_response(
    elysia::core::Vector2 value,
    float dead_zone,
    float exponent) noexcept
{
    const float magnitude = std::min(1.0f, value.length());
    if (magnitude <= dead_zone) return {};
    const float remapped = (magnitude - dead_zone) / (1.0f - dead_zone);
    const float response = std::pow(std::clamp(remapped, 0.0f, 1.0f), exponent);
    return value.normalized() * response;
}
}

GameInputController::GameInputController()
{
    configure_actions();
}

void GameInputController::configure_actions()
{
    using namespace elysia::input;
    const bool registered =
        _actions.register_action(
            {.id = kCameraPanAction, .value_type = InputActionValueType::Axis2D},
            {{kCameraPanAction, Button2DInputBinding{
                .left = RawInputControl::KeyA, .right = RawInputControl::KeyD,
                .up = RawInputControl::KeyW, .down = RawInputControl::KeyS}}})
        && _actions.register_action(
            {.id = kCameraPanStickAction, .value_type = InputActionValueType::Axis2D,
             .dead_zone = kGamepadPanDeadZone},
            {{kCameraPanStickAction, Axis2DInputBinding{
                    .x_axis = RawInputAxis::GamepadLeftX,
                    .y_axis = RawInputAxis::GamepadLeftY}}
            })
        && _actions.register_action(
            {.id = kAimStickAction, .value_type = InputActionValueType::Axis2D,
             .dead_zone = kGamepadAimExitDeadZone},
            {{kAimStickAction, Axis2DInputBinding{
                .x_axis = RawInputAxis::GamepadRightX,
                .y_axis = RawInputAxis::GamepadRightY}}})
        && _actions.register_action(
            {.id = kPowerAdjustAction, .value_type = InputActionValueType::Axis1D},
            {
                {kPowerAdjustAction, ButtonInputBinding{.control = RawInputControl::KeyQ, .scale = -1.0f}},
                {kPowerAdjustAction, ButtonInputBinding{.control = RawInputControl::KeyE, .scale = 1.0f}},
                {kPowerAdjustAction, AxisInputBinding{.axis = RawInputAxis::GamepadLeftTrigger, .scale = -1.0f}},
                {kPowerAdjustAction, AxisInputBinding{.axis = RawInputAxis::GamepadRightTrigger, .scale = 1.0f}}
            })
        && _actions.register_action(
            {.id = kZoomContinuousAction, .value_type = InputActionValueType::Axis1D},
            {
                {kZoomContinuousAction, ButtonInputBinding{
                    .control = RawInputControl::GamepadLeftShoulder, .scale = -1.0f}},
                {kZoomContinuousAction, ButtonInputBinding{
                    .control = RawInputControl::GamepadRightShoulder, .scale = 1.0f}}
            })
        && _actions.register_action(
            {.id = kZoomWheelAction, .value_type = InputActionValueType::Axis1D,
             .semantics = InputValueSemantics::Delta},
            {{kZoomWheelAction, PointerDeltaBinding{.axis = PointerDeltaAxis::WheelY}}})
        && _actions.register_action(
            {.id = kFireAction, .value_type = InputActionValueType::Button},
            {
                {kFireAction, ButtonInputBinding{.control = RawInputControl::MouseLeft}},
                {kFireAction, ButtonInputBinding{.control = RawInputControl::GamepadSouth}}
            })
        && _actions.register_action(
            {.id = kRestartAction, .value_type = InputActionValueType::Button},
            {
                {kRestartAction, ButtonInputBinding{.control = RawInputControl::KeyR}},
                {kRestartAction, ButtonInputBinding{.control = RawInputControl::GamepadNorth}}
            });

    if (!registered || !_actions.valid())
        throw std::logic_error("Game input action map is invalid.");
}

GameInputCommands GameInputController::route(const elysia::input::InputSnapshot& input)
{
    GameInputCommands commands;
    if (input.focus_lost)
    {
        clear_transient_state();
        commands.focus_lost = true;
        return commands;
    }

    auto actions = _actions.resolve(input);
    for (const auto& event : input.events)
    {
        if (event.device != elysia::input::InputDevice::Unknown)
            _last_input_device = event.device;

        if (event.device == elysia::input::InputDevice::Mouse
            && (event.type == elysia::input::RawInputEventType::MouseMoved
                || event.type == elysia::input::RawInputEventType::ControlPressed))
        {
            _mouse_screen = {
                static_cast<float>(event.mouse_x), static_cast<float>(event.mouse_y)};
            _mouse_position_valid = true;
            _aim_mode = AimInputMode::Mouse;
            _gamepad_aim_active = false;
        }
    }

    auto keyboard_pan = actions.frame.axis2d(kCameraPanAction);
    if (keyboard_pan.length_squared() > 1.0f) keyboard_pan = keyboard_pan.normalized();
    const auto gamepad_pan = apply_radial_response(
        actions.frame.axis2d(kCameraPanStickAction),
        kGamepadPanDeadZone,
        kGamepadPanResponseExponent) * kGamepadPanMaximumScale;
    _camera_pan_target = keyboard_pan + gamepad_pan;
    if (_camera_pan_target.length_squared() > 1.0f)
        _camera_pan_target = _camera_pan_target.normalized();
    _power_adjustment = actions.frame.axis1d(kPowerAdjustAction);
    _continuous_zoom = actions.frame.axis1d(kZoomContinuousAction);

    const auto stick_aim = actions.frame.axis2d(kAimStickAction);
    const float stick_magnitude = stick_aim.length();
    const float activation_threshold = _gamepad_aim_active
        ? kGamepadAimExitDeadZone : kGamepadAimEnterDeadZone;
    if (stick_magnitude > activation_threshold)
    {
        _gamepad_aim_target = stick_aim.normalized();
        const float remapped_strength = std::clamp(
            (stick_magnitude - kGamepadAimEnterDeadZone)
                / (1.0f - kGamepadAimEnterDeadZone),
            0.0f, 1.0f);
        _gamepad_aim_strength = std::pow(
            remapped_strength, kGamepadAimResponseExponent);
        _gamepad_aim_active = true;
        _aim_mode = AimInputMode::Gamepad;
        _last_input_device = elysia::input::InputDevice::Gamepad;
    }
    else if (_gamepad_aim_active && stick_magnitude <= kGamepadAimExitDeadZone)
    {
        _gamepad_aim_active = false;
        _gamepad_aim_target = _aim_direction;
        _gamepad_aim_strength = 0.0f;
    }

    if (const auto found = actions.deltas.find(kZoomWheelAction); found != actions.deltas.end())
        commands.zoom_wheel_steps = found->second.x;
    commands.fire_pressed = actions.frame.is_just_pressed(kFireAction);
    commands.restart_pressed = actions.frame.is_just_pressed(kRestartAction);
    return commands;
}

void GameInputController::reset(elysia::core::Vector2 initial_aim) noexcept
{
    if (initial_aim.is_zero()) initial_aim = {1.0f, 0.0f};
    clear_transient_state();
    _aim_mode = AimInputMode::Mouse;
    _last_input_device = elysia::input::InputDevice::Keyboard;
    _mouse_position_valid = false;
    _aim_direction = initial_aim.normalized();
    _gamepad_aim_target = _aim_direction;
    _mouse_screen = {};
}

void GameInputController::clear_transient_state() noexcept
{
    _actions.reset_state();
    _gamepad_aim_active = false;
    _gamepad_aim_target = _aim_direction;
    _gamepad_aim_strength = 0.0f;
    _power_adjustment = 0.0f;
    _continuous_zoom = 0.0f;
    _camera_pan = {};
    _camera_pan_target = {};
}

void GameInputController::update_camera_pan(double delta) noexcept
{
    const float frame_delta = static_cast<float>(delta > 0.0 ? delta : 0.0);
    if (frame_delta <= 0.0f) return;
    const float blend = 1.0f - std::exp(
        -kLn2 * frame_delta / kCameraPanHalfLifeSeconds);
    _camera_pan += (_camera_pan_target - _camera_pan) * blend;
    if (_camera_pan_target.is_zero() && _camera_pan.length_squared() < 0.0001f)
        _camera_pan = {};
}

void GameInputController::update_aim(double delta, elysia::core::Vector2 origin,
                                     std::optional<elysia::core::Vector2> mouse_world) noexcept
{
    if (_aim_mode == AimInputMode::Mouse && mouse_world)
    {
        const auto direction = origin.direction_to(*mouse_world);
        if (!direction.is_zero()) _aim_direction = direction;
        return;
    }
    if (_aim_mode != AimInputMode::Gamepad || !_gamepad_aim_active) return;

    const float frame_delta = static_cast<float>(delta > 0.0 ? delta : 0.0);
    const float current_angle = std::atan2(_aim_direction.y, _aim_direction.x);
    const float target_angle = std::atan2(_gamepad_aim_target.y, _gamepad_aim_target.x);
    const float angle_delta = std::remainder(target_angle - current_angle, kTwoPi);
    const float maximum_step = kGamepadAimMaximumAngularSpeed
        * _gamepad_aim_strength * frame_delta;
    const float smoothed_angle = current_angle
        + std::clamp(angle_delta, -maximum_step, maximum_step);
    _aim_direction = {std::cos(smoothed_angle), std::sin(smoothed_angle)};
}

std::optional<elysia::core::Vector2> GameInputController::mouse_screen_position() const noexcept
{
    if (_aim_mode != AimInputMode::Mouse || !_mouse_position_valid) return std::nullopt;
    return _mouse_screen;
}
}
