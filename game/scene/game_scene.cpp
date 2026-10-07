#include "game_scene.h"

#include "../level/game_level_catalog.h"
#include "../gameplay/fleet/enemy_ship.h"
#include "../gameplay/fleet/flagship_laser.h"
#include "../gameplay/launcher/aim_guide.h"
#include "../gameplay/launcher/moon_cell.h"
#include "../gameplay/projectile/projectile.h"

#include "engine/camera/follow_strategy.h"
#include "engine/tools/debug_draw.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>

namespace game::scene
{
GameScene::GameScene()
    : Scene(elysia::scene::SceneRuntimeFeatures{
          .fixed_step = elysia::scene::FixedStepConfig{},
          .physics = elysia::physics::PhysicsWorldConfig{.gravity = {}},
          .camera = elysia::scene::CameraSceneConfig{
              .owned_slots = elysia::camera::CameraSlot::Main | elysia::camera::CameraSlot::Cinematic,
              .focus_mode = elysia::scene::CameraFocusMode::ResolveEachFrame}}),
      _projectile_factory(*this)
{
    _impact_hold_timer.set_one_shot(true);
    _impact_hold_timer.set_wait_time(1.0);
    _impact_hold_timer.set_on_timeout([this] { finish_resolution(); });
    _impact_hold_timer.pause();
    _flagship_weapon_timer.set_one_shot(true);
    _flagship_weapon_timer.set_on_timeout([this] {
        if (_round.is_flagship_firing()) finish_flagship_firing();
    });
    _flagship_weapon_timer.pause();
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
void GameScene::on_reset() { clear_level(); }

void GameScene::on_before_update(double delta)
{
    if (!_level.is_built() || is_paused()) return;

    const auto& definition = *_level.definition();
    const float frame_delta = static_cast<float>(std::max(0.0, delta));
    _input.update_camera_pan(delta);
    if (_round.is_aiming()
        && std::fabs(_input.continuous_zoom()) > elysia::core::Vector2::kEpsilon)
    {
        const float requested = camera().zoom() * std::exp(_input.continuous_zoom() * frame_delta);
        camera_runtime().set_zoom(
            elysia::camera::CameraSlot::Main,
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
        const float zoom = std::max(camera().zoom(), elysia::core::Vector2::kEpsilon);
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
    _flagship_weapon_timer.update(delta);
}

void GameScene::on_after_update(double delta)
{
    (void)delta;
    if (_level.is_built()) _level.set_backdrop_visible_bounds(camera().view_rect());
}

void GameScene::on_routed_input(const elysia::input::InputSnapshot& input)
{
    const auto commands = _input.route(input);
    if (commands.focus_lost)
    {
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

    if (_round.is_aiming()
        && std::fabs(commands.zoom_wheel_steps) > elysia::core::Vector2::kEpsilon)
    {
        const float requested = camera().zoom() * std::pow(1.1f, commands.zoom_wheel_steps);
        camera_runtime().set_zoom(
            elysia::camera::CameraSlot::Main,
            std::clamp(requested,
                       definition->camera.minimum_zoom,
                       definition->camera.maximum_zoom));
    }

    if (_round.is_aiming() && commands.fire_pressed) launch_projectile();
    else if (_round.is_terminal() && commands.restart_pressed) restart_level();
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

void GameScene::on_scene_object_removing(elysia::core::SceneObject& object)
{
    if (auto* projectile = dynamic_cast<game::projectile::Projectile*>(&object))
        projectile->detach_collision_listener();
    if (&object == _active_projectile) _active_projectile = nullptr;
    if (&object == _aim_guide) _aim_guide = nullptr;
    if (&object == _flagship_laser) _flagship_laser = nullptr;
}

std::optional<elysia::camera::CameraFocus> GameScene::resolve_camera_focus(
    elysia::camera::CameraSlot slot) const
{
    if (slot == elysia::camera::CameraSlot::Cinematic)
    {
        if (_round.is_in_flight() && _active_projectile && !_active_projectile->is_destroyed())
        {
            const auto focus = _active_projectile->render_rect();
            return elysia::camera::CameraFocus{focus, focus};
        }
        return std::nullopt;
    }
    if (slot == elysia::camera::CameraSlot::Main
        && _level.moon_cell() && !_level.moon_cell()->is_destroyed())
    {
        const auto& observation = camera_runtime().slot_camera(slot);
        const auto focus = elysia::core::Rect::from_center(
            _round.is_aiming() ? aiming_camera_target(observation.zoom()) : observation.center(),
            {2.0f, 2.0f});
        return elysia::camera::CameraFocus{focus, focus};
    }
    return std::nullopt;
}

elysia::core::Vector2 GameScene::aiming_camera_target(float zoom) const noexcept
{
    const auto* moon_cell = _level.moon_cell();
    const auto* definition = _level.definition();
    if (!moon_cell || !definition) return {};
    return moon_cell->camera_anchor()
        - elysia::core::Vector2{0.0f,
            camera_runtime().slot_camera(elysia::camera::CameraSlot::Main).viewport_size().y
                / zoom * definition->camera.cannon_screen_offset_ratio}
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
        _hud.build(*this, {
            static_cast<float>(runtime_context().logical_width()),
            static_cast<float>(runtime_context().logical_height())});

        auto* flagship = _level.fleet().flagship();
        auto* moon_cell = _level.moon_cell();
        if (!flagship || !moon_cell) throw std::runtime_error("GameLevel has no MoonCell or flagship.");
        const auto initial_aim = moon_cell->cannon_pivot().direction_to(flagship->center());
        _input.reset(initial_aim);
        moon_cell->set_aim_direction(initial_aim);
        _camera_pan_offset = {};
        _power = definition.launch.initial_power;
        _round.configure(definition.mission.maximum_rounds);
        _impact_hold_timer.pause();
        _flagship_weapon_timer.pause();

        auto& cameras = camera_runtime();
        cameras.set_follow_strategy(
            elysia::camera::CameraSlot::Main,
            std::make_unique<elysia::camera::SmoothFollowStrategy>(1800.0));
        cameras.set_world_bounds(elysia::camera::CameraSlot::Main, definition.map.activity_bounds);
        cameras.set_zoom(elysia::camera::CameraSlot::Main, definition.camera.initial_zoom);
        cameras.set_follow_strategy(
            elysia::camera::CameraSlot::Cinematic,
            std::make_unique<elysia::camera::HardFollowStrategy>());
        cameras.set_world_bounds(elysia::camera::CameraSlot::Cinematic, std::nullopt);
        cameras.cut_to(elysia::camera::CameraSlot::Main);
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
    if (_flagship_laser && !_flagship_laser->is_destroyed()) _flagship_laser->destroy();
    _active_projectile = nullptr;
    _aim_guide = nullptr;
    _flagship_laser = nullptr;
    _hud.clear();
    _level.clear();
    physics_world().reset();
    _camera_pan_offset = {};
    _flagship_blend.reset();
    _flagship_pullback.reset();
    _input.reset();
    _impact_hold_timer.pause();
    _flagship_weapon_timer.pause();
    _round.reset();
    camera_runtime().reset();
}

void GameScene::restart_level()
{
    clear_level();
    build_level();
}

void GameScene::finish_resolution()
{
    const auto action = _round.finish_resolution();
    if (action == game::session::ProjectileCompletionAction::None) return;
    handle_round_completion(action);
}

void GameScene::handle_round_completion(game::session::ProjectileCompletionAction action)
{
    if (action == game::session::ProjectileCompletionAction::BeginFlagshipWarning)
    {
        begin_flagship_warning();
    }
}

void GameScene::begin_flagship_warning()
{
    auto* flagship = _level.fleet().flagship();
    auto* moon_cell = _level.moon_cell();
    const auto* definition = _level.definition();
    if (!flagship || !moon_cell || !definition) return;
    if (_flagship_laser && !_flagship_laser->is_destroyed()) _flagship_laser->destroy();
    _flagship_laser = create_and_add_object<game::fleet::FlagshipLaser>(
        flagship->center(), moon_cell->camera_anchor());
    auto& cameras = camera_runtime();
    cameras.set_focus(elysia::camera::CameraSlot::Cinematic, std::nullopt);
    cameras.set_center(elysia::camera::CameraSlot::Cinematic, flagship->render_rect().center());
    cameras.set_zoom(elysia::camera::CameraSlot::Cinematic, definition->camera.flagship_close_zoom);
    _flagship_blend = cameras.blend_to(
        elysia::camera::CameraSlot::Cinematic,
        {.duration_seconds = definition->camera.flagship_blend_seconds});
    if (!_flagship_blend) begin_flagship_pullback();
}

void GameScene::on_camera_blend_completed(
    elysia::camera::CameraBlendId id, elysia::camera::CameraSlot slot)
{
    if (slot != elysia::camera::CameraSlot::Cinematic
        || !_flagship_blend || *_flagship_blend != id || !_round.is_flagship_warning()) return;
    _flagship_blend.reset();
    begin_flagship_pullback();
}

void GameScene::begin_flagship_pullback()
{
    const auto* flagship = _level.fleet().flagship();
    const auto* moon_cell = _level.moon_cell();
    const auto* definition = _level.definition();
    if (!flagship || !moon_cell || !definition || !_round.is_flagship_warning()) return;

    const auto endpoints = elysia::core::Rect::from_points(
        flagship->render_rect().center(), moon_cell->camera_anchor());
    const float padding = definition->camera.flagship_framing_padding;
    const auto framing = elysia::core::Rect{
        endpoints.x() - padding, endpoints.y() - padding,
        endpoints.width() + 2.0f * padding, endpoints.height() + 2.0f * padding};
    const auto viewport = camera_runtime().slot_camera(
        elysia::camera::CameraSlot::Cinematic).viewport_size();
    const float zoom = std::min(viewport.x / framing.width(), viewport.y / framing.height());
    _flagship_pullback = camera_runtime().move_to(
        elysia::camera::CameraSlot::Cinematic,
        {.center = framing.center(), .zoom = zoom},
        definition->mission.flagship_warning_seconds,
        elysia::camera::CameraEasing::EaseInOutCubic,
        elysia::camera::CameraMotionEndBehavior::Hold);
}

void GameScene::on_camera_motion_completed(
    elysia::camera::CameraMotionId id, elysia::camera::CameraSlot slot)
{
    if (slot != elysia::camera::CameraSlot::Cinematic
        || !_flagship_pullback || *_flagship_pullback != id || !_round.is_flagship_warning()) return;
    _flagship_pullback.reset();
    begin_flagship_firing();
}

void GameScene::begin_flagship_firing()
{
    const auto* definition = _level.definition();
    if (!_round.begin_flagship_firing() || !definition) return;
    if (_flagship_laser)
        _flagship_laser->set_phase(game::fleet::FlagshipLaserPhase::Firing);
    _flagship_weapon_timer.set_wait_time(definition->mission.flagship_firing_seconds);
    _flagship_weapon_timer.restart();
}

void GameScene::finish_flagship_firing()
{
    if (!_round.finish_flagship_firing()) return;
    _flagship_weapon_timer.pause();
    update_hud();
}

void GameScene::update_hud()
{
    const auto* flagship = _level.fleet().flagship();
    const auto* definition = _level.definition();
    _hud.update({
        .victory = _round.is_victorious(),
        .defeat = _round.is_defeated(),
        .flagship_warning = _round.is_flagship_warning(),
        .flagship_firing = _round.is_flagship_firing(),
        .projectile_in_flight = _round.is_in_flight(),
        .resolving = _round.is_resolving(),
        .input_device = _input.last_input_device(),
        .power = _power,
        .minimum_power = definition ? definition->launch.minimum_power : 0.0f,
        .maximum_power = definition ? definition->launch.maximum_power : 1.0f,
        .flagship_hit_points = flagship ? flagship->hit_points() : 0,
        .flagship_maximum_hit_points = flagship ? flagship->maximum_hit_points() : 0,
        .flagship_shield_active = _level.fleet().flagship_shield_active(),
        .living_escorts = _level.fleet().living_escort_count(),
        .completed_rounds = _round.completed_rounds(),
        .maximum_rounds = _round.maximum_rounds()});
}

void GameScene::launch_projectile()
{
    auto* moon_cell = _level.moon_cell();
    const auto* definition = _level.definition();
    if (!moon_cell || !definition || _active_projectile) return;
    const auto direction = moon_cell->aim_direction().normalized();
    if (direction.is_zero()) return;

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
        auto& cameras = camera_runtime();
        cameras.set_zoom(elysia::camera::CameraSlot::Cinematic,
            cameras.slot_camera(elysia::camera::CameraSlot::Main).zoom());
        cameras.set_center(elysia::camera::CameraSlot::Cinematic,
            _active_projectile->render_rect().center());
        cameras.cut_to(elysia::camera::CameraSlot::Cinematic);
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
    _active_projectile = nullptr;
    camera_runtime().cut_to(elysia::camera::CameraSlot::Main);
    const auto action = _round.finish_projectile(reason);
    if (action == game::session::ProjectileCompletionAction::BeginResolution)
    {
        _impact_hold_timer.restart();
    }
    else if (action == game::session::ProjectileCompletionAction::ReturnToAiming)
    {
        _impact_hold_timer.pause();
        handle_round_completion(action);
    }
    else if (action == game::session::ProjectileCompletionAction::BeginFlagshipWarning)
    {
        _impact_hold_timer.pause();
        handle_round_completion(action);
    }
}
}
