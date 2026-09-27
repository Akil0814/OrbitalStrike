#include "game_scene.h"

#include "../level/game_level_catalog.h"
#include "../gameplay/fleet/enemy_ship.h"
#include "../gameplay/launcher/aim_guide.h"
#include "../gameplay/launcher/moon_cell.h"
#include "../gameplay/projectile/projectile.h"

#include "engine/camera/camera_manager.h"
#include "engine/camera/follow_strategy.h"
#include "engine/tools/debug_draw.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>

namespace game::scene
{
GameScene::GameScene()
    : Scene(elysia::physics::PhysicsWorldConfig{.gravity = {}}),
      _projectile_factory(*this)
{
    _impact_hold_timer.set_one_shot(true);
    _impact_hold_timer.set_wait_time(1.0);
    _impact_hold_timer.set_on_timeout([this] { finish_resolution(); });
    _impact_hold_timer.pause();
}

void GameScene::on_enter(const elysia::scene::ScenePayload& payload)
{
    const auto* game_payload = elysia::scene::try_scene_payload<game::level::GameScenePayload>(payload);
    if (!game_payload) throw std::logic_error("GameScene requires GameScenePayload.");
    if (_level.is_built()) clear_level();
    _level_id = game_payload->level_id;
    build_level();

    ELYSIA_DEBUG_DRAW->set_enabled(true);
    ELYSIA_DEBUG_DRAW->set_enabled_categories(elysia::tools::DebugDrawCategory::All);
}

void GameScene::on_exit() { clear_level(); }
void GameScene::reset() { clear_level(); }

void GameScene::on_update(double delta)
{
    if (!_level.is_built())
    {
        Scene::on_update(delta);
        return;
    }

    const auto& definition = *_level.definition();
    const float frame_delta = static_cast<float>(std::max(0.0, delta));
    if (std::fabs(_input.continuous_zoom()) > elysia::core::Vector2::k_epsilon)
    {
        const float requested = camera().zoom() * std::exp(_input.continuous_zoom() * frame_delta);
        elysia::camera::CameraManager::instance()->set_zoom(
            render_camera_slot(),
            std::clamp(requested, definition.camera.minimum_zoom, definition.camera.maximum_zoom));
    }

    auto* moon_cell = _level.moon_cell();
    if (_round.is_aiming() && moon_cell)
    {
        _power = std::clamp(
            _power + _input.power_adjustment() * definition.launch.adjustment_rate * frame_delta,
            definition.launch.minimum_power, definition.launch.maximum_power);

        auto pan = _input.camera_pan();
        if (pan.length_squared() > 1.0f) pan = pan.normalized();
        const float zoom = std::max(camera().zoom(), elysia::core::Vector2::k_epsilon);
        _camera_pan_offset += pan * (definition.camera.pan_speed / zoom * frame_delta);
        _camera_pan_offset.y = std::min(0.0f, _camera_pan_offset.y);

        const auto base = moon_cell->camera_anchor()
            - elysia::core::Vector2{0.0f,
                camera().world_viewport_size().y * definition.camera.cannon_screen_offset_ratio};
        const auto& bounds = definition.camera.aiming_bounds;
        const elysia::core::Vector2 target{
            std::clamp(base.x + _camera_pan_offset.x, bounds.left(), bounds.right()),
            std::clamp(base.y + _camera_pan_offset.y, bounds.top(), base.y)};
        _camera_pan_offset = target - base;

        std::optional<elysia::core::Vector2> mouse_world;
        if (const auto mouse_screen = _input.mouse_screen_position())
            mouse_world = camera().screen_to_world(*mouse_screen);
        _input.update_aim(delta, moon_cell->cannon_pivot(), mouse_world);
        moon_cell->set_aim_direction(_input.aim_direction());
        if (_aim_guide)
            _aim_guide->set_aim(
                moon_cell->muzzle_position(), moon_cell->aim_direction(), _power, true);
    }
    else if (_aim_guide)
    {
        _aim_guide->set_aim({}, {}, 0.0f, false);
    }

    update_hud();
    _impact_hold_timer.update(delta);
    Scene::on_update(delta);
    _level.set_backdrop_visible_bounds(camera().view_rect());
}

void GameScene::on_routed_input(const elysia::input::InputSnapshot& input)
{
    const auto commands = _input.route(input);
    if (commands.focus_lost)
    {
        _camera_pan_offset = {};
        return;
    }

    const auto* definition = _level.definition();
    if (!definition) return;

    if (_round.is_aiming())
    {
        if (auto* moon_cell = _level.moon_cell())
        {
            std::optional<elysia::core::Vector2> mouse_world;
            if (const auto mouse_screen = _input.mouse_screen_position())
                mouse_world = camera().screen_to_world(*mouse_screen);
            _input.update_aim(0.0, moon_cell->cannon_pivot(), mouse_world);
            moon_cell->set_aim_direction(_input.aim_direction());
        }
    }

    if (std::fabs(commands.zoom_wheel_steps) > elysia::core::Vector2::k_epsilon)
    {
        const float requested = camera().zoom() * std::pow(1.1f, commands.zoom_wheel_steps);
        elysia::camera::CameraManager::instance()->set_zoom(
            render_camera_slot(),
            std::clamp(requested,
                       definition->camera.minimum_zoom,
                       definition->camera.maximum_zoom));
    }

    if (_round.is_aiming() && commands.fire_pressed) launch_projectile();
    else if (_round.is_victorious() && commands.restart_pressed) restart_level();
}

void GameScene::on_fixed_update(std::uint64_t tick, double delta)
{
    (void)tick;
    (void)delta;
    if (!_round.is_in_flight() || !_active_projectile || _active_projectile->finished()) return;
    const game::projectile::ProjectileState projectile{
        .position = _active_projectile->center(), .velocity = _active_projectile->velocity()};
    elysia::core::Vector2 total_force{};
    for (const auto* source : _level.projectile_force_sources())
        if (source) total_force += source->force_on(projectile);
    if (!total_force.is_zero())
        (void)physics_world().apply_force(_active_projectile->physics_handle(), total_force);
}

void GameScene::on_scene_object_registered(elysia::core::SceneObject& object)
{
    if (auto* projectile = dynamic_cast<game::projectile::Projectile*>(&object))
        projectile->attach_collision_listener();
}

std::optional<elysia::camera::CameraFocus> GameScene::resolve_camera_focus() const
{
    if (_round.is_in_flight() && _active_projectile && !_active_projectile->is_destroyed())
        return elysia::camera::CameraFocus{
            _active_projectile->world_rect(), _active_projectile->world_rect()};
    if ((_round.is_resolving() || _round.is_victorious()) && _resolution_focus)
        return elysia::camera::CameraFocus{*_resolution_focus, *_resolution_focus};
    if (_level.moon_cell() && !_level.moon_cell()->is_destroyed())
    {
        const auto focus = elysia::core::Rect::from_center(aiming_camera_target(), {2.0f, 2.0f});
        return elysia::camera::CameraFocus{focus, focus};
    }
    return std::nullopt;
}

elysia::core::Vector2 GameScene::aiming_camera_target() const noexcept
{
    const auto* moon_cell = _level.moon_cell();
    const auto* definition = _level.definition();
    if (!moon_cell || !definition) return {};
    return moon_cell->camera_anchor()
        - elysia::core::Vector2{0.0f,
            camera().world_viewport_size().y * definition->camera.cannon_screen_offset_ratio}
        + _camera_pan_offset;
}

void GameScene::build_level()
{
    if (!_level_id) throw std::logic_error("Cannot build a game level without a selected level id.");
    const auto& definition = game::level::GameLevelCatalog::get(*_level_id);
    try
    {
        _level.build(*this, definition);
        _aim_guide = create_and_add_object<game::launcher::AimGuide>();
        if (!_aim_guide) throw std::runtime_error("GameScene failed to create AimGuide.");
        _hud.build(*this);

        auto* flagship = _level.fleet().flagship();
        auto* moon_cell = _level.moon_cell();
        if (!flagship || !moon_cell) throw std::runtime_error("GameLevel has no MoonCell or flagship.");
        const auto initial_aim = moon_cell->cannon_pivot().direction_to(flagship->center());
        _input.reset(initial_aim);
        moon_cell->set_aim_direction(initial_aim);
        _camera_pan_offset = {};
        _power = definition.launch.initial_power;
        _round.reset();
        _impact_hold_timer.pause();

        auto* cameras = elysia::camera::CameraManager::instance();
        cameras->set_follow_strategy(
            render_camera_slot(), std::make_unique<elysia::camera::SmoothFollowStrategy>(1800.0));
        cameras->set_world_bounds(render_camera_slot(), definition.map.activity_bounds);
        cameras->set_zoom(render_camera_slot(), definition.camera.initial_zoom);
        _level.set_backdrop_visible_bounds(camera().view_rect());
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
    if (_active_projectile)
    {
        _active_projectile->detach_collision_listener();
        if (!_active_projectile->is_destroyed()) _active_projectile->destroy();
    }
    if (_aim_guide && !_aim_guide->is_destroyed()) _aim_guide->destroy();
    _active_projectile = nullptr;
    _aim_guide = nullptr;
    _hud.clear();
    _level.clear();
    physics_world().reset();
    _camera_pan_offset = {};
    _resolution_focus.reset();
    _input.reset();
    _impact_hold_timer.pause();
    _round.reset();
    auto* cameras = elysia::camera::CameraManager::instance();
    cameras->set_world_bounds(render_camera_slot(), std::nullopt);
    cameras->request_clear_effects(render_camera_slot());
}

void GameScene::restart_level()
{
    clear_level();
    build_level();
}

void GameScene::finish_resolution()
{
    if (!_round.finish_resolution()) return;
    _resolution_focus.reset();
    const auto* definition = _level.definition();
    const float target_zoom = definition ? definition->camera.initial_zoom : 0.6f;
    elysia::camera::CameraManager::instance()->request_zoom_to(
        render_camera_slot(), target_zoom, 0.35);
}

void GameScene::update_hud()
{
    const auto* flagship = _level.fleet().flagship();
    _hud.update({
        .victory = _round.is_victorious(),
        .projectile_in_flight = _round.is_in_flight(),
        .resolving = _round.is_resolving(),
        .input_device = _input.last_input_device(),
        .power = _power,
        .flagship_hit_points = flagship ? flagship->hit_points() : 0,
        .flagship_maximum_hit_points = flagship ? flagship->maximum_hit_points() : 0,
        .flagship_shield_active = _level.fleet().flagship_shield_active(),
        .living_escorts = _level.fleet().living_escort_count()});
}

void GameScene::launch_projectile()
{
    auto* moon_cell = _level.moon_cell();
    const auto* definition = _level.definition();
    if (!moon_cell || !definition || _active_projectile) return;
    const auto direction = moon_cell->aim_direction().normalized();
    if (direction.is_zero()) return;

    _resolution_focus.reset();
    _active_projectile = _projectile_factory.spawn({
        .position = moon_cell->muzzle_position() + direction * 10.0f,
        .velocity = direction * _power,
        .despawn_bounds = definition->map.projectile_bounds,
        .definition = definition->launch.projectile,
        .on_impact = [this](const game::projectile::ProjectileImpact& impact) {
            return on_projectile_impact(impact);
        },
        .on_finished = [this](game::projectile::ProjectileEndReason reason) {
            on_projectile_finished(reason);
        }});
    if (_active_projectile)
    {
        (void)_round.begin_projectile_flight();
        _camera_pan_offset = {};
    }
}

game::projectile::ProjectileImpactResolution GameScene::on_projectile_impact(
    const game::projectile::ProjectileImpact& impact)
{
    const auto result = _level.resolve_projectile_impact(impact);
    _round.record_impact(_level.fleet().flagship_defeated());
    return result;
}

void GameScene::on_projectile_finished(game::projectile::ProjectileEndReason reason)
{
    if (_active_projectile && reason == game::projectile::ProjectileEndReason::Hit)
        _resolution_focus = _active_projectile->world_rect();
    _active_projectile = nullptr;
    const auto action = _round.finish_projectile(reason);
    if (action == game::session::ProjectileCompletionAction::BeginResolution)
    {
        _impact_hold_timer.restart();
    }
    else if (action == game::session::ProjectileCompletionAction::ReturnToAiming)
    {
        _resolution_focus.reset();
        _impact_hold_timer.pause();
        const auto* definition = _level.definition();
        const float target_zoom = definition ? definition->camera.initial_zoom : 0.6f;
        elysia::camera::CameraManager::instance()->request_zoom_to(
            render_camera_slot(), target_zoom, 0.35);
    }
}
}
