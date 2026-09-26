#pragma once

#include "../input/game_input_controller.h"
#include "../level/game_level.h"
#include "../objects/bullet_factory.h"
#include "../ui/game_hud.h"

#include "engine/scene/scene.h"
#include "engine/tools/timer.h"

#include <optional>

namespace game::objects { class AimGuide; class Bullet; }

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
    enum class RoundState : unsigned char { Aiming, Flight, Resolving, Victory };

    void build_level();
    void clear_level() noexcept;
    void restart_level();
    void finish_resolution();
    void update_hud();
    void launch_bullet();
    [[nodiscard]] game::objects::ProjectileCollisionResult on_bullet_hit(
        const game::objects::ProjectileHitContext& hit);
    void on_bullet_finished(game::objects::BulletEndReason reason);

    RoundState _state = RoundState::Aiming;
    std::optional<game::level::GameLevelId> _level_id;
    float _power = 700.0f;
    elysia::core::Vector2 _camera_pan_offset{};
    std::optional<elysia::core::Rect> _resolution_focus;

    game::input::GameInputController _input;
    game::level::GameLevel _level;
    game::ui::GameHud _hud;
    game::objects::BulletFactory _bullet_factory;

    game::objects::AimGuide* _aim_guide = nullptr;
    game::objects::Bullet* _active_bullet = nullptr;
    elysia::tools::Timer _impact_hold_timer;
};
}
