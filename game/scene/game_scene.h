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

namespace game::scene
{
class GameScene final : public elysia::scene::Scene
{
public:
    GameScene();
    ~GameScene() override = default;

    void on_update(double delta) override;
    void on_enter(const elysia::scene::ScenePayload& payload) override;
    void on_exit() override;
    void reset() override;

protected:
    void on_routed_input(const elysia::input::InputSnapshot& input) override;
    void on_fixed_update(std::uint64_t tick, double delta) override;
    void on_scene_object_registered(elysia::core::SceneObject& object) override;
    [[nodiscard]] std::optional<elysia::camera::CameraFocus> resolve_camera_focus() const override;

private:
    void build_level();
    void clear_level() noexcept;
    void restart_level();
    void finish_resolution();
    void update_hud();
    void launch_projectile();
    [[nodiscard]] elysia::core::Vector2 aiming_camera_target() const noexcept;
    [[nodiscard]] game::projectile::ProjectileImpactResolution on_projectile_impact(
        const game::projectile::ProjectileImpact& impact);
    void on_projectile_finished(game::projectile::ProjectileEndReason reason);

    std::optional<game::level::GameLevelId> _level_id;
    float _power = 1050.0f;
    elysia::core::Vector2 _camera_pan_offset{};
    std::optional<elysia::core::Rect> _resolution_focus;

    game::input::GameInputController _input;
    game::level::GameLevel _level;
    game::session::RoundController _round;
    game::ui::GameHud _hud;
    game::projectile::ProjectileFactory _projectile_factory;

    game::launcher::AimGuide* _aim_guide = nullptr;
    game::projectile::Projectile* _active_projectile = nullptr;
    elysia::tools::Timer _impact_hold_timer;
};
}
