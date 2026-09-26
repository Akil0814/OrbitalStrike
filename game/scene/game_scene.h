#pragma once

#include "engine/scene/scene.h"
#include "engine/tools/timer.h"

#include "../objects/bullet_factory.h"

#include <memory>
#include <optional>
#include <vector>

namespace elysia::ui { class UiLabel; }
namespace game::objects { class Bullet; class CelestialBody; }

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
        void on_shortcuts(const elysia::input::RawInputFrame& frame,
                          const std::vector<elysia::input::RawInputEvent>& events) override;
        void on_routed_input(const elysia::input::InputSnapshot& input) override;
        void on_fixed_update(std::uint64_t tick, double delta) override;
        void on_scene_object_registered(elysia::core::SceneObject& object) override;
        [[nodiscard]] std::optional<elysia::camera::CameraFocus> resolve_camera_focus() const override;

    private:
        enum class RoundState : unsigned char { Aiming, Flight, Resolving, Victory };

        void build_level();
        void clear_level();
        void restart_level();
        void update_hud();
        void launch_bullet();
        void on_bullet_hit(elysia::physics::ColliderId collider, int damage);
        void on_bullet_finished(game::objects::BulletEndReason reason);

        RoundState _state = RoundState::Aiming;
        bool _level_built = false;
        bool _increase_power = false;
        bool _decrease_power = false;
        float _power = 700.0f;

        elysia::core::Vector2 _mouse_world{};

        game::objects::CelestialBody* _player = nullptr;
        game::objects::CelestialBody* _enemy = nullptr;
        game::objects::CelestialBody* _neutral = nullptr;
        game::objects::Bullet* _active_bullet = nullptr;

        std::unique_ptr<game::objects::BulletFactory> _bullet_factory;

        elysia::core::GameObject* _background = nullptr;
        elysia::core::GameObject* _aim_guide = nullptr;
        elysia::ui::UiLabel* _hud = nullptr;

        elysia::tools::Timer _impact_hold_timer;
    };


}
