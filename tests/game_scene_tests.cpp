#include "game/scene/game_scene.h"
#include "game/scene/level_select_scene.h"
#include "game/scene/main_menu_scene.h"
#include "game/scene/scene_keys.h"
#include "game/gameplay/projectile/projectile.h"
#include "game/gameplay/fleet/flagship_laser.h"
#include "game/gameplay/fleet/enemy_ship.h"
#include "game/level/game_level_catalog.h"
#include "game/gameplay/launcher/moon_cell.h"
#include "game/resources/game_texture_keys.h"

#include "engine/camera/camera_manager.h"
#include "engine/core/render/render_command.h"
#include "engine/io/loaders/asset_config_types.h"
#include "engine/resources/runtime/resource_manager.h"
#include "engine/scene/scene_manager.h"
#include "engine/ui/composites/ui_confirmation_dialog.h"
#include "engine/ui/window/ui_window.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>
#include <string_view>

namespace elysia::scene
{
class SceneTestAccess
{
public:
    static void enter(Scene& scene, const SceneRuntimeContext& context, const ScenePayload& payload)
    {
        scene.bind_runtime_context(context);
        scene.lifecycle_enter(payload);
    }
    static void exit(Scene& scene) { scene.lifecycle_exit(); }
    static void reset(Scene& scene) { scene.lifecycle_reset(); }
    static void input(Scene& scene, const elysia::input::InputSnapshot& input)
    {
        scene.lifecycle_input(input);
    }
    static void update(Scene& scene, double delta) { scene.lifecycle_update(delta); }
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
    Access::input(scene, fire);
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
    expect(scene.camera().center().y > 3000.0f,
           "the scene camera acquires the MoonCell focus");
    Access::input(scene, idle_input());

    auto wheel = idle_input();
    wheel.events.push_back({.type = RawInputEventType::MouseWheel,
        .device = InputDevice::Mouse, .wheel_y = 1.0f, .source = InputSourceId::mouse()});
    Access::input(scene, wheel);
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
        Access::input(scene, press(RawInputControl::MouseLeft));
        auto* projectile = Access::find<game::projectile::Projectile>(scene);
        expect(projectile != nullptr, "routed mouse input launches a projectile");
        if (!projectile) break;
        expect(cameras.presented_slot() == CameraSlot::Cinematic
                   && scene.camera().center() == projectile->render_rect().center(),
               "launch immediately cuts to the cinematic camera at the projectile");
        const auto start = projectile->center();
        Access::update(scene, 0.05);
        expect(projectile->center().distance_to(start) > 1.0f,
               "fixed-step physics advances the launched projectile");
        expect(scene.camera().center().distance_to(projectile->render_rect().center()) < 0.001f,
               "the cinematic camera hard-follows the interpolated projectile pose");
        std::vector<elysia::core::RenderCommand> commands;
        projectile->submit_render_commands(commands);
        const auto& sprite = commands.front();
        const elysia::core::Vector2 sprite_anchor{
            sprite.command_rect.x() + sprite.command_rect.width() * sprite.rotation_origin.x,
            sprite.command_rect.y() + sprite.command_rect.height() * sprite.rotation_origin.y};
        expect(sprite_anchor.distance_to(scene.camera().center()) < 0.001f,
               "projectile rendering and hard-follow use the same presentation position");
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
            expect(physics.teleport_object(projectile->physics_handle(), {-500.0f, 2700.0f}),
                   "the projectile can travel outside the activity bounds");
            Access::update(scene, 1.0 / 60.0);
            expect(scene.camera().center().x < 0.0f
                       && scene.camera().center().distance_to(projectile->render_rect().center()) < 0.001f,
                   "hard-follow remains centered outside the main-camera bounds");
            Access::update(scene, 21.0);
        }
        else
        {
            expect(physics.teleport_object(projectile->physics_handle(), {650.0f - 8.0f, 2050.0f - 8.0f}),
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
            Access::input(scene, press(RawInputControl::MouseLeft));
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
    Access::input(scene, press(RawInputControl::MouseLeft));
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
        Access::input(scene, press(RawInputControl::MouseLeft));
        auto* projectile = Access::find<game::projectile::Projectile>(scene);
        if (!projectile) throw std::runtime_error("Failed to launch a test round.");
        projectile->update(definition.launch.projectile.lifetime_seconds + 1.0);
        expect(Access::cameras(scene).presented_slot() == elysia::camera::CameraSlot::Main,
               "even the final expired projectile immediately cuts back to Main");
        Access::update(scene, 0.0);
    }
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
    Access::input(scene, press(RawInputControl::KeyR));
    expect(Access::find<game::fleet::EnemyShip>(scene)->hit_points() == 2,
           "restart preserves the selected first level");
    for (int shot = 0; shot < 4; ++shot)
    {
        Access::input(scene, idle_input());
        Access::input(scene, press(RawInputControl::MouseLeft));
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
    Access::input(scene, press(RawInputControl::MouseLeft));
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
    Access::input(scene, press(RawInputControl::MouseLeft));
    Access::update(scene, 0.5);
    expect(Access::find<game::projectile::Projectile>(scene) == nullptr
               && window->is_overlay_open(*dialog),
           "clicking outside the modal cannot fire or dismiss the confirmation");
    Access::input(scene, idle_input());
    Access::input(scene, press(RawInputControl::KeyEnter));
    Access::input(scene, release(RawInputControl::KeyEnter));
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
    Access::input(scene, press(RawInputControl::MouseLeft));
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
    Access::input(scene, press(RawInputControl::KeyR));
    expect(Access::find<game::fleet::EnemyShip>(scene)->hit_points() > 1,
           "restart is available after the winning impact");
    Access::exit(scene);
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
    Access::update(scene, definition.camera.flagship_blend_seconds * 0.5 + 0.001);
    expect(cameras.presented_slot() == CameraSlot::Cinematic
               && scene.camera().center().distance_to(definition.ships.front().center) < 0.001f,
           "the completed blend locates the cinematic camera at the flagship");

    Access::update(scene, definition.mission.flagship_warning_seconds * 0.4);
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
    const auto final_camera = scene.camera();
    Access::update(scene, definition.mission.flagship_firing_seconds + 0.01);
    expect(cameras.presented_slot() == CameraSlot::Cinematic
               && scene.camera().center() == final_camera.center()
               && scene.camera().zoom() == final_camera.zoom(),
           "defeat retains the final wide cinematic view");
    Access::input(scene, press(RawInputControl::KeyR));
    expect(cameras.presented_slot() == CameraSlot::Main
               && Access::find<game::fleet::FlagshipLaser>(scene) == nullptr,
           "restart after defeat clears the sequence and restores Main");
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
        test_victory_camera(context);
        test_prototype_aiming_tolerance(context);
        test_failure_cinematic(context);
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
