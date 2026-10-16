#include "game/scene/game_scene.h"
#include "game/scene/level_select_scene.h"
#include "game/scene/main_menu_scene.h"
#include "game/scene/scene_keys.h"
#include "game/gameplay/projectile/projectile.h"
#include "game/gameplay/anomaly/black_hole_anomaly.h"
#include "game/gameplay/anomaly/wormhole_portal.h"
#include "game/gameplay/fleet/flagship_laser.h"
#include "game/gameplay/fleet/enemy_ship.h"
#include "game/level/game_level_catalog.h"
#include "game/level/game_map.h"
#include "game/presentation/backdrop/arena_backdrop.h"
#include "game/gameplay/launcher/moon_cell.h"
#include "game/resources/game_texture_keys.h"

#include "engine/camera/camera_manager.h"
#include "engine/gameplay/collision/gameplay_collision_service.h"
#include "engine/core/render/render_command.h"
#include "engine/core/render/render_command_projection.h"
#include "engine/core/render/sdl_render_command_executor.h"
#include "engine/io/loaders/asset_config_types.h"
#include "engine/resources/runtime/resource_manager.h"
#include "engine/scene/scene_manager.h"
#include "engine/ui/composites/ui_confirmation_dialog.h"
#include "engine/ui/window/ui_window.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>
#include <string_view>

#include <SDL3_image/SDL_image.h>

namespace elysia::scene
{
class SceneTestAccess
{
public:
    static void enter(Scene& scene, const SceneRuntimeContext& context, const ScenePayload& payload)
    {
        _runtime = std::make_unique<SceneManager>();
        _runtime->initialize(context);
        scene.bind_runtime_context(context);
        (void)scene.local_players().replace_gamepad(elysia::input::PrimaryLocalPlayer,
            elysia::input::InputSourceId::gamepad(1));
        scene.attach_runtime_services();
        scene.lifecycle_enter(payload);
    }
    static void exit(Scene& scene)
    {
        scene.detach_runtime_services();
        scene.lifecycle_exit();
        if (!_runtime->shutdown()) throw std::runtime_error("Test runtime failed to shut down.");
        _runtime.reset();
    }
    static void reset(Scene& scene) { scene.lifecycle_reset(); }
    static void input(Scene& scene, const elysia::input::InputSnapshot& input)
    {
        scene.lifecycle_input(input);
    }
    static void update(Scene& scene, double delta) { scene.lifecycle_update(delta); }
    static void command(Scene& scene, const elysia::input::InputSnapshot& input)
    {
        scene.lifecycle_input(input);
        scene.lifecycle_update(1.0 / 60.0);
        scene.lifecycle_update(0.0); // A requested restart runs outside controller dispatch.
    }
    static elysia::physics::PhysicsWorld& physics(Scene& scene) { return scene.physics_world(); }
    static SceneCameraRuntime& cameras(Scene& scene) { return scene.camera_runtime(); }
    template <typename T>
    static T* find_ui(Scene& scene)
    {
        const auto visit = [](auto&& self, elysia::ui::UiElement& element) -> T* {
            if (element.is_destroyed()) return nullptr;
            if (auto* result = dynamic_cast<T*>(&element)) return result;
            if (auto* host = dynamic_cast<elysia::ui::UiChildHost*>(&element))
                for (std::size_t index = 0; index < host->child_count(); ++index)
                    if (auto* child = host->child_at(index))
                        if (auto* result = self(self, *child)) return result;
            return nullptr;
        };
        for (const auto& root : scene._ui_roots)
            if (auto* result = visit(visit, *root)) return result;
        return nullptr;
    }
    template <typename T>
    static T* find(Scene& scene)
    {
        for (const auto& layer : scene._object_layers)
            for (const auto& object : layer)
                if (auto* result = dynamic_cast<T*>(object.get()); result && !result->is_destroyed())
                    return result;
        return nullptr;
    }
    template <typename T>
    static std::vector<T*> find_all(Scene& scene)
    {
        std::vector<T*> result;
        for (const auto& layer : scene._object_layers)
            for (const auto& object : layer)
                if (auto* candidate = dynamic_cast<T*>(object.get()); candidate && !candidate->is_destroyed())
                    result.push_back(candidate);
        return result;
    }
private:
    inline static std::unique_ptr<SceneManager> _runtime;
};
}

namespace
{
using Access = elysia::scene::SceneTestAccess;
using namespace elysia::input;
int failures = 0;

void expect(bool condition, std::string_view message)
{
    if (condition) return;
    ++failures;
    std::cerr << "FAILED: " << message << '\n';
}

InputSnapshot idle_input()
{
    InputSnapshot input;
    input.sources = {{.source = InputSourceId::keyboard()}, {.source = InputSourceId::mouse()}};
    return input;
}

InputSnapshot press(RawInputControl control)
{
    auto input = idle_input();
    const bool mouse = is_mouse_button_control(control);
    auto& source = input.sources[mouse ? 1 : 0];
    source.frame.state.set_pressed(control, true);
    input.events.push_back({
        .control = control,
        .type = RawInputEventType::ControlPressed,
        .device = mouse ? InputDevice::Mouse : InputDevice::Keyboard,
        .source = source.source});
    return input;
}

InputSnapshot release(RawInputControl control)
{
    auto input = idle_input();
    const bool mouse = is_mouse_button_control(control);
    auto& source = input.sources[mouse ? 1 : 0];
    source.initial_state.set_pressed(control, true);
    input.events.push_back({.control = control,
        .type = RawInputEventType::ControlReleased,
        .device = mouse ? InputDevice::Mouse : InputDevice::Keyboard,
        .source = source.source});
    return input;
}

void aim_and_fire(game::scene::GameScene& scene, elysia::core::Vector2 target,
                  float error_degrees = 0.0f)
{
    auto* moon_cell = Access::find<game::launcher::MoonCell>(scene);
    if (!moon_cell) throw std::runtime_error("Missing launcher in aiming test.");
    const auto bearing = moon_cell->cannon_pivot().direction_to(target);
    const float angle = std::atan2(bearing.y, bearing.x)
        + error_degrees * std::numbers::pi_v<float> / 180.0f;
    const elysia::core::Vector2 direction{std::cos(angle), std::sin(angle)};
    const auto cursor = scene.camera().world_to_screen(
        moon_cell->cannon_pivot() + direction * 500.0f);
    auto fire = press(RawInputControl::MouseLeft);
    fire.events.front().mouse_x = static_cast<int>(std::lround(cursor.x));
    fire.events.front().mouse_y = static_cast<int>(std::lround(cursor.y));
    Access::input(scene, idle_input());
    Access::command(scene, fire);
}

class RouteCapture final : public elysia::scene::SceneRequestObserver
{
public:
    void on_scene_request(const elysia::scene::SceneRequest& request) override
    {
        last_request = request;
    }
    elysia::scene::SceneRequest last_request{};
};

void test_level_selection(const elysia::scene::SceneRuntimeContext& context)
{
    RouteCapture capture;
    game::scene::LevelSelectScene scene;
    scene.attach(&capture);
    Access::enter(scene, context, {});
    Access::update(scene, 1.0 / 60.0);
    for (const auto& entry : game::level::GameLevelCatalog::entries())
    {
        capture.last_request = {};
        Access::input(scene, press(RawInputControl::KeyEnter));
        Access::input(scene, release(RawInputControl::KeyEnter));
        const auto* payload = elysia::scene::try_scene_payload<game::level::GameScenePayload>(
            capture.last_request.route.payload);
        expect(capture.last_request.type == elysia::scene::SceneRequestType::Switch
                   && capture.last_request.route.target == game::scene_keys::Game
                   && payload && payload->level_id == entry.id,
               "level selection launches the focused level in catalog order");
        Access::input(scene, press(RawInputControl::KeyDown));
        Access::input(scene, release(RawInputControl::KeyDown));
    }
    scene.detach(&capture);
    Access::exit(scene);
}

void test_menu_lifecycle(const elysia::scene::SceneRuntimeContext& context)
{
    elysia::scene::SceneManager manager;
    manager.initialize(context);
    manager.register_game_scene<game::scene::MainMenuScene>(game::scene_keys::MainMenu);
    manager.register_game_scene<game::scene::LevelSelectScene>(game::scene_keys::LevelSelect);
    manager.start({.target = game::scene_keys::MainMenu});
    manager.on_update(1.0 / 60.0);
    manager.on_input(idle_input());
    manager.on_input(press(RawInputControl::KeyEnter));
    auto release = idle_input();
    release.sources.front().initial_state.set_pressed(RawInputControl::KeyEnter, true);
    release.events.push_back({.control = RawInputControl::KeyEnter,
        .type = RawInputEventType::ControlReleased,
        .device = InputDevice::Keyboard, .source = InputSourceId::keyboard()});
    manager.on_input(release);
    expect(manager.current_scene_key() == game::scene_keys::LevelSelect,
           "menu confirmation enters level select through the new scene lifecycle");
    manager.on_update(1.0 / 60.0);
    manager.on_scene_request({elysia::scene::SceneRequestType::Switch,
        {.target = game::scene_keys::MainMenu,
         .reload_mode = elysia::scene::SceneReloadMode::Reset}});
    manager.on_update(1.0 / 60.0);
    expect(manager.current_scene_key() == game::scene_keys::MainMenu
               && manager.state() == elysia::scene::SceneManagerState::Running,
           "menu reset and reentry complete without a scene failure");
    expect(manager.shutdown(), "menu scenes shut down cleanly");
}

void test_game_lifecycle(const elysia::scene::SceneRuntimeContext& context)
{
    game::scene::GameScene scene;
    expect(scene.has_fixed_step() && scene.has_physics() && scene.has_camera(),
           "game scene enables the runtimes required by gameplay");
    const game::level::GameScenePayload payload{.level_id = game::level::GameLevelId::Prototype};
    Access::enter(scene, context, payload);
    Access::update(scene, 1.0 / 60.0);
    auto& physics = Access::physics(scene);
    const auto level_bodies = physics.registered_object_count();
    expect(level_bodies > 0, "entering a level registers its physics bodies");
    const auto& definition = game::level::GameLevelCatalog::get(payload.level_id);
    const auto* moon_cell = Access::find<game::launcher::MoonCell>(scene);
    if (!moon_cell) throw std::runtime_error("Missing launcher in lifecycle test.");
    const auto initial_center = moon_cell->camera_anchor() - elysia::core::Vector2{0.0f,
        scene.camera().world_viewport_size().y * definition.camera.cannon_screen_offset_ratio};
    expect(scene.camera().center().distance_to(initial_center) < 0.001f,
           "the scene camera starts at the MoonCell observation position");
    Access::input(scene, idle_input());

    auto wheel = idle_input();
    wheel.events.push_back({.type = RawInputEventType::MouseWheel,
        .device = InputDevice::Mouse, .wheel_y = 1.0f, .source = InputSourceId::mouse()});
    Access::command(scene, wheel);
    expect(std::fabs(scene.camera().zoom() - 0.66f) < 0.0001f,
           "mouse wheel changes the owned scene camera");
    Access::input(scene, press(RawInputControl::KeyD));
    Access::update(scene, 0.1);
    Access::input(scene, idle_input());
    Access::update(scene, 0.2);
    const auto observation_center = scene.camera().center();
    auto& cameras = Access::cameras(scene);
    using elysia::camera::CameraSlot;

    for (int shot = 0; shot < 2; ++shot)
    {
        Access::input(scene, idle_input());
        Access::command(scene, press(RawInputControl::MouseLeft));
        auto* projectile = Access::find<game::projectile::Projectile>(scene);
        expect(projectile != nullptr, "routed mouse input launches a projectile");
        if (!projectile) break;
        expect(cameras.presented_slot() == CameraSlot::Cinematic
                   && scene.camera().view_rect().contains(projectile->render_rect().center())
                   && scene.camera().view_rect().bottom() <= game::level::map::kBounds.bottom() + 0.001f,
               "launch immediately cuts to the projectile view while respecting the bottom map edge");
        const auto start = projectile->center();
        Access::update(scene, 0.05);
        expect(projectile->center().distance_to(start) > 1.0f,
               "fixed-step physics advances the launched projectile");
        expect(scene.camera().view_rect().contains(projectile->render_rect().center())
                   && scene.camera().view_rect().bottom() <= game::level::map::kBounds.bottom() + 0.001f,
               "the cinematic camera keeps the interpolated projectile visible within map bounds");
        std::vector<elysia::core::RenderCommand> commands;
        projectile->submit_render_commands(commands);
        const auto& sprite = commands.front();
        const elysia::core::Vector2 sprite_anchor{
            sprite.command_rect.x() + sprite.command_rect.width() * sprite.rotation_origin.x,
            sprite.command_rect.y() + sprite.command_rect.height() * sprite.rotation_origin.y};
        expect(sprite_anchor.distance_to(projectile->render_rect().center()) < 0.001f,
               "projectile rendering uses the same presentation position as camera tracking");
        const float flight_zoom = scene.camera().zoom();
        Access::input(scene, wheel);
        expect(scene.camera().zoom() == flight_zoom,
               "observation zoom input cannot alter the flight camera");
        expect(cameras.slot_camera(CameraSlot::Main).center() == observation_center,
               "the observation camera stays at its pre-launch position during flight");
        scene.pause();
        const auto paused_position = projectile->center();
        Access::update(scene, 0.2);
        expect(projectile->center() == paused_position,
               "pause freezes projectile physics");
        scene.resume();

        // Force expiry on the first shot, then a real physics contact on the next.
        // The second contact exercises listener cleanup after the first object retired.
        if (shot == 0)
        {
            expect(physics.teleport_object(projectile->physics_handle(),
                {game::level::map::kBounds.left() - 500.0f, game::level::map::kBounds.center().y}),
                   "the projectile can travel outside the activity bounds");
            Access::update(scene, 1.0 / 60.0);
            expect(std::fabs(scene.camera().view_rect().left() - game::level::map::kBounds.left()) < 0.001f
                       && !scene.camera().view_rect().contains(projectile->render_rect().center()),
                   "the flight camera stops at the map edge while the projectile continues outside");
            Access::update(scene, 21.0);
        }
        else
        {
            const auto* black_hole = Access::find<game::anomaly::BlackHoleAnomaly>(scene);
            if (!black_hole) throw std::runtime_error("Missing black hole in lifecycle test.");
            expect(physics.teleport_object(projectile->physics_handle(),
                black_hole->center() - projectile->world_rect().size() * 0.5f),
                   "the second projectile can be moved into the black-hole event horizon");
            Access::update(scene, 1.0 / 60.0);
        }
        expect(Access::find<game::projectile::Projectile>(scene) == nullptr,
               "finished projectiles retire from the scene");
        expect(cameras.presented_slot() == CameraSlot::Main
                   && scene.camera().center() == observation_center
                   && std::fabs(scene.camera().zoom() - 0.66f) < 0.0001f,
               "expiry and collision immediately restore the saved observation view");
        if (shot == 1)
        {
            Access::input(scene, idle_input());
            Access::command(scene, press(RawInputControl::MouseLeft));
            expect(Access::find<game::projectile::Projectile>(scene) == nullptr,
                   "the one-second hit resolution blocks the next launch after cutting back");
        }
        for (int frame = 0; frame < 90; ++frame) Access::update(scene, 1.0 / 60.0);
        expect(physics.registered_object_count() == level_bodies,
               "projectile retirement preserves level physics");
        expect(std::fabs(scene.camera().zoom() - 0.66f) < 0.0001f
                   && scene.camera().center() == observation_center,
               "round completion retains the player's observation position and zoom");
    }

    Access::exit(scene);
    expect(physics.registered_object_count() == 0, "exit clears level physics");
    Access::reset(scene);
    Access::enter(scene, context, payload);
    Access::update(scene, 1.0 / 60.0);
    expect(physics.registered_object_count() == level_bodies,
           "reset and reentry rebuild the complete level");
    Access::input(scene, idle_input());
    Access::command(scene, press(RawInputControl::MouseLeft));
    expect(Access::find<game::projectile::Projectile>(scene) != nullptr,
           "a rebuilt level accepts a new launch");
    Access::exit(scene);
    expect(physics.registered_object_count() == 0, "exit during flight cleans up the projectile");
}

void exhaust_rounds(game::scene::GameScene& scene,
                    game::level::GameLevelId level_id = game::level::GameLevelId::Prototype)
{
    const auto& definition = game::level::GameLevelCatalog::get(level_id);
    for (int shot = 0; shot < definition.mission.maximum_rounds; ++shot)
    {
        Access::input(scene, idle_input());
        Access::command(scene, press(RawInputControl::MouseLeft));
        auto* projectile = Access::find<game::projectile::Projectile>(scene);
        if (!projectile) throw std::runtime_error("Failed to launch a test round.");
        projectile->update(definition.launch.projectile.lifetime_seconds + 1.0);
        expect(Access::cameras(scene).presented_slot() == elysia::camera::CameraSlot::Main,
               "even the final expired projectile immediately cuts back to Main");
        Access::update(scene, 0.0);
    }
}

void test_control_command_buffering(const elysia::scene::SceneRuntimeContext& context)
{
    using namespace game::input::actions;
    game::scene::GameScene scene;
    Access::enter(scene, context, game::level::GameScenePayload{});
    expect(scene.has_collision_runtime(), "game scene enables the engine gameplay collision runtime");
    auto* moon = Access::find<game::launcher::MoonCell>(scene);
    elysia::gameplay::ControlCommand last;
    int deliveries = 0, cancellations = 0;
    moon->set_control_handlers([&](const auto& command, double delta) {
        last = command;
        ++deliveries;
        expect(delta == scene.fixed_step_config()->delta_seconds,
               "MoonCell receives engine control commands on the scene fixed step");
    }, [&](auto) { ++cancellations; });
    Access::input(scene, idle_input());
    Access::update(scene, 1.0 / 60.0);
    deliveries = 0;

    auto pointer = idle_input();
    pointer.events.push_back({.type = RawInputEventType::MouseMoved, .device = InputDevice::Mouse,
        .mouse_x = 812, .mouse_y = 302, .source = InputSourceId::mouse()});
    Access::input(scene, press(RawInputControl::MouseLeft));
    Access::input(scene, release(RawInputControl::MouseLeft));
    Access::input(scene, pointer);
    auto wheel = idle_input();
    wheel.events.push_back({.type = RawInputEventType::MouseWheel, .device = InputDevice::Mouse,
        .wheel_y = 1.0f, .source = InputSourceId::mouse()});
    Access::input(scene, wheel);
    Access::input(scene, wheel);
    Access::update(scene, 0.0);
    expect(deliveries == 0, "zero-tick frames buffer input without delivering commands");
    Access::update(scene, 1.0 / 60.0);
    expect(deliveries == 1 && !last.state.is_pressed(Fire)
               && std::ranges::count_if(last.events, [&](const auto& event) {
                   return event.action == Fire && event.phase == ActionInputPhase::Started;
               }) == 1 && last.deltas.contains(ZoomWheel) && last.deltas.at(ZoomWheel).x == 2.0f,
           "a quick press-release and accumulated wheel input survive until the next tick exactly once");
    expect(last.pointer_updated && last.pointer_position == elysia::core::Vector2{812.0f, 302.0f}
               && last.input_device == InputDevice::Mouse,
           "commands carry the routed logical pointer position and active device");
    const auto old_controller = last.controller;
    Access::update(scene, 1.0 / 60.0);
    expect(last.events.empty() && last.deltas.empty() && !last.pointer_updated
               && last.pointer_position == elysia::core::Vector2{812.0f, 302.0f},
           "subsequent ticks keep pointer position without replaying edges, deltas, or pointer updates");

    Access::input(scene, pointer);
    Access::input(scene, press(RawInputControl::MouseLeft));
    Access::input(scene, wheel);
    auto captured = idle_input();
    captured.capture = InputCapture::Pointer;
    Access::input(scene, captured);
    Access::update(scene, 1.0 / 60.0);
    expect(last.events.empty() && last.deltas.empty() && !last.pointer_position && !last.pointer_updated,
           "pointer capture discards pending fire, wheel, and absolute pointer input");
    Access::input(scene, idle_input());
    Access::input(scene, press(RawInputControl::MouseLeft));
    scene.pause();
    scene.resume();
    Access::input(scene, idle_input());
    Access::update(scene, 1.0 / 60.0);
    expect(cancellations > 0 && last.events.empty() && !last.pointer_position,
           "pause cancels buffered commands before the target can receive them");
    Access::input(scene, press(RawInputControl::MouseLeft));
    auto unfocused = idle_input();
    unfocused.focus_lost = true;
    Access::input(scene, unfocused);
    Access::update(scene, 1.0 / 60.0);
    expect(last.events.empty() && !last.pointer_position,
           "focus loss cancels buffered pointer and fire commands");
    Access::exit(scene);
    expect(!elysia::gameplay::ControllerService::instance()->describe(old_controller)
               && !elysia::gameplay::ControllerService::instance()->session_active()
               && !elysia::gameplay::collision::GameplayCollisionService::instance()->has_active_runtime(),
           "exit retires controller handles, the control session, and the active collision runtime");
}

void test_gamepad_ownership(const elysia::scene::SceneRuntimeContext& context)
{
    game::scene::GameScene scene;
    Access::enter(scene, context, game::level::GameScenePayload{});
    Access::input(scene, idle_input());
    Access::update(scene, 1.0 / 60.0);
    auto* moon = Access::find<game::launcher::MoonCell>(scene);
    const auto original_aim = moon->aim_direction();
    auto pad = idle_input();
    const auto source = InputSourceId::gamepad(2);
    pad.sources.push_back({.source = source});
    pad.sources.back().frame.state.set_pressed(RawInputControl::GamepadSouth, true);
    pad.events.push_back({.control = RawInputControl::GamepadSouth,
        .type = RawInputEventType::ControlPressed, .device = InputDevice::Gamepad, .source = source});
    Access::command(scene, pad);
    expect(scene.local_players().owner(source) == PrimaryLocalPlayer
               && !scene.local_players().owner(InputSourceId::gamepad(1)).value
               && Access::find<game::projectile::Projectile>(scene) == nullptr,
           "an unassigned gamepad takes ownership without firing from the activation press");
    auto neutral = idle_input();
    neutral.sources.push_back({.source = source});
    Access::input(scene, neutral);
    Access::command(scene, pad);
    auto* projectile = Access::find<game::projectile::Projectile>(scene);
    expect(projectile && moon->aim_direction() == original_aim,
           "the assigned gamepad fires while preserving aim when its right stick is neutral");
    Access::exit(scene);

    Access::reset(scene);
    Access::enter(scene, context, game::level::GameScenePayload{});
    Access::input(scene, idle_input());
    Access::input(scene, press(RawInputControl::MouseLeft));
    Access::input(scene, release(RawInputControl::MouseLeft));
    Access::update(scene, 0.0);
    expect(Access::find<game::projectile::Projectile>(scene) == nullptr,
           "a real launch waits for command delivery even when press and release occur between ticks");
    Access::update(scene, 1.0 / 60.0);
    expect(Access::find_all<game::projectile::Projectile>(scene).size() == 1,
           "the queued short click launches exactly one projectile");
    Access::update(scene, 0.1);
    expect(Access::find_all<game::projectile::Projectile>(scene).size() == 1,
           "later fixed steps do not replay a consumed fire command");
    Access::exit(scene);
}

void test_direct_camera_pan(const elysia::scene::SceneRuntimeContext& context)
{
    const auto& definition = game::level::GameLevelCatalog::get(game::level::GameLevelId::FirstStrike);
    for (double delta : {1.0 / 30.0, 1.0 / 60.0, 1.0 / 120.0})
    {
        game::scene::GameScene scene;
        Access::enter(scene, context, game::level::GameScenePayload{game::level::GameLevelId::FirstStrike});
        Access::cameras(scene).set_zoom(elysia::camera::CameraSlot::Main, definition.camera.minimum_zoom);
        Access::cameras(scene).set_center(elysia::camera::CameraSlot::Main, game::level::map::kBounds.center());
        Access::update(scene, delta);

        const auto move = [&](float stick_y, float expected_y, bool focus_lost = false) {
            auto input = idle_input();
            input.sources.push_back({.source = InputSourceId::gamepad(1)});
            input.sources.back().frame.state.set_axis(RawInputAxis::GamepadLeftY, stick_y);
            input.focus_lost = focus_lost;
            const auto before = scene.camera().center();
            Access::input(scene, input);
            const int frames = delta < 1.0 / 60.0 ? 2 : 1;
            for (int frame = 0; frame < frames; ++frame) Access::update(scene, delta);
            const auto movement = scene.camera().center() - before;
            const float expected_distance = expected_y * definition.camera.pan_speed
                / scene.camera().zoom() * static_cast<float>(delta * frames);
            if (std::fabs(movement.y - expected_distance) >= 0.01f)
                std::cerr << "Camera delta " << delta << ", stick " << stick_y
                          << ", expected " << expected_distance << ", actual " << movement.y
                          << ", center " << before.y << '\n';
            expect(std::fabs(movement.y - expected_distance) < 0.01f
                       && std::fabs(movement.x) < 0.01f,
                   "camera pan responds linearly at the next fixed tick at 30/60/120 FPS");
        };

        move(-0.5f, -0.5f);
        move(-1.0f, -1.0f);
        move(0.5f, 0.5f);
        move(0.0f, 0.0f);
        move(0.1f, 0.0f);
        move(-1.0f, -1.0f);
        move(-1.0f, 0.0f, true);
        Access::exit(scene);
    }
}

void test_camera_map_bounds(const elysia::scene::SceneRuntimeContext& context)
{
    using elysia::camera::CameraSlot;
    const auto& bounds = game::level::map::kBounds;
    const auto& definition = game::level::GameLevelCatalog::get(game::level::GameLevelId::FirstStrike);
    game::scene::GameScene scene;
    Access::enter(scene, context, game::level::GameScenePayload{});
    Access::update(scene, 1.0 / 60.0);
    const auto half_view = scene.camera().world_viewport_size() * 0.5f;
    const float surface_bottom = definition.moon_cell.cannon_pivot.y
        + definition.moon_cell.base_visual_size.y * (1.0f - definition.moon_cell.base_visual_anchor.y);
    struct Edge
    {
        RawInputControl outward, inward;
        elysia::core::Vector2 direction;
        float coordinate;
    };
    const std::array edges{
        Edge{RawInputControl::KeyA, RawInputControl::KeyD, {-1.0f, 0.0f}, bounds.left() + half_view.x},
        Edge{RawInputControl::KeyD, RawInputControl::KeyA, {1.0f, 0.0f}, bounds.right() - half_view.x},
        Edge{RawInputControl::KeyW, RawInputControl::KeyS, {0.0f, -1.0f}, bounds.top() + half_view.y},
        Edge{RawInputControl::KeyS, RawInputControl::KeyW, {0.0f, 1.0f}, surface_bottom - 2.0f - half_view.y}};
    for (const auto& edge : edges)
    {
        Access::input(scene, press(edge.outward));
        for (int frame = 0; frame < 30; ++frame) Access::update(scene, 0.1);
        const auto at_edge = scene.camera().center();
        const float coordinate = edge.direction.x != 0.0f ? at_edge.x : at_edge.y;
        expect(std::fabs(coordinate - edge.coordinate) < 0.01f,
               "observation movement stops with the viewport at each map edge");
        Access::input(scene, press(edge.inward));
        Access::update(scene, 1.0 / 60.0);
        const auto expected = at_edge - edge.direction
            * (definition.camera.pan_speed / scene.camera().zoom() / 60.0f);
        expect(scene.camera().center().distance_to(expected) < 0.01f,
               "reversing at an edge moves immediately without accumulated overshoot");
    }

    Access::input(scene, idle_input());
    auto zoom_out = idle_input();
    zoom_out.events.push_back({.type = RawInputEventType::MouseWheel,
        .device = InputDevice::Mouse, .wheel_y = -20.0f, .source = InputSourceId::mouse()});
    Access::input(scene, zoom_out);
    Access::update(scene, 1.0 / 60.0);
    expect(std::fabs(scene.camera().zoom() - definition.camera.minimum_zoom) < 0.0001f
               && std::fabs(scene.camera().center().x - bounds.center().x) < 0.01f,
           "zooming out centers the axis whose viewport is wider than the map");
    Access::input(scene, press(RawInputControl::KeyD));
    Access::update(scene, 0.1);
    expect(std::fabs(scene.camera().center().x - bounds.center().x) < 0.01f,
           "panning cannot displace an axis whose viewport covers the entire map");
    Access::input(scene, idle_input());
    Access::cameras(scene).set_zoom(CameraSlot::Main, definition.camera.initial_zoom);
    Access::input(scene, press(RawInputControl::KeyA));
    Access::update(scene, 1.0 / 60.0);
    expect(scene.camera().center().x < bounds.center().x,
           "zooming back in allows immediate movement without hidden pan offsets");
    Access::exit(scene);
}

void test_fixed_map_projectile_bounds(const elysia::scene::SceneRuntimeContext& context)
{
    using game::level::GameLevelId;
    const auto& bounds = game::level::map::kBounds;
    const auto& cleanup = game::level::map::kProjectileBounds;
    for (auto level_id : {GameLevelId::FirstStrike, GameLevelId::Prototype})
    {
        game::scene::GameScene scene;
        Access::enter(scene, context, game::level::GameScenePayload{level_id});
        Access::update(scene, 1.0 / 60.0);
        const auto* backdrop = Access::find<game::presentation::ArenaBackdrop>(scene);
        expect(backdrop && backdrop->world_rect() == game::level::map::kBackdropBounds,
               "both levels use the shared backdrop bounds");
        const auto observation_center = scene.camera().center();
        Access::command(scene, press(RawInputControl::MouseLeft));
        auto* projectile = Access::find<game::projectile::Projectile>(scene);
        if (!projectile) throw std::runtime_error("Missing projectile in map bounds test.");
        const auto move_projectile = [&](elysia::core::Vector2 center) {
            expect(Access::physics(scene).teleport_object(projectile->physics_handle(),
                center - projectile->world_rect().size() * 0.5f),
                "test projectile can be positioned at a map boundary");
            projectile->set_velocity({});
            Access::update(scene, 1.0 / 60.0);
        };
        move_projectile({bounds.left() - 100.0f, bounds.center().y});
        expect(Access::find<game::projectile::Projectile>(scene) == projectile
                   && std::fabs(scene.camera().view_rect().left() - bounds.left()) < 0.001f,
               "the projectile continues outside the left edge while the flight viewport stays inside");
        move_projectile({bounds.right() + 100.0f, bounds.center().y});
        expect(std::fabs(scene.camera().view_rect().right() - bounds.right()) < 0.001f,
               "the flight viewport stops at the right map edge");
        move_projectile({bounds.center().x, bounds.top() - 100.0f});
        expect(std::fabs(scene.camera().view_rect().top() - bounds.top()) < 0.001f,
               "the flight viewport stops at the top map edge");
        // Keep clear of the MoonCell collision body below the battlefield.
        move_projectile({bounds.right() + 100.0f, bounds.bottom() + 100.0f});
        const auto& moon = game::level::GameLevelCatalog::get(level_id).moon_cell;
        const float surface_bottom = moon.cannon_pivot.y
            + moon.base_visual_size.y * (1.0f - moon.base_visual_anchor.y);
        expect(std::fabs(scene.camera().view_rect().right() - bounds.right()) < 0.001f
                   && scene.camera().view_rect().bottom() <= surface_bottom - 1.0f,
               "the flight viewport stays above the artwork bottom when the projectile exits a corner");
        move_projectile(bounds.center());
        expect(scene.camera().center().distance_to(projectile->render_rect().center()) < 0.001f,
               "the flight camera resumes exact presentation tracking when the projectile returns inside");
        move_projectile({cleanup.left(), bounds.center().y});
        expect(Access::find<game::projectile::Projectile>(scene) == projectile,
               "a projectile whose center is on the cleanup boundary remains active");
        move_projectile({cleanup.left() - 1.0f, bounds.center().y});
        expect(Access::find<game::projectile::Projectile>(scene) == nullptr
                   && Access::cameras(scene).presented_slot() == elysia::camera::CameraSlot::Main
                   && scene.camera().center() == observation_center,
               "crossing the shared cleanup boundary ends the shot and restores observation");
        const auto& definition = game::level::GameLevelCatalog::get(level_id);
        Access::cameras(scene).set_zoom(elysia::camera::CameraSlot::Main, definition.camera.minimum_zoom);
        Access::input(scene, idle_input());
        Access::command(scene, press(RawInputControl::MouseLeft));
        projectile = Access::find<game::projectile::Projectile>(scene);
        if (!projectile) throw std::runtime_error("Missing zoomed-out projectile in map bounds test.");
        move_projectile({bounds.left() - 100.0f, bounds.center().y});
        expect(std::fabs(scene.camera().center().x - bounds.center().x) < 0.001f,
               "a flight viewport wider than the battlefield remains horizontally centered");
        Access::exit(scene);
    }
}

void test_gameplay_collision_routing(const elysia::scene::SceneRuntimeContext& context)
{
    using namespace elysia::gameplay::collision;
    struct ContactObserver final : GameplayCollisionListener
    {
        elysia::physics::ColliderId projectile = 0, target = 0;
        int begins = 0;
        void on_body_contact(const BodyContactEvent& event) override
        {
            if (event.body.collider != projectile || event.phase != elysia::physics::CollisionEventPhase::Begin)
                return;
            ++begins;
            expect(event.body.team == teams::Player && event.body.role == ColliderRole::Body
                       && event.other.collider == target,
                   "engine body events carry the projectile team and the contacted collider");
        }
    } observer;
    game::scene::GameScene scene;
    Access::enter(scene, context, game::level::GameScenePayload{game::level::GameLevelId::Prototype});
    for (int generation = 0; generation < 2; ++generation)
    {
        Access::update(scene, 1.0 / 60.0);
        game::fleet::EnemyShip* flagship = nullptr;
        for (auto* ship : Access::find_all<game::fleet::EnemyShip>(scene))
            if (ship->role() == game::fleet::ShipRole::Flagship) flagship = ship;
        const auto portals = Access::find_all<game::anomaly::WormholePortal>(scene);
        const auto* hole = Access::find<game::anomaly::BlackHoleAnomaly>(scene);
        if (!flagship || portals.size() != 2 || !hole)
            throw std::runtime_error("Missing gameplay collision targets.");
        Access::input(scene, idle_input());
        Access::command(scene, press(RawInputControl::MouseLeft));
        auto* projectile = Access::find<game::projectile::Projectile>(scene);
        if (!projectile) throw std::runtime_error("Missing semantic collision projectile.");
        const auto place = [&](elysia::core::Vector2 center, elysia::core::Vector2 velocity) {
            expect(Access::physics(scene).teleport_object(projectile->physics_handle(),
                center - projectile->world_rect().size() * 0.5f), "collision test positions a real physics body");
            projectile->set_velocity(velocity);
            Access::update(scene, 1.0 / 60.0);
        };
        observer.projectile = projectile->physics_collider(0);
        observer.target = flagship->collider_id();
        observer.begins = 0;
        auto* collisions = GameplayCollisionService::instance();
        expect(collisions->add_listener(observer), "the built-in gameplay collision service is active");
        place(flagship->center() + elysia::core::Vector2{0.0f, 150.0f}, {0.0f, -600.0f});
        // Box2D can publish a CCD begin event on the following step.
        for (int frame = 0; frame < 3 && observer.begins == 0; ++frame)
            Access::update(scene, 1.0 / 60.0);
        expect(observer.begins == 1 && !projectile->finished() && projectile->velocity().y > 0.0f
                   && flagship->hit_points() == 3,
               "a physical shield contact reflects the shot without damaging the flagship, including after restart");

        observer.target = portals.front()->collider_id();
        place(portals.front()->center(), {0.0f, -300.0f});
        const auto& definition = game::level::GameLevelCatalog::get(game::level::GameLevelId::Prototype);
        const game::anomaly::WormholePairConfig* pair = nullptr;
        for (const auto& anomaly : definition.anomalies)
            if (const auto* candidate = std::get_if<game::anomaly::WormholePairConfig>(&anomaly)) pair = candidate;
        expect(pair && observer.begins == 2 && !projectile->finished()
                   && projectile->center().distance_to(portals.back()->center()
                       + elysia::core::Vector2{0.0f, -pair->exit_offset}) < 10.0f
                   && projectile->velocity().y < -290.0f,
               "overlap body events teleport the shot through the paired portal and preserve its velocity");
        observer.target = hole->collider_id();
        place(hole->center(), {0.0f, -300.0f});
        expect(observer.begins == 3 && !Access::find<game::projectile::Projectile>(scene),
               "black-hole overlap ends the projectile through the same engine event route");
        expect(collisions->remove_listener(observer), "the test listener retires before scene reconstruction");

        if (generation == 0)
        {
            for (int frame = 0; frame < 90; ++frame) Access::update(scene, 1.0 / 60.0);
            for (auto* ship : Access::find_all<game::fleet::EnemyShip>(scene))
                (void)ship->receive_damage({.amount = ship->hit_points() - (ship == flagship ? 1 : 0)});
            Access::input(scene, idle_input());
            Access::command(scene, press(RawInputControl::MouseLeft));
            projectile = Access::find<game::projectile::Projectile>(scene);
            if (!projectile) throw std::runtime_error("Missing pre-restart winning projectile.");
            place(flagship->center(), {0.0f, -300.0f});
            Access::command(scene, press(RawInputControl::KeyR));
            expect(Access::find_all<game::fleet::EnemyShip>(scene).size() == 3,
                   "restart rebuilds the complete fleet before collision routing is exercised again");
        }
    }
    Access::exit(scene);
}

void test_first_strike(const elysia::scene::SceneRuntimeContext& context)
{
    using game::level::GameLevelId;
    game::scene::GameScene scene;
    Access::enter(scene, context, game::level::GameScenePayload{});
    Access::update(scene, 1.0 / 60.0);
    const auto ships = Access::find_all<game::fleet::EnemyShip>(scene);
    expect(ships.size() == 1 && Access::physics(scene).registered_object_count() == 2,
           "the default first level contains only the launcher and one enemy physics body");
    if (ships.size() != 1) throw std::runtime_error("Missing first-level flagship.");
    auto* flagship = ships.front();
    expect(flagship->role() == game::fleet::ShipRole::Flagship
               && flagship->ability() == game::fleet::ShipAbility::None
               && flagship->hit_points() == 2,
           "the first-level flagship starts with two hit points and no ability");
    for (int shot = 0; shot < 2; ++shot)
    {
        aim_and_fire(scene, flagship->center());
        expect(Access::find<game::projectile::Projectile>(scene) != nullptr,
               "the first level accepts a shot at default power");
        for (int frame = 0;
             frame < 1260 && Access::find<game::projectile::Projectile>(scene); ++frame)
            Access::update(scene, 1.0 / 60.0);
        expect(flagship->hit_points() == 1 - shot,
               "each naturally flown shot deals one damage to the first-level flagship");
        for (int frame = 0; frame < 90; ++frame) Access::update(scene, 1.0 / 60.0);
    }
    expect(flagship->is_defeated() && Access::find<game::fleet::FlagshipLaser>(scene) == nullptr,
           "two natural hits win the first level without triggering the failure laser");
    Access::command(scene, press(RawInputControl::KeyR));
    expect(Access::find<game::fleet::EnemyShip>(scene)->hit_points() == 2,
           "restart preserves the selected first level");
    for (int shot = 0; shot < 4; ++shot)
    {
        Access::input(scene, idle_input());
        Access::command(scene, press(RawInputControl::MouseLeft));
        auto* projectile = Access::find<game::projectile::Projectile>(scene);
        expect(projectile != nullptr, "all four first-level rounds are available");
        if (!projectile) break;
        projectile->update(21.0);
        Access::update(scene, 0.0);
        expect((Access::find<game::fleet::FlagshipLaser>(scene) != nullptr) == (shot == 3),
               "only the fourth failed shot starts the first-level failure sequence");
    }
    Access::update(scene, 3.0);
    Access::input(scene, idle_input());
    Access::command(scene, press(RawInputControl::MouseLeft));
    expect(Access::find<game::projectile::Projectile>(scene) == nullptr,
           "a fifth first-level shot is rejected after ammunition is exhausted");
    Access::exit(scene);
    Access::reset(scene);
    Access::enter(scene, context, game::level::GameScenePayload{GameLevelId::Prototype});
    Access::update(scene, 1.0 / 60.0);
    expect(Access::find_all<game::fleet::EnemyShip>(scene).size() == 3,
           "selecting the second level after the first builds the original fleet");
    Access::exit(scene);
}

void test_return_menu_dialog(const elysia::scene::SceneRuntimeContext& context)
{
    RouteCapture capture;
    game::scene::GameScene scene;
    scene.attach(&capture);
    Access::enter(scene, context, game::level::GameScenePayload{});
    Access::update(scene, 1.0 / 60.0);
    auto* window = Access::find_ui<elysia::ui::UiWindow>(scene);
    auto* dialog = Access::find_ui<elysia::ui::UiConfirmationDialog>(scene);
    if (!window || !dialog) throw std::runtime_error("Missing return-menu dialog.");
    expect(!window->is_overlay_open(*dialog), "return-menu dialog starts closed");

    Access::input(scene, press(RawInputControl::KeyEscape));
    Access::input(scene, release(RawInputControl::KeyEscape));
    expect(scene.is_paused() && window->is_overlay_open(*dialog),
           "Escape opens a modal return-menu dialog and pauses aiming");
    Access::command(scene, press(RawInputControl::MouseLeft));
    Access::update(scene, 0.5);
    expect(Access::find<game::projectile::Projectile>(scene) == nullptr
               && window->is_overlay_open(*dialog),
           "clicking outside the modal cannot fire or dismiss the confirmation");
    Access::input(scene, idle_input());
    Access::input(scene, press(RawInputControl::KeyEnter));
    Access::input(scene, release(RawInputControl::KeyEnter));
    Access::update(scene, 0.0);
    expect(!scene.is_paused() && !window->is_overlay_open(*dialog)
               && capture.last_request.type == elysia::scene::SceneRequestType::None,
           "the initially focused Cancel button resumes without leaving the level");

    auto* flagship = Access::find<game::fleet::EnemyShip>(scene);
    aim_and_fire(scene, flagship->center());
    Access::update(scene, 0.05);
    auto* projectile = Access::find<game::projectile::Projectile>(scene);
    if (!projectile) throw std::runtime_error("Missing projectile in return-menu test.");
    Access::input(scene, press(RawInputControl::KeyEscape));
    Access::input(scene, release(RawInputControl::KeyEscape));
    const auto paused_position = projectile->center();
    const auto paused_camera = scene.camera().center();
    Access::update(scene, 25.0);
    expect(!projectile->finished() && projectile->center() == paused_position
               && scene.camera().center() == paused_camera,
           "the modal freezes projectile lifetime, physics, and the flight camera");
    Access::input(scene, press(RawInputControl::KeyEscape));
    Access::input(scene, release(RawInputControl::KeyEscape));
    Access::update(scene, 0.0);
    expect(!scene.is_paused() && !window->is_overlay_open(*dialog),
           "Escape dismisses the open dialog and resumes the same flight");
    Access::update(scene, 0.05);
    expect(projectile->center().distance_to(paused_position) > 1.0f,
           "the existing projectile continues after cancelling the dialog");

    scene.pause();
    Access::input(scene, press(RawInputControl::KeyEscape));
    Access::input(scene, release(RawInputControl::KeyEscape));
    Access::input(scene, press(RawInputControl::KeyEscape));
    Access::input(scene, release(RawInputControl::KeyEscape));
    expect(scene.is_paused() && !window->is_overlay_open(*dialog),
           "cancelling a dialog opened during an existing pause preserves that pause");
    scene.resume();

    Access::input(scene, press(RawInputControl::KeyEscape));
    Access::input(scene, release(RawInputControl::KeyEscape));
    Access::input(scene, press(RawInputControl::KeyRight));
    Access::input(scene, release(RawInputControl::KeyRight));
    Access::input(scene, press(RawInputControl::KeyEnter));
    Access::input(scene, release(RawInputControl::KeyEnter));
    expect(capture.last_request.type == elysia::scene::SceneRequestType::Switch
               && capture.last_request.route.target == game::scene_keys::MainMenu
               && scene.is_paused(),
           "confirm requests the main menu while keeping the departing game paused");
    Access::exit(scene);
    expect(Access::physics(scene).registered_object_count() == 0,
           "leaving through the menu dialog cleans up in-flight physics");
    Access::reset(scene);
    Access::enter(scene, context, game::level::GameScenePayload{});
    Access::update(scene, 1.0 / 60.0);
    window = Access::find_ui<elysia::ui::UiWindow>(scene);
    dialog = Access::find_ui<elysia::ui::UiConfirmationDialog>(scene);
    expect(!scene.is_paused() && window && dialog && !window->is_overlay_open(*dialog),
           "reentering the level does not inherit the old modal or its pause");
    scene.detach(&capture);
    Access::exit(scene);

    elysia::scene::SceneManager manager;
    manager.initialize(context);
    manager.register_game_scene<game::scene::MainMenuScene>(game::scene_keys::MainMenu);
    manager.register_game_scene<game::scene::GameScene>(game::scene_keys::Game);
    manager.start({.target = game::scene_keys::Game, .payload = game::level::GameScenePayload{}});
    manager.on_update(1.0 / 60.0);
    for (auto key : {RawInputControl::KeyEscape, RawInputControl::KeyRight, RawInputControl::KeyEnter})
    {
        manager.on_input(press(key));
        manager.on_input(release(key));
    }
    expect(manager.current_scene_key() == game::scene_keys::MainMenu
               && manager.state() == elysia::scene::SceneManagerState::Running,
           "the live scene manager completes the confirmed return to the main menu");
    expect(manager.shutdown(), "return-menu scene transition shuts down cleanly");
}

void test_victory_camera(const elysia::scene::SceneRuntimeContext& context)
{
    game::scene::GameScene scene;
    Access::enter(scene, context, game::level::GameScenePayload{game::level::GameLevelId::Prototype});
    Access::update(scene, 1.0 / 60.0);
    const auto observation_center = scene.camera().center();
    game::fleet::EnemyShip* flagship = nullptr;
    for (auto* ship : Access::find_all<game::fleet::EnemyShip>(scene))
    {
        if (ship->role() == game::fleet::ShipRole::Flagship)
        {
            flagship = ship;
            (void)ship->receive_damage({.amount = ship->hit_points() - 1});
        }
        else
            (void)ship->receive_damage({.amount = ship->hit_points()});
    }
    if (!flagship) throw std::runtime_error("Missing flagship in victory test.");
    Access::input(scene, idle_input());
    Access::command(scene, press(RawInputControl::MouseLeft));
    auto* projectile = Access::find<game::projectile::Projectile>(scene);
    if (!projectile) throw std::runtime_error("Missing projectile in victory test.");
    expect(Access::physics(scene).teleport_object(
        projectile->physics_handle(), flagship->center() - elysia::core::Vector2{8.0f, 8.0f}),
        "the winning projectile can contact the unshielded flagship");
    Access::update(scene, 1.0 / 60.0);
    expect(flagship->is_defeated() && Access::find<game::projectile::Projectile>(scene) == nullptr,
           "the winning impact finishes the projectile");
    expect(Access::cameras(scene).presented_slot() == elysia::camera::CameraSlot::Main
               && scene.camera().center() == observation_center,
           "a winning impact immediately restores the observation camera");
    Access::update(scene, 3.0);
    expect(Access::find<game::fleet::FlagshipLaser>(scene) == nullptr,
           "victory never starts the failure camera or laser sequence");
    Access::command(scene, press(RawInputControl::KeyR));
    expect(Access::find<game::fleet::EnemyShip>(scene)->hit_points() > 1,
           "restart is available after the winning impact");
    Access::exit(scene);
}

void test_mooncell_camera_framing(const elysia::scene::SceneRuntimeContext& context,
                                 SDL_Renderer* renderer, SDL_Surface* surface)
{
    const auto texture_root = std::filesystem::path(__FILE__).parent_path().parent_path() / "assets/textures";
    for (const auto& [key, file] : std::array{
        std::pair{"test.mooncell.base", "moon_cell_base.png"},
        std::pair{"test.mooncell.cannon", "moon_cell.png"}})
    {
        elysia::resources::TexturePtr texture(IMG_LoadTexture(renderer, (texture_root / file).string().c_str()));
        if (!texture || !elysia::resources::ResourceManager::instance()->store_texture(key, std::move(texture)))
            throw std::runtime_error("Cannot load real MoonCell artwork for framing test.");
    }
    const auto level_id = game::level::GameLevelId::Prototype;
    auto moon_config = game::level::GameLevelCatalog::get(level_id).moon_cell;
    moon_config.base_texture_key = "test.mooncell.base";
    moon_config.cannon_texture_key = "test.mooncell.cannon";
    game::launcher::MoonCell moon_cell(moon_config);
    std::vector<elysia::core::RenderCommand> world_commands;
    moon_cell.submit_render_commands(world_commands);
    expect(world_commands.size() == 2 && world_commands.front().type == elysia::core::RenderCommandType::Texture,
           "MoonCell keeps its original two artwork layers without an added ground fill");
    const float surface_bottom = world_commands.front().command_rect.bottom();
    game::scene::GameScene scene;
    Access::enter(scene, context, game::level::GameScenePayload{level_id});
    Access::update(scene, 1.0 / 60.0);
    const auto check_view = [&] {
        expect(scene.camera().view_rect().bottom() <= surface_bottom - 1.0f
                   && scene.camera().view_rect().top() >= -0.001f,
               "the entire viewport stays above the surface artwork cutoff, including while blending and zooming");
    };
    const auto render_view = [&](const char* filename) {
        check_view();
        SDL_SetRenderDrawColor(renderer, 7, 11, 28, 255);
        SDL_RenderClear(renderer);
        std::vector<elysia::core::ScreenRenderCommand> screen_commands;
        elysia::core::project_render_commands_to_screen(world_commands, scene.camera(), screen_commands);
        elysia::core::require_render_success(elysia::core::execute_render_commands(renderer, screen_commands));
        SDL_RenderPresent(renderer);
        if (const char* output = SDL_getenv("ORBITALSTRIKE_VISUAL_CHECK_DIR"))
        {
            std::filesystem::create_directories(output);
            if (!SDL_SavePNG(surface, (std::filesystem::path(output) / filename).string().c_str()))
                throw std::runtime_error("Cannot save MoonCell framing check.");
        }
    };
    Access::input(scene, press(RawInputControl::KeyS));
    for (int frame = 0; frame < 30; ++frame) Access::update(scene, 0.1);
    render_view("mooncell-aiming.png");
    auto wheel = idle_input();
    wheel.events.push_back({.type = RawInputEventType::MouseWheel,
        .device = InputDevice::Mouse, .wheel_y = -20.0f, .source = InputSourceId::mouse()});
    Access::input(scene, wheel);
    Access::update(scene, 1.0 / 60.0);
    render_view("mooncell-zoomed-out.png");
    wheel.events.front().wheel_y = 20.0f;
    Access::input(scene, wheel);
    auto zoom_hold = idle_input();
    zoom_hold.sources.push_back({.source = InputSourceId::gamepad(1)});
    zoom_hold.sources.back().frame.state.set_pressed(RawInputControl::GamepadLeftShoulder, true);
    Access::input(scene, zoom_hold);
    for (int frame = 0; frame < 60; ++frame)
    {
        Access::update(scene, 0.1);
        check_view();
    }
    Access::input(scene, idle_input());
    exhaust_rounds(scene, level_id);
    const auto& definition = game::level::GameLevelCatalog::get(level_id);
    for (int frame = 0; frame < 40; ++frame)
    {
        Access::update(scene, definition.camera.flagship_blend_seconds / 40.0);
        check_view();
    }
    Access::update(scene, 0.001);
    for (int frame = 0; frame < 60; ++frame)
    {
        Access::update(scene, definition.mission.flagship_warning_seconds / 60.0);
        check_view();
    }
    Access::update(scene, 0.001);
    render_view("mooncell-wide.png");
    Access::exit(scene);
}
void test_mooncell_gravity(const elysia::scene::SceneRuntimeContext& context)
{
    using game::level::GameLevelId;
    for (auto level_id : {GameLevelId::FirstStrike, GameLevelId::Prototype})
    {
        game::scene::GameScene scene;
        Access::enter(scene, context, game::level::GameScenePayload{level_id});
        Access::update(scene, 1.0 / 60.0);
        const auto* moon_cell = Access::find<game::launcher::MoonCell>(scene);
        if (!moon_cell) throw std::runtime_error("Missing MoonCell in gravity test.");
        const auto& definition = game::level::GameLevelCatalog::get(level_id);
        const auto distant_force = moon_cell->force_on({
            .position = definition.moon_cell.moon_center + elysia::core::Vector2{0.0f, -1800.0f}});
        expect(level_id == GameLevelId::Prototype ? distant_force.y > 0.0f : distant_force.is_zero(),
               "MoonCell gravity reaches beyond the old range only in the gravity-enabled level");

        aim_and_fire(scene, moon_cell->cannon_pivot() + elysia::core::Vector2{500.0f, 0.0f});
        const auto* projectile = Access::find<game::projectile::Projectile>(scene);
        if (!projectile) throw std::runtime_error("Missing horizontal projectile in gravity test.");
        const float launch_y = projectile->center().y;
        for (int frame = 0; frame < 30; ++frame) Access::update(scene, 1.0 / 60.0);
        projectile = Access::find<game::projectile::Projectile>(scene);
        if (!projectile) throw std::runtime_error("Horizontal projectile ended before gravity measurement.");
        const float drop = projectile->center().y - launch_y;
        if (level_id == GameLevelId::Prototype)
            expect(drop > 20.0f && projectile->velocity().y > 50.0f,
                   "MoonCell gravity visibly bends a horizontal shot toward the moon within half a second");
        else
            expect(std::fabs(drop) < 0.01f && std::fabs(projectile->velocity().y) < 0.01f,
                   "the introductory level retains straight flight without MoonCell gravity");
        Access::exit(scene);
    }
}

void test_prototype_aiming_tolerance(const elysia::scene::SceneRuntimeContext& context)
{
    // Use real input and physics, including every field and portal. No teleports or direct damage.
    // A one-degree aiming error should still allow the shield-first route at default launch power.
    for (float error_degrees : {-1.0f, 0.0f, 1.0f})
    {
        game::scene::GameScene scene;
        Access::enter(scene, context, game::level::GameScenePayload{game::level::GameLevelId::Prototype});
        Access::update(scene, 1.0 / 60.0);
        auto* moon_cell = Access::find<game::launcher::MoonCell>(scene);
        game::fleet::EnemyShip* flagship = nullptr;
        game::fleet::EnemyShip* projector = nullptr;
        game::fleet::EnemyShip* repulsor = nullptr;
        for (auto* ship : Access::find_all<game::fleet::EnemyShip>(scene))
        {
            if (ship->role() == game::fleet::ShipRole::Flagship) flagship = ship;
            if (ship->ability() == game::fleet::ShipAbility::ShieldProjector) projector = ship;
            if (ship->ability() == game::fleet::ShipAbility::RepulsionField) repulsor = ship;
        }
        if (!moon_cell || !flagship || !projector || !repulsor)
            throw std::runtime_error("Missing prototype aiming targets.");

        const auto& definition = game::level::GameLevelCatalog::get(game::level::GameLevelId::Prototype);
        const int required_hits = projector->hit_points() + flagship->hit_points();
        expect(definition.mission.maximum_rounds >= required_hits * 3,
               "the prototype leaves at least two practice shots per required hit");
        for (int shot = 0; shot < required_hits; ++shot)
        {
            auto* target = projector->is_defeated() ? flagship : projector;
            const int health_before = target->hit_points();
            aim_and_fire(scene, target->center(), error_degrees);
            expect(Access::find<game::projectile::Projectile>(scene) != nullptr,
                   "the prototype route launches through mouse input");
            float nearest_distance = 10000.0f;
            elysia::core::Vector2 last_position{};
            for (int frame = 0;
                 frame < 1260 && Access::find<game::projectile::Projectile>(scene); ++frame)
            {
                const auto* projectile = Access::find<game::projectile::Projectile>(scene);
                last_position = projectile->center();
                nearest_distance = std::min(nearest_distance, last_position.distance_to(target->center()));
                Access::update(scene, 1.0 / 60.0);
            }
            if (target->hit_points() != health_before - 1)
                std::cerr << "Aim error " << error_degrees << ", shot " << shot + 1
                          << ", nearest target distance " << nearest_distance
                          << ", last position " << last_position.x << ',' << last_position.y << '\n';
            expect(target->hit_points() == health_before - 1,
                   "a one-degree aiming error still hits the intended target at default power");
            for (int frame = 0; frame < 90; ++frame) Access::update(scene, 1.0 / 60.0);
        }
        expect(projector->is_defeated() && flagship->is_defeated() && !repulsor->is_defeated(),
               "the prototype can be won through natural flight without clearing the repulsor");
        expect(Access::find<game::fleet::FlagshipLaser>(scene) == nullptr,
               "the accessible prototype route wins before the flagship fires");
        Access::exit(scene);
    }
}

void test_failure_cinematic(const elysia::scene::SceneRuntimeContext& context)
{
    using elysia::camera::CameraSlot;
    using game::fleet::FlagshipLaserPhase;
    const auto& definition = game::level::GameLevelCatalog::get(game::level::GameLevelId::Prototype);
    game::scene::GameScene scene;
    Access::enter(scene, context, game::level::GameScenePayload{game::level::GameLevelId::Prototype});
    Access::update(scene, 1.0 / 60.0);
    const auto observation_center = scene.camera().center();
    exhaust_rounds(scene);
    auto& cameras = Access::cameras(scene);
    const auto expect_cinematic_bounds = [&] {
        const auto& bounds = game::level::map::kBounds;
        const auto& cinematic = cameras.slot_camera(CameraSlot::Cinematic);
        const auto view = cinematic.view_rect();
        const bool valid_x = view.width() >= bounds.width()
            ? std::fabs(cinematic.center().x - bounds.center().x) < 0.001f
            : view.left() >= bounds.left() - 0.001f && view.right() <= bounds.right() + 0.001f;
        const bool valid_y = view.height() >= bounds.height()
            ? std::fabs(cinematic.center().y - bounds.center().y) < 0.001f
            : view.top() >= bounds.top() - 0.001f && view.bottom() <= bounds.bottom() + 0.001f;
        expect(valid_x && valid_y,
               "the cinematic camera remains map-constrained throughout the failure sequence");
    };
    expect_cinematic_bounds();
    auto* laser = Access::find<game::fleet::FlagshipLaser>(scene);
    expect(laser && laser->phase() == FlagshipLaserPhase::Warning,
           "round exhaustion starts laser charging without firing");
    if (!laser) throw std::runtime_error("Missing flagship laser.");
    expect(scene.camera().center() == observation_center,
           "the failure blend starts at the restored observation view");
    Access::update(scene, definition.camera.flagship_blend_seconds * 0.5);
    expect(cameras.presented_slot() == CameraSlot::Main
               && scene.camera().center().distance_to(observation_center) > 1.0f
               && laser->phase() == FlagshipLaserPhase::Warning,
           "failure smoothly blends toward the flagship before pulling back");
    expect(std::fabs(cameras.slot_camera(CameraSlot::Cinematic).zoom()
                         - definition.camera.flagship_close_zoom) < 0.0001f,
           "the flagship close-up stays still during the blend");
    expect_cinematic_bounds();
    Access::update(scene, definition.camera.flagship_blend_seconds * 0.5 + 0.001);
    expect(cameras.presented_slot() == CameraSlot::Cinematic
               && scene.camera().center().distance_to(definition.ships.front().center) < 0.001f,
           "the completed blend locates the cinematic camera at the flagship");
    expect_cinematic_bounds();

    for (int frame = 0; frame < 24; ++frame)
    {
        Access::update(scene, definition.mission.flagship_warning_seconds * 0.4 / 24.0);
        expect_cinematic_bounds();
    }
    expect(scene.camera().zoom() < definition.camera.flagship_close_zoom
               && laser->phase() == FlagshipLaserPhase::Warning,
           "the cinematic camera pulls back while the laser is still charging");
    auto wheel = idle_input();
    wheel.events.push_back({.type = RawInputEventType::MouseWheel,
        .device = InputDevice::Mouse, .wheel_y = 5.0f, .source = InputSourceId::mouse()});
    const auto zoom_before_input = scene.camera().zoom();
    Access::input(scene, wheel);
    expect(scene.camera().zoom() == zoom_before_input,
           "observation input cannot interrupt the failure camera motion");
    scene.pause();
    const auto paused_camera = scene.camera();
    Access::update(scene, 5.0);
    expect(scene.camera().center() == paused_camera.center()
               && scene.camera().zoom() == paused_camera.zoom()
               && laser->phase() == FlagshipLaserPhase::Warning,
           "pause freezes the failure camera sequence and delays laser firing");
    scene.resume();
    Access::update(scene, definition.mission.flagship_warning_seconds);
    expect(laser->phase() == FlagshipLaserPhase::Firing,
           "the laser fires only after the pullback motion completes");
    expect(scene.camera().view_rect().contains(definition.ships.front().center)
               && scene.camera().view_rect().contains(definition.moon_cell.cannon_pivot),
           "the final laser shot frames the flagship and MoonCell together");
    expect_cinematic_bounds();
    const auto final_camera = scene.camera();
    Access::update(scene, definition.mission.flagship_firing_seconds + 0.01);
    expect(cameras.presented_slot() == CameraSlot::Cinematic
               && scene.camera().center() == final_camera.center()
               && scene.camera().zoom() == final_camera.zoom(),
           "defeat retains the final wide cinematic view");
    expect_cinematic_bounds();
    Access::command(scene, press(RawInputControl::KeyR));
    expect(cameras.presented_slot() == CameraSlot::Main
               && Access::find<game::fleet::FlagshipLaser>(scene) == nullptr,
           "restart after defeat clears the sequence and restores Main");
    Access::input(scene, idle_input());
    Access::command(scene, press(RawInputControl::MouseLeft));
    auto* projectile = Access::find<game::projectile::Projectile>(scene);
    if (!projectile) throw std::runtime_error("Missing projectile after cinematic restart.");
    expect(Access::physics(scene).teleport_object(projectile->physics_handle(),
        elysia::core::Vector2{game::level::map::kBounds.left() - 100.0f, game::level::map::kBounds.center().y}
            - projectile->world_rect().size() * 0.5f),
        "the restarted projectile can move outside the battlefield");
    projectile->set_velocity({});
    Access::update(scene, 1.0 / 60.0);
    expect(std::fabs(scene.camera().view_rect().left() - game::level::map::kBounds.left()) < 0.001f,
           "restarting after the laser sequence retains flight-camera map bounds");
    Access::exit(scene);

    // An interrupted blend must not advance a rebuilt level into laser firing.
    Access::reset(scene);
    Access::enter(scene, context, game::level::GameScenePayload{game::level::GameLevelId::Prototype});
    Access::update(scene, 1.0 / 60.0);
    exhaust_rounds(scene);
    Access::update(scene, definition.camera.flagship_blend_seconds * 0.25);
    Access::exit(scene);
    Access::reset(scene);
    Access::enter(scene, context, game::level::GameScenePayload{game::level::GameLevelId::Prototype});
    Access::update(scene, 3.0);
    expect(cameras.presented_slot() == CameraSlot::Main
               && Access::find<game::fleet::FlagshipLaser>(scene) == nullptr,
           "exit and reset cancel pending cinematic completion callbacks");
    Access::exit(scene);
}
}

int main()
{
    if (!SDL_Init(0)) return 1;
    std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> surface(
        SDL_CreateSurface(1280, 720, SDL_PIXELFORMAT_RGBA32), SDL_DestroySurface);
    std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer(
        surface ? SDL_CreateSoftwareRenderer(surface.get()) : nullptr, SDL_DestroyRenderer);
    if (!renderer) return 1;
    auto* resources = elysia::resources::ResourceManager::instance();
    try
    {
        for (auto key : {game::resources::texture_keys::MoonCell,
                         game::resources::texture_keys::MoonCellBase,
                         game::resources::texture_keys::Projectile})
        {
            elysia::resources::TexturePtr texture(SDL_CreateTexture(
                renderer.get(), SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, 4, 4));
            if (!resources->store_texture(std::string(key), std::move(texture)))
                throw std::runtime_error("Cannot create test texture.");
        }
        elysia::io::ContentRegistry registry;
        elysia::scene::SceneRuntimeContext context(renderer.get(), registry, 1280, 720);
        test_menu_lifecycle(context);
        test_level_selection(context);
        test_first_strike(context);
        test_return_menu_dialog(context);
        test_game_lifecycle(context);
        test_control_command_buffering(context);
        test_gamepad_ownership(context);
        test_direct_camera_pan(context);
        test_camera_map_bounds(context);
        test_fixed_map_projectile_bounds(context);
        test_gameplay_collision_routing(context);
        test_victory_camera(context);
        test_mooncell_gravity(context);
        test_prototype_aiming_tolerance(context);
        test_failure_cinematic(context);
        test_mooncell_camera_framing(context, renderer.get(), surface.get());
    }
    catch (const std::exception& error)
    {
        ++failures;
        std::cerr << "Scene integration exception: " << error.what() << '\n';
    }
    resources->clear();
    renderer.reset();
    surface.reset();
    SDL_Quit();
    if (failures) return 1;
    std::cout << "All game scene tests passed.\n";
    return 0;
}
