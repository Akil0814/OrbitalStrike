#pragma once

#include "projectile_definition.h"
#include "projectile_interaction.h"

#include "engine/core/game_object.h"
#include "engine/core/interface/updatable.h"
#include "engine/physics/contracts/collision_listener.h"
#include "engine/physics/contracts/physics_participant.h"
#include "engine/physics/contracts/physics_step_participant.h"

#include <functional>

struct SDL_Texture;

namespace game::projectile
{
enum class ProjectileEndReason : unsigned char { Hit, Expired, OutOfBounds };

struct ProjectileConfig
{
    elysia::core::Vector2 position{}, velocity{};
    elysia::core::Rect despawn_bounds{0.0f, 0.0f, 1600.0f, 1000.0f};
    ProjectileDefinition definition{};
    SDL_Texture* texture = nullptr;
    std::function<ProjectileImpactResolution(const ProjectileImpact&)> on_impact;
    std::function<void(ProjectileEndReason)> on_finished;
};

class Projectile final : public elysia::core::GameObject, public elysia::core::Updatable,
                         public elysia::physics::PhysicsParticipant,
                         public elysia::physics::PhysicsStepParticipant,
                         public elysia::physics::ICollisionListener
{
public:
    explicit Projectile(ProjectileConfig config);
    ~Projectile() override;
    void submit_render_commands(std::vector<elysia::core::RenderCommand>& out_commands) const override;
    void update(double delta_seconds) override;
    void fixed_update(double fixed_delta_seconds) override;
    void on_collision_event(const elysia::physics::CollisionEvent& event) override;
    [[nodiscard]] elysia::physics::BodyDefinition body_definition() const override;
    [[nodiscard]] std::span<const elysia::physics::Collider> collider_definitions() const override;
    void attach_collision_listener();
    void detach_collision_listener() noexcept;
    [[nodiscard]] bool finished() const noexcept { return _finished; }

private:
    void finish(ProjectileEndReason reason);
    ProjectileConfig _config;
    elysia::physics::Collider _collider{};
    elysia::core::Vector2 _pre_collision_velocity{};
    elysia::core::Vector2 _visual_direction{1.0f, 0.0f};
    double _age_seconds = 0.0;
    bool _listening = false, _finished = false;
};
}
