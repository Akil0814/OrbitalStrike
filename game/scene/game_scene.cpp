#include "game_scene.h"
#include "scene_keys.h"

#include "../level/game_level_catalog.h"
#include "../level/game_map.h"
#include "../gameplay/fleet/enemy_ship.h"
#include "../gameplay/fleet/flagship_laser.h"
#include "../gameplay/launcher/aim_guide.h"
#include "../gameplay/launcher/moon_cell.h"
#include "../gameplay/projectile/projectile.h"

#include "engine/camera/follow_strategy.h"
#include "engine/tools/debug_draw.h"
#include "engine/ui/composites/ui_confirmation_dialog.h"
#include "engine/ui/window/ui_window.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <memory>
#include <numbers>
#include <stdexcept>

namespace game::scene
{
GameScene::GameScene()
    : GameplayScene(elysia::gameplay::GameplaySceneFeatures{
          .fixed_step = elysia::scene::FixedStepConfig{},
          .physics = elysia::physics::PhysicsWorldConfig{.gravity = {}},
          .gameplay_collision = true,
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
    if (!elysia::gameplay::ControllerService::instance()->begin_session())
        throw std::runtime_error("Cannot begin OrbitalStrike control session.");
    try { build_level(); }
    catch (...)
    {
        elysia::gameplay::ControllerService::instance()->end_session();
        throw;
    }
    set_ui_interaction_mode(elysia::scene::UiInteractionMode::Pointer);
    resume();

    ELYSIA_DEBUG_DRAW->set_enabled(true);
    ELYSIA_DEBUG_DRAW->set_enabled_categories(elysia::tools::DebugDrawCategory::All);
}

void GameScene::on_exit()
{
    std::exception_ptr failure;
    try { clear_level(); }
    catch (...) { failure = std::current_exception(); }
    elysia::gameplay::ControllerService::instance()->end_session();
    if (failure) std::rethrow_exception(failure);
}
void GameScene::on_reset() { clear_level(); }

void GameScene::on_before_update(double delta)
{
    resume_after_return_menu_dialog();
    if (!_level.is_built() || is_paused()) return;
    if (_restart_requested) restart_level();
    _impact_hold_timer.update(delta);
    _flagship_weapon_timer.update(delta);
}

void GameScene::on_after_update(double delta)
{
    (void)delta;
    if (!_level.is_built()) return;
    _level.set_backdrop_visible_bounds(camera().view_rect());
    if (_aim_guide)
    {
        const auto* moon = _level.moon_cell();
        _aim_guide->set_aim(moon->muzzle_position(), moon->aim_direction(), _power, _round.is_aiming());
    }
    update_hud();
}

bool GameScene::on_unassigned_input(const elysia::input::RawInputEvent& event)
{
    if (!event.source.is_gamepad()) return false;
    if (!local_players().replace_gamepad(elysia::input::PrimaryLocalPlayer, event.source)) return false;
    set_ui_gamepad(event.source);
    return true;
}

void GameScene::on_shortcuts(const elysia::input::RawInputFrame& input,
                            const std::vector<elysia::input::RawInputEvent>& events)
{
    (void)input;
    resume_after_return_menu_dialog();
    for (const auto& event : events)
        if (event.type == elysia::input::RawInputEventType::ControlPressed
            && (event.control == elysia::input::RawInputControl::KeyEscape
                || event.control == elysia::input::RawInputControl::GamepadEast))
        {
            consume_input(event);
            open_return_menu_dialog();
        }
}

void GameScene::cancel_control(elysia::gameplay::InputCancelReason reason) noexcept
{
    (void)reason;
    _mouse_aim = true;
    _gamepad_aim_active = false;
    _restart_requested = false;
}

void GameScene::handle_control(const elysia::gameplay::ControlCommand& command, double delta)
{
    using namespace elysia::input;
    using namespace game::input::actions;
    if (!_level.is_built() || is_paused()) return;
    if (command.input_device != InputDevice::Unknown) _input_device = command.input_device;
    const auto pressed = [&](const InputActionId& action) {
        return std::ranges::any_of(command.events, [&](const auto& event) {
            return event.action == action && event.phase == ActionInputPhase::Started;
        });
    };
    if (_round.is_terminal())
    {
        if (pressed(Restart)) _restart_requested = true;
        return;
    }
    if (!_round.is_aiming()) return;
    const auto& definition = *_level.definition();
    const float step = static_cast<float>(delta);
    auto& cameras = camera_runtime();
    using elysia::camera::CameraSlot;
    const float zoom_input = command.state.axis1d(ZoomContinuous);
    float zoom = cameras.slot_camera(CameraSlot::Main).zoom() * std::exp(zoom_input * step);
    if (const auto wheel = command.deltas.find(ZoomWheel); wheel != command.deltas.end())
        zoom *= std::pow(1.1f, wheel->second.x);
    cameras.set_zoom(CameraSlot::Main, clamp_observation_zoom(zoom));
    _power = std::clamp(_power + command.state.axis1d(PowerAdjust) * definition.launch.adjustment_rate * step,
        definition.launch.minimum_power, definition.launch.maximum_power);
    auto pan = command.state.axis2d(CameraPan) + command.state.axis2d(CameraPanStick);
    if (pan.length_squared() > 1.0f) pan = pan.normalized();
    const auto& observation = cameras.slot_camera(CameraSlot::Main);
    cameras.set_center(CameraSlot::Main,
        observation.center() + pan * (definition.camera.pan_speed / observation.zoom() * step));

    if (command.pointer_updated)
    {
        _mouse_aim = true;
        _gamepad_aim_active = false;
    }
    const auto stick = command.state.axis2d(AimStick);
    const float magnitude = stick.length();
    if (magnitude > (_gamepad_aim_active ? 0.22f : 0.30f))
    {
        _gamepad_aim_active = true;
        _mouse_aim = false;
        _input_device = InputDevice::Gamepad;
    }
    else if (magnitude <= 0.22f) _gamepad_aim_active = false;
    auto* moon = _level.moon_cell();
    auto direction = moon->aim_direction();
    if (_mouse_aim && command.pointer_position)
    {
        const auto target = observation.screen_to_world(*command.pointer_position);
        const auto aimed = moon->cannon_pivot().direction_to(target);
        if (!aimed.is_zero()) direction = aimed;
    }
    else if (_gamepad_aim_active)
    {
        const float current = std::atan2(direction.y, direction.x);
        const float target = std::atan2(stick.y, stick.x);
        const float difference = std::remainder(target - current, 2.0f * std::numbers::pi_v<float>);
        const float strength = std::pow(std::clamp((magnitude - 0.30f) / 0.70f, 0.0f, 1.0f), 1.35f);
        const float maximum = 2.617993878f * strength * step;
        const float angle = current + std::clamp(difference, -maximum, maximum);
        direction = {std::cos(angle), std::sin(angle)};
    }
    moon->set_aim_direction(direction);
    if (pressed(Fire)) launch_projectile();
}
void GameScene::on_game_fixed_update(std::uint64_t tick, double delta)
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
    using namespace elysia::gameplay::collision;
    const auto team = dynamic_cast<game::fleet::EnemyShip*>(&object) ? teams::Enemy
        : dynamic_cast<game::launcher::MoonCell*>(&object) || dynamic_cast<game::projectile::Projectile*>(&object)
            ? teams::Player : teams::Neutral;
    for (auto collider : registered_physics_colliders(object))
        if (!collision_runtime().bind_collider({collider, collider, team, ColliderRole::Body}))
            throw std::runtime_error("Cannot bind gameplay collision body.");
    if (auto* projectile = dynamic_cast<game::projectile::Projectile*>(&object))
        if (!collision_runtime().add_listener(*projectile))
            throw std::runtime_error("Cannot register projectile gameplay collision listener.");
}

void GameScene::on_control_target_removing(elysia::core::SceneObject& object)
{
    if (auto* projectile = dynamic_cast<game::projectile::Projectile*>(&object))
        (void)collision_runtime().remove_listener(*projectile);
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
    return std::nullopt;
}

float GameScene::clamp_observation_zoom(float zoom) const noexcept
{
    const auto& config = _level.definition()->camera;
    const float minimum = std::max(config.minimum_zoom,
        camera_runtime().slot_camera(elysia::camera::CameraSlot::Main).viewport_size().y
            / _camera_bounds.height());
    return std::clamp(zoom, minimum, std::max(minimum, config.maximum_zoom));
}

void GameScene::build_level()
{
    if (!_level_id) 
        throw std::logic_error("Cannot build a game level without a selected level id.");

    const auto& definition = game::level::GameLevelCatalog::get(*_level_id);
    try
    {
        _level.build(*this, definition);
        _aim_guide = create_and_add_object<game::launcher::AimGuide>();
        if (!_aim_guide)
            throw std::runtime_error("GameScene failed to create AimGuide.");
        _hud.build(*this, {
            static_cast<float>(runtime_context().logical_width()),
            static_cast<float>(runtime_context().logical_height())});
        build_return_menu_dialog();

        auto* flagship = _level.fleet().flagship();
        auto* moon_cell = _level.moon_cell();
        if (!flagship || !moon_cell) throw std::runtime_error("GameLevel has no MoonCell or flagship.");
        const auto initial_aim = moon_cell->cannon_pivot().direction_to(flagship->center());
        moon_cell->set_aim_direction(initial_aim);
        cancel_control(elysia::gameplay::InputCancelReason::Unbound);
        _input_device = elysia::input::InputDevice::Keyboard;
        moon_cell->set_control_handlers(
            [this](const auto& command, double delta) { handle_control(command, delta); },
            [this](auto reason) { cancel_control(reason); });
        auto* controls = elysia::gameplay::ControllerService::instance();
        const auto created = controls->create<elysia::gameplay::LocalPlayerController>(
            {.scope = elysia::gameplay::ControllerScope::Scene, .scene = control_context().token()},
            elysia::input::PrimaryLocalPlayer, game::input::make_game_actions());
        if (!created) throw std::runtime_error("Cannot create OrbitalStrike player controller.");
        _controller = *created;
        if (!controls->bind_target(_controller, control_context(), *moon_cell).succeeded())
            throw std::runtime_error("Cannot bind OrbitalStrike player controller.");
        set_ui_gamepad(local_players().configuration().bindings.at(elysia::input::PrimaryLocalPlayer).gamepad);
        _power = definition.launch.initial_power;
        _round.configure(definition.mission.maximum_rounds);
        _impact_hold_timer.pause();
        _flagship_weapon_timer.pause();

        auto& cameras = camera_runtime();
        // Keep the viewport above the cut-off bottom of the surface artwork.
        const auto& moon = definition.moon_cell;
        const float surface_bottom = moon.cannon_pivot.y
            + moon.base_visual_size.y * (1.0f - moon.base_visual_anchor.y);
        _camera_bounds = game::level::map::kBounds;
        _camera_bounds.set_height(std::max(1.0f,
            std::min(_camera_bounds.bottom(), surface_bottom - 2.0f) - _camera_bounds.top()));
        cameras.set_world_bounds(elysia::camera::CameraSlot::Main, _camera_bounds);
        cameras.set_zoom(elysia::camera::CameraSlot::Main, clamp_observation_zoom(definition.camera.initial_zoom));
        cameras.set_center(elysia::camera::CameraSlot::Main,
            moon_cell->camera_anchor() - elysia::core::Vector2{0.0f,
                cameras.slot_camera(elysia::camera::CameraSlot::Main).world_viewport_size().y
                    * definition.camera.cannon_screen_offset_ratio});
        cameras.set_follow_strategy(
            elysia::camera::CameraSlot::Cinematic,
            std::make_unique<elysia::camera::HardFollowStrategy>());
        cameras.set_world_bounds(elysia::camera::CameraSlot::Cinematic, _camera_bounds);
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

void GameScene::clear_level()
{
    if (_controller)
        (void)elysia::gameplay::ControllerService::instance()->remove(_controller);
    _controller = {};
    if (_return_menu_window && !_return_menu_window->is_destroyed())
        _return_menu_window->destroy();
    _return_menu_window = nullptr;
    _return_menu_dialog = nullptr;
    _return_menu_paused_scene = false;
    if (_active_projectile)
    {
        if (!_active_projectile->is_destroyed()) _active_projectile->destroy();
    }
    if (_aim_guide && !_aim_guide->is_destroyed()) _aim_guide->destroy();
    if (_flagship_laser && !_flagship_laser->is_destroyed()) _flagship_laser->destroy();
    _active_projectile = nullptr;
    _aim_guide = nullptr;
    _flagship_laser = nullptr;
    _hud.clear();
    _level.clear();
    clear_scene_objects();
    collision_runtime().clear();
    _camera_bounds = {};
    _flagship_blend.reset();
    _flagship_pullback.reset();
    cancel_control(elysia::gameplay::InputCancelReason::Unbound);
    _restart_requested = false;
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

void GameScene::build_return_menu_dialog()
{
    _return_menu_window = create_and_add_object<elysia::ui::UiWindow>(
        elysia::core::Rect{0.0f, 0.0f,
            static_cast<float>(runtime_context().logical_width()),
            static_cast<float>(runtime_context().logical_height())}, 200);
    if (!_return_menu_window) throw std::runtime_error("Cannot create return-menu window.");
    _return_menu_window->set_style_overrides({.draw_background = false, .draw_border = false});
    _return_menu_dialog = _return_menu_window->create_child<elysia::ui::UiConfirmationDialog>(
        elysia::core::Rect{0.0f, 0.0f, 480.0f, 260.0f}, 10);
    if (!_return_menu_dialog) throw std::runtime_error("Cannot create return-menu dialog.");
    _return_menu_dialog->set_config({
        .title = elysia::ui::ui_text_key("game_scene.return_menu.title"),
        .message = elysia::ui::ui_text_key("game_scene.return_menu.message"),
        .confirm = elysia::ui::ui_text_key("game_scene.return_menu.confirm"),
        .cancel = elysia::ui::ui_text_key("common.cancel"),
        .close = elysia::ui::ui_text_key("common.close")});
    _return_menu_dialog->set_on_confirm([this] {
        // Keep gameplay paused until the pending scene switch retires this level.
        _return_menu_paused_scene = false;
        request_scene_switch(game::scene_keys::MainMenu);
    });
    if (!_return_menu_dialog->register_with_window(*_return_menu_window))
        throw std::runtime_error("Cannot register return-menu dialog.");
    _return_menu_window->set_on_cancel([this] { open_return_menu_dialog(); });
}

void GameScene::open_return_menu_dialog()
{
    if (!_level.is_built() || !_return_menu_window || !_return_menu_dialog
        || _return_menu_window->is_overlay_open(*_return_menu_dialog)) return;
    _return_menu_paused_scene = !is_paused();
    set_ui_interaction_mode(elysia::scene::UiInteractionMode::Navigation);
    const auto binding = local_players().configuration().bindings.find(elysia::input::PrimaryLocalPlayer);
    if (binding != local_players().configuration().bindings.end() && binding->second.gamepad.value)
        set_ui_gamepad(binding->second.gamepad);
    pause();
    _return_menu_dialog->open();
}

void GameScene::resume_after_return_menu_dialog()
{
    if (_return_menu_window && _return_menu_dialog
        && !_return_menu_window->is_overlay_open(*_return_menu_dialog))
    {
        set_ui_interaction_mode(elysia::scene::UiInteractionMode::Pointer);
        if (_return_menu_paused_scene)
        {
            _return_menu_paused_scene = false;
            resume();
        }
    }
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
    cameras.set_zoom(elysia::camera::CameraSlot::Cinematic, definition->camera.flagship_close_zoom);
    cameras.set_center(elysia::camera::CameraSlot::Cinematic, flagship->render_rect().center());
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
    const float zoom = std::max(viewport.y / _camera_bounds.height(),
        std::min(viewport.x / framing.width(), viewport.y / framing.height()));
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
        .input_device = _input_device,
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
