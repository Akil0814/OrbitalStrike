#include "game_scene.h"

#include "../objects/bullet.h"
#include "../objects/celestial_body.h"

#include "engine/camera/camera_manager.h"
#include "engine/camera/follow_strategy.h"
#include "engine/core/render/render_command.h"
#include "engine/ui/text/ui_text_content.h"
#include "engine/ui/widgets/label/ui_label.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>

namespace game::scene
{
namespace
{
constexpr elysia::core::Rect kFlightBounds{0.0f, 0.0f, 1600.0f, 1000.0f};
constexpr float kMinPower = 300.0f;
constexpr float kMaxPower = 1100.0f;
constexpr float kPowerRate = 250.0f;
constexpr float kCameraPanSpeed = 650.0f;
constexpr float kMinimumZoom = 0.55f;
constexpr float kMaximumZoom = 1.6f;
constexpr float kGamepadAimEnterDeadZone = 0.28f;
constexpr float kGamepadAimExitDeadZone = 0.20f;
constexpr float kGamepadAimHalfLifeSeconds = 0.10f;
constexpr float kLn2 = 0.69314718056f;
constexpr float kTwoPi = 6.28318530718f;
constexpr float kGravityRange = 520.0f;
constexpr float kGravityMinimumDistance = 140.0f;
constexpr float kGravityMaximumForce = 80.0f;

const elysia::input::InputActionId kCameraPanAction{"game.camera_pan"};
const elysia::input::InputActionId kAimStickAction{"game.aim_stick"};
const elysia::input::InputActionId kPowerAdjustAction{"game.power_adjust"};
const elysia::input::InputActionId kZoomContinuousAction{"game.zoom_continuous"};
const elysia::input::InputActionId kZoomWheelAction{"game.zoom_wheel"};
const elysia::input::InputActionId kFireAction{"game.fire"};
const elysia::input::InputActionId kRestartAction{"game.restart"};

class ArenaBackdrop final : public elysia::core::GameObject
{
public:
    ArenaBackdrop() : GameObject(elysia::core::DepthLayer::Background)
    {
        set_world_rect(kFlightBounds);
    }

    void submit_render_commands(std::vector<elysia::core::RenderCommand>& commands) const override
    {
        commands.push_back(elysia::core::make_world_fill_rect_command(world_rect(), {7, 11, 28}));
        constexpr elysia::core::Vector2 stars[] = {
            {95, 130}, {260, 260}, {410, 90}, {540, 780}, {750, 135}, {910, 640},
            {1040, 125}, {1190, 520}, {1420, 270}, {1510, 810}, {330, 900}, {780, 910}
        };
        for (const auto star : stars)
            commands.push_back(elysia::core::make_world_fill_circle_command(star, 2.0f, {170, 195, 255, 180}));
    }
};

class AimGuide final : public elysia::core::GameObject
{
public:
    AimGuide() : GameObject(elysia::core::DepthLayer::EffectFront) {}

    void set_aim(elysia::core::Vector2 origin, elysia::core::Vector2 direction, float power, bool visible)
    {
        _origin = origin;
        _direction = direction;
        _power = power;
        _visible = visible;
    }

    void submit_render_commands(std::vector<elysia::core::RenderCommand>& commands) const override
    {
        if (!_visible || _direction.is_zero()) return;
        const auto end = _origin + _direction * (_power * 0.18f);
        commands.push_back(elysia::core::make_world_draw_line_command(_origin, end, {124, 220, 255}, 2.0f));
        commands.push_back(elysia::core::make_world_fill_circle_command(end, 5.0f, {124, 220, 255}));
    }

private:
    elysia::core::Vector2 _origin{}, _direction{};
    float _power = 0.0f;
    bool _visible = false;
};
} // namespace

GameScene::GameScene()
    : Scene(elysia::physics::PhysicsWorldConfig{.gravity = {}})
{
    configure_input_actions();
}

void GameScene::configure_input_actions()
{
    using namespace elysia::input;

    const bool registered =
        _input_actions.register_action(
            {.id = kCameraPanAction, .value_type = InputActionValueType::Axis2D},
            {
                {kCameraPanAction, Button2DInputBinding{
                    .left = RawInputControl::KeyA,
                    .right = RawInputControl::KeyD,
                    .up = RawInputControl::KeyW,
                    .down = RawInputControl::KeyS}},
                {kCameraPanAction, Axis2DInputBinding{
                    .x_axis = RawInputAxis::GamepadLeftX,
                    .y_axis = RawInputAxis::GamepadLeftY}}
            })
        && _input_actions.register_action(
            {.id = kAimStickAction,
             .value_type = InputActionValueType::Axis2D,
             .dead_zone = kGamepadAimExitDeadZone},
            {{kAimStickAction, Axis2DInputBinding{
                .x_axis = RawInputAxis::GamepadRightX,
                .y_axis = RawInputAxis::GamepadRightY}}})
        && _input_actions.register_action(
            {.id = kPowerAdjustAction, .value_type = InputActionValueType::Axis1D},
            {
                {kPowerAdjustAction, ButtonInputBinding{.control = RawInputControl::KeyQ, .scale = -1.0f}},
                {kPowerAdjustAction, ButtonInputBinding{.control = RawInputControl::KeyE, .scale = 1.0f}},
                {kPowerAdjustAction, AxisInputBinding{.axis = RawInputAxis::GamepadLeftTrigger, .scale = -1.0f}},
                {kPowerAdjustAction, AxisInputBinding{.axis = RawInputAxis::GamepadRightTrigger, .scale = 1.0f}}
            })
        && _input_actions.register_action(
            {.id = kZoomContinuousAction, .value_type = InputActionValueType::Axis1D},
            {
                {kZoomContinuousAction, ButtonInputBinding{
                    .control = RawInputControl::GamepadLeftShoulder, .scale = -1.0f}},
                {kZoomContinuousAction, ButtonInputBinding{
                    .control = RawInputControl::GamepadRightShoulder, .scale = 1.0f}}
            })
        && _input_actions.register_action(
            {.id = kZoomWheelAction,
             .value_type = InputActionValueType::Axis1D,
             .semantics = InputValueSemantics::Delta},
            {{kZoomWheelAction, PointerDeltaBinding{.axis = PointerDeltaAxis::WheelY}}})
        && _input_actions.register_action(
            {.id = kFireAction, .value_type = InputActionValueType::Button},
            {
                {kFireAction, ButtonInputBinding{.control = RawInputControl::MouseLeft}},
                {kFireAction, ButtonInputBinding{.control = RawInputControl::GamepadSouth}}
            })
        && _input_actions.register_action(
            {.id = kRestartAction, .value_type = InputActionValueType::Button},
            {
                {kRestartAction, ButtonInputBinding{.control = RawInputControl::KeyR}},
                {kRestartAction, ButtonInputBinding{.control = RawInputControl::GamepadNorth}}
            });

    if (!registered || !_input_actions.valid())
        throw std::logic_error("GameScene input action map is invalid.");
}

void GameScene::reset_input_state()
{
    _input_actions.reset_state();
    _camera_pan_input = {};
    _camera_pan_offset = {};
    _gamepad_aim_active = false;
    _gamepad_aim_target = _aim_direction;
    _power_input = 0.0f;
    _zoom_input = 0.0f;
}

void GameScene::on_enter(const elysia::scene::ScenePayload& payload)
{
    (void)payload;
    if (!_level_built)
        build_level();

    _impact_hold_timer.set_one_shot(true);
    _impact_hold_timer.set_on_timeout([this]
        {
            _state = RoundState::Aiming;
        });
    _impact_hold_timer.set_wait_time(1.0);
    _impact_hold_timer.pause();

}

void GameScene::on_exit()
{
    clear_level();
}

void GameScene::reset()
{
    clear_level();
}

void GameScene::on_update(double delta)
{
    if (!_level_built) build_level();
    const float frame_delta = static_cast<float>(std::max(0.0, delta));

    if (std::fabs(_zoom_input) > elysia::core::Vector2::k_epsilon)
    {
        const float requested = camera().zoom() * std::exp(_zoom_input * frame_delta);
        elysia::camera::CameraManager::instance()->set_zoom(
            render_camera_slot(), std::clamp(requested, kMinimumZoom, kMaximumZoom));
    }

    if (_state == RoundState::Aiming)
    {
        _power = std::clamp(_power + _power_input * kPowerRate * frame_delta, kMinPower, kMaxPower);

        elysia::core::Vector2 pan = _camera_pan_input;
        if (pan.length_squared() > 1.0f)
            pan = pan.normalized();
        const float zoom = std::max(camera().zoom(), elysia::core::Vector2::k_epsilon);
        _camera_pan_offset += pan * (kCameraPanSpeed / zoom * frame_delta);

        const auto player_center = _player->center();
        const elysia::core::Vector2 camera_target{
            std::clamp(player_center.x + _camera_pan_offset.x, kFlightBounds.left(), kFlightBounds.right()),
            std::clamp(player_center.y + _camera_pan_offset.y, kFlightBounds.top(), kFlightBounds.bottom())};
        _camera_pan_offset = camera_target - player_center;

        if (_aim_input_mode == AimInputMode::Mouse && _mouse_position_valid)
        {
            const auto mouse_world = camera().screen_to_world(_mouse_screen);
            const auto mouse_direction = _player->center().direction_to(mouse_world);
            if (!mouse_direction.is_zero())
                _aim_direction = mouse_direction;
        }
        else if (_aim_input_mode == AimInputMode::Gamepad && _gamepad_aim_active)
        {
            const float current_angle = std::atan2(_aim_direction.y, _aim_direction.x);
            const float target_angle = std::atan2(_gamepad_aim_target.y, _gamepad_aim_target.x);
            const float angle_delta = std::remainder(target_angle - current_angle, kTwoPi);
            const float blend = 1.0f - std::exp(-kLn2 * frame_delta / kGamepadAimHalfLifeSeconds);
            const float smoothed_angle = current_angle + angle_delta * blend;
            _aim_direction = {std::cos(smoothed_angle), std::sin(smoothed_angle)};
        }

        if (auto* guide = dynamic_cast<AimGuide*>(_aim_guide))
            guide->set_aim(_player->center(), _aim_direction, _power, true);
    }
    else if (auto* guide = dynamic_cast<AimGuide*>(_aim_guide))
    {
        guide->set_aim({}, {}, 0.0f, false);
    }
    update_hud();

    _impact_hold_timer.update(delta);

    Scene::on_update(delta);

}

void GameScene::on_routed_input(const elysia::input::InputSnapshot& input)
{
    if (input.focus_lost)
    {
        reset_input_state();
        return;
    }

    auto actions = _input_actions.resolve(input);

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
            _aim_input_mode = AimInputMode::Mouse;
            _gamepad_aim_active = false;
            _mouse_position_valid = true;
            if (_state == RoundState::Aiming && _player)
            {
                const auto mouse_world = camera().screen_to_world(_mouse_screen);
                const auto mouse_direction = _player->center().direction_to(mouse_world);
                if (!mouse_direction.is_zero())
                    _aim_direction = mouse_direction;
            }
        }
    }

    _camera_pan_input = actions.frame.axis2d(kCameraPanAction);
    _power_input = actions.frame.axis1d(kPowerAdjustAction);
    _zoom_input = actions.frame.axis1d(kZoomContinuousAction);

    const auto stick_aim = actions.frame.axis2d(kAimStickAction);
    const float stick_magnitude = stick_aim.length();
    const float activation_threshold = _gamepad_aim_active
        ? kGamepadAimExitDeadZone
        : kGamepadAimEnterDeadZone;
    if (stick_magnitude > activation_threshold)
    {
        _gamepad_aim_target = stick_aim.normalized();
        _gamepad_aim_active = true;
        _aim_input_mode = AimInputMode::Gamepad;
        _last_input_device = elysia::input::InputDevice::Gamepad;
    }
    else if (_gamepad_aim_active && stick_magnitude <= kGamepadAimExitDeadZone)
    {
        _gamepad_aim_active = false;
        _gamepad_aim_target = _aim_direction;
    }

    if (const auto found = actions.deltas.find(kZoomWheelAction); found != actions.deltas.end())
    {
        const float requested = camera().zoom() * std::pow(1.1f, found->second.x);
        elysia::camera::CameraManager::instance()->set_zoom(
            render_camera_slot(), std::clamp(requested, kMinimumZoom, kMaximumZoom));
    }

    if (_state == RoundState::Aiming && actions.frame.is_just_pressed(kFireAction))
        launch_bullet();
    else if (_state == RoundState::Victory && actions.frame.is_just_pressed(kRestartAction))
        restart_level();
}

void GameScene::on_fixed_update(std::uint64_t tick, double delta)
{
    (void)tick;
    (void)delta;
    if (_state != RoundState::Flight || !_active_bullet || _active_bullet->finished()) return;

    for (const auto* body : {_player, _enemy, _neutral})
    {
        const auto offset = _active_bullet->center() - body->center();
        const float distance_squared = offset.length_squared();
        if (distance_squared <= elysia::core::Vector2::k_epsilon
            || distance_squared > kGravityRange * kGravityRange) continue;
        const float clamped_distance_squared = std::max(
            distance_squared, kGravityMinimumDistance * kGravityMinimumDistance);
        const float strength = std::min(kGravityMaximumForce,
            body->gravity_strength() * (kGravityMinimumDistance * kGravityMinimumDistance / clamped_distance_squared));
        (void)physics_world().apply_force(
            _active_bullet->physics_handle(), (-offset).normalized() * strength);
    }
}

void GameScene::on_scene_object_registered(elysia::core::SceneObject& object)
{
    if (auto* bullet = dynamic_cast<game::objects::Bullet*>(&object))
        bullet->attach_collision_listener();
}

std::optional<elysia::camera::CameraFocus> GameScene::resolve_camera_focus() const
{
    if (_state == RoundState::Flight && _active_bullet && !_active_bullet->is_destroyed())
        return elysia::camera::CameraFocus{_active_bullet->world_rect(), _active_bullet->world_rect()};
    if (_player && !_player->is_destroyed())
    {
        auto focus = _player->circle_bounds();
        if (_state == RoundState::Aiming)
            focus = focus.translated(_camera_pan_offset);
        return elysia::camera::CameraFocus{focus, focus};
    }
    return std::nullopt;
}

void GameScene::build_level()
{
    _background = create_and_add_object<ArenaBackdrop>();
    _player = create_and_add_object<game::objects::CelestialBody>(game::objects::CelestialBodyConfig{
        .center = {230.0f, 720.0f}, .radius = 66.0f, .faction = game::objects::CelestialFaction::Player,
        .hit_points = 1, .gravity_strength = 16.0f, .color = {63, 145, 255}});
    _enemy = create_and_add_object<game::objects::CelestialBody>(game::objects::CelestialBodyConfig{
        .center = {1320.0f, 240.0f}, .radius = 74.0f, .faction = game::objects::CelestialFaction::Enemy,
        .hit_points = 3, .gravity_strength = 20.0f, .color = {218, 70, 86}});
    _neutral = create_and_add_object<game::objects::CelestialBody>(game::objects::CelestialBodyConfig{
        .center = {760.0f, 570.0f}, .radius = 100.0f, .faction = game::objects::CelestialFaction::Neutral,
        .hit_points = 1, .gravity_strength = 35.0f, .color = {126, 102, 176}});
    _aim_guide = create_and_add_object<AimGuide>();
    _hud = create_and_add_object<elysia::ui::UiLabel>(elysia::core::Rect{24.0f, 20.0f, 600.0f, 45.0f});
    if (_hud)
    {
        _hud->set_text_fit_mode(elysia::ui::UiLabelTextFitMode::ShrinkToFit);
        _hud->set_text_content(elysia::ui::ui_raw_text(""));
    }
    _bullet_factory = std::make_unique<game::objects::BulletFactory>(*this);
    _aim_direction = _player->center().direction_to(_enemy->center());
    _gamepad_aim_target = _aim_direction;
    _gamepad_aim_active = false;
    _aim_input_mode = AimInputMode::Mouse;
    _last_input_device = elysia::input::InputDevice::Keyboard;
    _mouse_screen = {};
    _mouse_position_valid = false;
    _camera_pan_offset = {};
    reset_input_state();
    _power = 700.0f;
    _state = RoundState::Aiming;
    _level_built = true;

    auto* cameras = elysia::camera::CameraManager::instance();
    cameras->set_follow_strategy(render_camera_slot(),
        std::make_unique<elysia::camera::SmoothFollowStrategy>(1800.0));
    cameras->set_zoom(render_camera_slot(), 0.8f);
}

void GameScene::clear_level()
{
    if (_active_bullet) _active_bullet->detach_collision_listener();
    const auto destroy = [](elysia::core::SceneObject* object) {
        if (object && !object->is_destroyed()) object->destroy();
    };
    destroy(_active_bullet);
    destroy(_player);
    destroy(_enemy);
    destroy(_neutral);
    destroy(_background);
    destroy(_aim_guide);
    destroy(_hud);
    physics_world().reset();
    _active_bullet = nullptr;
    _player = _enemy = _neutral = nullptr;
    _background = _aim_guide = nullptr;
    _hud = nullptr;
    _bullet_factory.reset();
    _level_built = false;
    _camera_pan_offset = {};
    reset_input_state();
}

void GameScene::restart_level()
{
    clear_level();
    build_level();
}

void GameScene::update_hud()
{
    if (!_hud || !_enemy) return;
    std::string text;
    const bool gamepad = _last_input_device == elysia::input::InputDevice::Gamepad;
    if (_state == RoundState::Victory)
        text = gamepad ? "Victory! Press Y to restart" : "Victory! Press R to restart";
    else if (_state == RoundState::Flight)
        text = gamepad ? "Projectile in flight | LB/RB zoom" : "Projectile in flight | Mouse wheel zoom";
    else if (gamepad)
        text = "LS move | RS aim | LT/RT power: " + std::to_string(static_cast<int>(std::lround(_power)))
            + " | LB/RB zoom | A fire | Target HP: " + std::to_string(_enemy->hit_points());
    else
        text = "WASD move | Mouse aim | Q/E power: " + std::to_string(static_cast<int>(std::lround(_power)))
            + " | Wheel zoom | Left click fire | Target HP: " + std::to_string(_enemy->hit_points());
    _hud->set_text_content(elysia::ui::ui_raw_text(std::move(text)));
}

void GameScene::launch_bullet()
{
    if (!_bullet_factory || !_player || _active_bullet) return;
    const auto direction = _aim_direction.normalized();
    if (direction.is_zero()) return;

    _state = RoundState::Flight;
    _active_bullet = _bullet_factory->spawn({
        .position = _player->center() + direction * (_player->radius() + 10.0f),
        .velocity = direction * _power,
        .damage = 1,
        .on_hit = [this](elysia::physics::ColliderId collider, int damage) { on_bullet_hit(collider, damage); },
        .on_finished = [this](game::objects::BulletEndReason reason) { on_bullet_finished(reason); }
    });
    if (_active_bullet)
        _camera_pan_offset = {};
    else
        _state = RoundState::Aiming;
}

void GameScene::on_bullet_hit(elysia::physics::ColliderId collider, int damage)
{
    if (_enemy && collider == _enemy->physics_collider(0) && _enemy->apply_damage(damage))
        _state = RoundState::Victory;
}

void GameScene::on_bullet_finished(game::objects::BulletEndReason reason)
{
    (void)reason;
    _active_bullet = nullptr;
    if (_state == RoundState::Flight)
    {
        _state = RoundState::Resolving;
        _impact_hold_timer.restart();
    }

    elysia::camera::CameraManager::instance()->request_zoom_to(render_camera_slot(), 0.8f, 0.35);
}

} // namespace game::scene
