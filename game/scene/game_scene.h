#pragma once

#include "engine/input/action/input_action_map.h"
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
        void on_routed_input(const elysia::input::InputSnapshot& input) override;
        void on_fixed_update(std::uint64_t tick, double delta) override;
        void on_scene_object_registered(elysia::core::SceneObject& object) override;
        [[nodiscard]] std::optional<elysia::camera::CameraFocus> resolve_camera_focus() const override;

    private:
        enum class RoundState : unsigned char { Aiming, Flight, Resolving, Victory };
        enum class AimInputMode : unsigned char { Mouse, Gamepad };

        void configure_input_actions();
        void reset_input_state();
        void build_level();
        void clear_level();
        void restart_level();
        void update_hud();
        void launch_bullet();
        void on_bullet_hit(elysia::physics::ColliderId collider, int damage);
        void on_bullet_finished(game::objects::BulletEndReason reason);

        RoundState _state = RoundState::Aiming;
        AimInputMode _aim_input_mode = AimInputMode::Mouse;
        elysia::input::InputDevice _last_input_device = elysia::input::InputDevice::Keyboard;
        bool _level_built = false;
        bool _mouse_position_valid = false;
        float _power = 700.0f;
        float _power_input = 0.0f;
        float _zoom_input = 0.0f;

        elysia::core::Vector2 _camera_pan_input{};
        elysia::core::Vector2 _camera_pan_offset{};
        elysia::core::Vector2 _aim_direction{1.0f, 0.0f};
        elysia::core::Vector2 _mouse_screen{};

        elysia::input::InputActionMap _input_actions;

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
