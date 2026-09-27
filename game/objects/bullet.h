#pragma once

#include "projectile_interactor.h"

#include "engine/core/game_object.h"
#include "engine/core/interface/updatable.h"
#include "engine/physics/contracts/collision_listener.h"
#include "engine/physics/contracts/physics_participant.h"
#include "engine/physics/contracts/physics_step_participant.h"

#include <functional>

struct SDL_Texture;

namespace game::objects
{
enum class BulletEndReason : unsigned char { Hit, Expired, OutOfBounds };

struct BulletConfig
{
    elysia::core::Vector2 position{}, velocity{};
    elysia::core::Rect despawn_bounds{0.0f, 0.0f, 1600.0f, 1000.0f};
    SDL_Texture* texture = nullptr;
    elysia::core::Vector2 visual_size{160.0f, 106.0f};
    elysia::core::Vector2 visual_anchor{0.65f, 0.5f};
    float radius = 8.0f;
    int damage = 1;
    double lifetime_seconds = 8.0;
    std::function<ProjectileCollisionResult(const ProjectileHitContext&)> on_hit;
    std::function<void(BulletEndReason)> on_finished;
};

class Bullet final : public elysia::core::GameObject, public elysia::core::Updatable,
                     public elysia::physics::PhysicsParticipant,
                     public elysia::physics::PhysicsStepParticipant,
                     public elysia::physics::ICollisionListener
{
public:
    explicit Bullet(BulletConfig config);
    ~Bullet() override;
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
    void finish(BulletEndReason reason);
    BulletConfig _config;
    elysia::physics::Collider _collider{};
    elysia::core::Vector2 _pre_collision_velocity{};
    elysia::core::Vector2 _visual_direction{1.0f, 0.0f};
    double _age_seconds = 0.0;
    bool _listening = false, _finished = false;
};
} // namespace game::objects

