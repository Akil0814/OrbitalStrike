#include "game_scene.h"

#include "../level/game_level_catalog.h"
#include "../objects/aim_guide.h"
#include "../objects/bullet.h"
#include "../objects/celestial_body.h"

#include "engine/camera/camera_manager.h"
#include "engine/camera/follow_strategy.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>

namespace game::scene
{
namespace
{
constexpr float kMinPower = 300.0f;
constexpr float kMaxPower = 1100.0f;
constexpr float kPowerRate = 250.0f;
constexpr float kCameraPanSpeed = 650.0f;
constexpr float kMinimumZoom = 0.55f;
constexpr float kMaximumZoom = 1.6f;
}

GameScene::GameScene()
    : Scene(elysia::physics::PhysicsWorldConfig{.gravity = {}}),
      _bullet_factory(*this)
{
    _impact_hold_timer.set_one_shot(true);
    _impact_hold_timer.set_wait_time(1.0);
    _impact_hold_timer.set_on_timeout([this] { finish_resolution(); });
    _impact_hold_timer.pause();
}

void GameScene::on_enter(const elysia::scene::ScenePayload& payload)
{
    const auto* game_payload = elysia::scene::try_scene_payload<game::level::GameScenePayload>(payload);
    if (!game_payload)
        throw std::logic_error("GameScene requires GameScenePayload.");

    if (_level.is_built()) clear_level();
    _level_id = game_payload->level_id;
    build_level();
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
    if (!_level.is_built())
    {
        Scene::on_update(delta);
        return;
    }

    const float frame_delta = static_cast<float>(std::max(0.0, delta));
    if (std::fabs(_input.continuous_zoom()) > elysia::core::Vector2::k_epsilon)
    {
        const float requested = camera().zoom() * std::exp(_input.continuous_zoom() * frame_delta);
        elysia::camera::CameraManager::instance()->set_zoom(
            render_camera_slot(), std::clamp(requested, kMinimumZoom, kMaximumZoom));
    }

    auto* player = _level.player();
    if (_state == RoundState::Aiming && player)
    {
        _power = std::clamp(
            _power + _input.power_adjustment() * kPowerRate * frame_delta,
            kMinPower, kMaxPower);

        elysia::core::Vector2 pan = _input.camera_pan();
        if (pan.length_squared() > 1.0f) pan = pan.normalized();
        const float zoom = std::max(camera().zoom(), elysia::core::Vector2::k_epsilon);
        _camera_pan_offset += pan * (kCameraPanSpeed / zoom * frame_delta);

        const auto& bounds = _level.definition()->activity_bounds;
        const auto player_center = player->center();
        const elysia::core::Vector2 camera_target{
            std::clamp(player_center.x + _camera_pan_offset.x, bounds.left(), bounds.right()),
            std::clamp(player_center.y + _camera_pan_offset.y, bounds.top(), bounds.bottom())};
        _camera_pan_offset = camera_target - player_center;

        std::optional<elysia::core::Vector2> mouse_world;
        if (const auto mouse_screen = _input.mouse_screen_position())
            mouse_world = camera().screen_to_world(*mouse_screen);
        _input.update_aim(delta, player_center, mouse_world);
        if (_aim_guide)
            _aim_guide->set_aim(player_center, _input.aim_direction(), _power, true);
    }
    else if (_aim_guide)
    {
        _aim_guide->set_aim({}, {}, 0.0f, false);
    }

    update_hud();
    _impact_hold_timer.update(delta);
    Scene::on_update(delta);
}

void GameScene::on_routed_input(const elysia::input::InputSnapshot& input)
{
    const auto commands = _input.route(input);
    if (commands.focus_lost)
    {
        _camera_pan_offset = {};
        return;
    }

    if (_state == RoundState::Aiming)
    {
        if (auto* player = _level.player())
        {
            std::optional<elysia::core::Vector2> mouse_world;
            if (const auto mouse_screen = _input.mouse_screen_position())
                mouse_world = camera().screen_to_world(*mouse_screen);
            _input.update_aim(0.0, player->center(), mouse_world);
        }
    }

    if (std::fabs(commands.zoom_wheel_steps) > elysia::core::Vector2::k_epsilon)
    {
        const float requested = camera().zoom() * std::pow(1.1f, commands.zoom_wheel_steps);
        elysia::camera::CameraManager::instance()->set_zoom(
            render_camera_slot(), std::clamp(requested, kMinimumZoom, kMaximumZoom));
    }

    if (_state == RoundState::Aiming && commands.fire_pressed)
        launch_bullet();
    else if (_state == RoundState::Victory && commands.restart_pressed)
        restart_level();
}

void GameScene::on_fixed_update(std::uint64_t tick, double delta)
{
    (void)tick;
    (void)delta;
    if (_state != RoundState::Flight || !_active_bullet || _active_bullet->finished()) return;
    const auto* definition = _level.definition();
    if (!definition) return;

    const auto& gravity = definition->gravity;
    for (const auto* body : _level.bodies())
    {
        const auto offset = _active_bullet->center() - body->center();
        const float distance_squared = offset.length_squared();
        if (distance_squared <= elysia::core::Vector2::k_epsilon
            || distance_squared > gravity.maximum_range * gravity.maximum_range) continue;
        const float minimum_distance_squared = gravity.minimum_distance * gravity.minimum_distance;
        const float clamped_distance_squared = std::max(distance_squared, minimum_distance_squared);
        const float strength = std::min(
            gravity.maximum_force,
            body->gravity_strength() * (minimum_distance_squared / clamped_distance_squared));
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
    if ((_state == RoundState::Resolving || _state == RoundState::Victory) && _resolution_focus)
        return elysia::camera::CameraFocus{*_resolution_focus, *_resolution_focus};
    if (const auto* player = _level.player(); player && !player->is_destroyed())
    {
        auto focus = player->circle_bounds();
        if (_state == RoundState::Aiming) focus = focus.translated(_camera_pan_offset);
        return elysia::camera::CameraFocus{focus, focus};
    }
    return std::nullopt;
}

void GameScene::build_level()
{
    if (!_level_id) throw std::logic_error("Cannot build a game level without a selected level id.");
    const auto& definition = game::level::GameLevelCatalog::get(*_level_id);

    try
    {
        _level.build(*this, definition);
        _aim_guide = create_and_add_object<game::objects::AimGuide>();
        if (!_aim_guide) throw std::runtime_error("GameScene failed to create AimGuide.");
        _hud.build(*this);

        const auto initial_aim = _level.player()->center().direction_to(_level.enemy()->center());
        _input.reset(initial_aim);
        _camera_pan_offset = {};
        _power = definition.initial_power;
        _state = RoundState::Aiming;
        _impact_hold_timer.pause();

        auto* cameras = elysia::camera::CameraManager::instance();
        cameras->set_follow_strategy(
            render_camera_slot(), std::make_unique<elysia::camera::SmoothFollowStrategy>(1800.0));
        cameras->set_zoom(render_camera_slot(), definition.initial_zoom);
        update_hud();
    }
    catch (...)
    {
        clear_level();
        throw;
    }
}

void GameScene::clear_level() noexcept
{
    if (_active_bullet)
    {
        _active_bullet->detach_collision_listener();
        if (!_active_bullet->is_destroyed()) _active_bullet->destroy();
    }
    if (_aim_guide && !_aim_guide->is_destroyed()) _aim_guide->destroy();

    _active_bullet = nullptr;
    _aim_guide = nullptr;
    _hud.clear();
    _level.clear();
    physics_world().reset();
    _camera_pan_offset = {};
    _resolution_focus.reset();
    _input.reset();
    _impact_hold_timer.pause();
    _state = RoundState::Aiming;
}

void GameScene::restart_level()
{
    clear_level();
    build_level();
}

void GameScene::finish_resolution()
{
    if (_state != RoundState::Resolving) return;
    _resolution_focus.reset();
    _state = RoundState::Aiming;

    const auto* definition = _level.definition();
    const float target_zoom = definition ? definition->initial_zoom : 0.8f;
    elysia::camera::CameraManager::instance()->request_zoom_to(
        render_camera_slot(), target_zoom, 0.35);
}

void GameScene::update_hud()
{
    const auto* enemy = _level.enemy();
    if (!enemy) return;
    _hud.update({
        .victory = _state == RoundState::Victory,
        .projectile_in_flight = _state == RoundState::Flight,
        .resolving = _state == RoundState::Resolving,
        .input_device = _input.last_input_device(),
        .power = _power,
        .target_hit_points = enemy->hit_points()});
}

void GameScene::launch_bullet()
{
    auto* player = _level.player();
    const auto* definition = _level.definition();
    if (!player || !definition || _active_bullet) return;
    const auto direction = _input.aim_direction().normalized();
    if (direction.is_zero()) return;

    _resolution_focus.reset();
    _state = RoundState::Flight;
    _active_bullet = _bullet_factory.spawn({
        .position = player->center() + direction * (player->radius() + 10.0f),
        .velocity = direction * _power,
        .flight_bounds = definition->activity_bounds,
        .damage = 1,
        .on_hit = [this](elysia::physics::ColliderId collider, int damage) {
            on_bullet_hit(collider, damage);
        },
        .on_finished = [this](game::objects::BulletEndReason reason) {
            on_bullet_finished(reason);
        }});
    if (_active_bullet)
        _camera_pan_offset = {};
    else
        _state = RoundState::Aiming;
}

void GameScene::on_bullet_hit(elysia::physics::ColliderId collider, int damage)
{
    auto* enemy = _level.enemy();
    if (enemy && collider == enemy->physics_collider(0) && enemy->apply_damage(damage))
        _state = RoundState::Victory;
}

void GameScene::on_bullet_finished(game::objects::BulletEndReason reason)
{
    (void)reason;
    if (_active_bullet)
        _resolution_focus = _active_bullet->world_rect();
    _active_bullet = nullptr;
    if (_state == RoundState::Flight)
    {
        _state = RoundState::Resolving;
        _impact_hold_timer.restart();
    }
}
}
