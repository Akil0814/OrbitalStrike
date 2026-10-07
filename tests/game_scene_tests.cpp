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

#include <cmath>
#include <iostream>
#include <memory>
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

void exhaust_rounds(game::scene::GameScene& scene)
{
    const auto& definition = game::level::GameLevelCatalog::get(game::level::GameLevelId::Prototype);
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

void test_victory_camera(const elysia::scene::SceneRuntimeContext& context)
{
    game::scene::GameScene scene;
    Access::enter(scene, context, game::level::GameScenePayload{});
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

void test_failure_cinematic(const elysia::scene::SceneRuntimeContext& context)
{
    using elysia::camera::CameraSlot;
    using game::fleet::FlagshipLaserPhase;
    const auto& definition = game::level::GameLevelCatalog::get(game::level::GameLevelId::Prototype);
    game::scene::GameScene scene;
    Access::enter(scene, context, game::level::GameScenePayload{});
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
    Access::enter(scene, context, game::level::GameScenePayload{});
    Access::update(scene, 1.0 / 60.0);
    exhaust_rounds(scene);
    Access::update(scene, definition.camera.flagship_blend_seconds * 0.25);
    Access::exit(scene);
    Access::reset(scene);
    Access::enter(scene, context, game::level::GameScenePayload{});
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
        test_game_lifecycle(context);
        test_victory_camera(context);
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
