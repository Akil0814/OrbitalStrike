#include "engine/core/game_object.h"
#include "engine/core/interface/updatable.h"
#include "engine/physics/contracts/collision_listener.h"
#include "engine/physics/contracts/physics_participant.h"
#include "engine/physics/contracts/physics_step_participant.h"

namespace game::objcets
{ 

class Bullet : public elysia::core::GameObject,
               public elysia::core::Updatable,
               public elysia::physics::PhysicsParticipant,
               public elysia::physics::PhysicsStepParticipant,
               public elysia::physics::ICollisionListener
{
public:
	Bullet();
	~Bullet();

    //GameObject
    void submit_render_commands(std::vector<elysia::core::RenderCommand>& out_commands) const override;

    //Updatable
    void update(double delta_seconds) override;

    //PhysicsStepParticipant
    void fixed_update(double fixed_delta_seconds) override;

    //ICollisionListener
    void on_collision_event(const elysia::physics::CollisionEvent& event) override;

    void set_velocity(elysia::core::Vector2 velocity) noexcept;
    void set_restitution(float restitution) noexcept;

    //PhysicsParticipant
    [[nodiscard]] elysia::physics::BodyDefinition body_definition() const override;

    //PhysicsParticipant
    [[nodiscard]] std::span<const elysia::physics::Collider> collider_definitions() const override;

private:
    elysia::core::Vector2 _velocity{};
    elysia::physics::Collider _collider{};
    double _age_seconds = 0.0;
    bool _listener_registered = false;
    bool _death_notified = false;
};

}

