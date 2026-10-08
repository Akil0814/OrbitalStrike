#pragma once

#include "../input/game_input_controller.h"
#include "../level/game_level.h"
#include "../gameplay/projectile/projectile_factory.h"
#include "../gameplay/session/round_controller.h"
#include "../ui/game_hud.h"

#include "engine/scene/scene.h"
#include "engine/tools/timer.h"

#include <optional>

namespace game::launcher { class AimGuide; }
namespace game::projectile { class Projectile; }
namespace game::fleet { class FlagshipLaser; }
namespace elysia::ui { class UiWindow; class UiConfirmationDialog; }

namespace game::scene
{
class GameScene final : public elysia::scene::Scene
{
public:
    GameScene();
    ~GameScene() override = default;

protected:
    void on_enter(const elysia::scene::ScenePayload& payload) override;
    void on_exit() override;
    void on_reset() override;

    void on_before_update(double delta) override;
    void on_after_update(double delta) override;
    void on_routed_input(const elysia::input::InputSnapshot& input) override;
    void on_fixed_update(std::uint64_t tick, double delta) override;
    void on_scene_object_registered(elysia::core::SceneObject& object) override;
    void on_scene_object_removing(elysia::core::SceneObject& object) override;
    void on_camera_blend_completed(
        elysia::camera::CameraBlendId id, elysia::camera::CameraSlot slot) override;
    void on_camera_motion_completed(
        elysia::camera::CameraMotionId id, elysia::camera::CameraSlot slot) override;
    [[nodiscard]] std::optional<elysia::camera::CameraFocus> resolve_camera_focus(
        elysia::camera::CameraSlot slot) const override;

private:
    void build_level();
    void clear_level() noexcept;
    void restart_level();
    void finish_resolution();
    void handle_round_completion(game::session::ProjectileCompletionAction action);
    void begin_flagship_warning();
    void begin_flagship_pullback();
    void begin_flagship_firing();
    void finish_flagship_firing();
    void update_hud();
    void build_return_menu_dialog();
    void open_return_menu_dialog();
    void resume_after_return_menu_dialog();
    void launch_projectile();
    [[nodiscard]] elysia::core::Vector2 aiming_camera_target(float zoom) const noexcept;
    [[nodiscard]] game::projectile::ProjectileImpactResolution on_projectile_impact(
        const game::projectile::ProjectileImpact& impact);
    void on_projectile_finished(game::projectile::ProjectileEndReason reason);

    std::optional<game::level::GameLevelId> _level_id;
    float _power = 1050.0f;
    elysia::core::Vector2 _camera_pan_offset{};
    std::optional<elysia::camera::CameraBlendId> _flagship_blend;
    std::optional<elysia::camera::CameraMotionId> _flagship_pullback;

    game::input::GameInputController _input;
    game::level::GameLevel _level;
    game::session::RoundController _round;
    game::ui::GameHud _hud;
    game::projectile::ProjectileFactory _projectile_factory;

    game::launcher::AimGuide* _aim_guide = nullptr;
    game::projectile::Projectile* _active_projectile = nullptr;
    game::fleet::FlagshipLaser* _flagship_laser = nullptr;
    elysia::ui::UiWindow* _return_menu_window = nullptr;
    elysia::ui::UiConfirmationDialog* _return_menu_dialog = nullptr;
    bool _return_menu_paused_scene = false;
    elysia::tools::Timer _impact_hold_timer;
    elysia::tools::Timer _flagship_weapon_timer;
};
}
