#pragma once

#include "projectile_interactor.h"

#include "engine/core/game_object.h"
#include "engine/core/render/color.h"
#include "engine/physics/contracts/physics_participant.h"
#include "engine/physics/contracts/physics_step_participant.h"

namespace game::objects
{
enum class ShipRole : unsigned char
{
    Flagship,
    Command,
    Escort,
    Specialist
};

enum class ShipAbility : unsigned char
{
    None,
    RepulsionField,
    ShieldProjector
};

struct EnemyShipMotionConfig
{
    float mass = 25.0f;
    float linear_damping = 1.5f;
    elysia::core::Rect movement_bounds{};
};

struct EnemyShipConfig
{
    elysia::core::Vector2 center{};
    elysia::core::Vector2 facing{0.0f, 1.0f};
    float length = 120.0f;
    float width = 90.0f;
    float collision_radius = 58.0f;
    ShipRole role = ShipRole::Escort;
    ShipAbility ability = ShipAbility::None;
    int hit_points = 2;
    RadialForceConfig radial_force{};
    ProjectileDisposition projectile_disposition = ProjectileDisposition::Destroy;
    float projectile_restitution = 1.0f;
    float knockback_impulse = 1200.0f;
    EnemyShipMotionConfig motion{};
    elysia::core::Color color{218, 70, 86};
};

class EnemyShip final : public elysia::core::GameObject,
                        public elysia::physics::PhysicsParticipant,
                        public elysia::physics::PhysicsStepParticipant,
                        public ProjectileInteractor
{
public:
    explicit EnemyShip(EnemyShipConfig config);

    void submit_render_commands(std::vector<elysia::core::RenderCommand>& commands) const override;
    [[nodiscard]] elysia::physics::BodyDefinition body_definition() const override;
    [[nodiscard]] std::span<const elysia::physics::Collider> collider_definitions() const override;
    void fixed_update(double fixed_delta_seconds) override;

    [[nodiscard]] elysia::core::Vector2 force_on(
        const ProjectileState& projectile) const noexcept override;
    [[nodiscard]] ProjectileCollisionResult on_projectile_hit(
        const ProjectileHitContext& hit) override;
    [[nodiscard]] elysia::physics::ColliderId collider_id() const noexcept override
    {
        return physics_collider(0);
    }

    [[nodiscard]] ShipRole role() const noexcept { return _config.role; }
    [[nodiscard]] ShipAbility ability() const noexcept { return _config.ability; }
    [[nodiscard]] int hit_points() const noexcept { return _hit_points; }
    [[nodiscard]] int maximum_hit_points() const noexcept { return _maximum_hit_points; }
    [[nodiscard]] bool is_defeated() const noexcept { return _hit_points <= 0; }
    void set_projectile_shielded(bool shielded) noexcept { _projectile_shielded = shielded; }

private:
    [[nodiscard]] bool apply_damage(int amount) noexcept;
    void disable_defeated_body() noexcept;
    void constrain_to_movement_bounds() noexcept;

    EnemyShipConfig _config;
    elysia::physics::Collider _collider{};
    int _hit_points = 1;
    int _maximum_hit_points = 1;
    bool _projectile_shielded = false;
};
}
