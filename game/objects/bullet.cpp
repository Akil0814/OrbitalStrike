#include "bullet.h"

namespace game::objcets
{
	Bullet::Bullet() : elysia::core::GameObject(elysia::core::DepthLayer::Item)
	{}
	
	Bullet::~Bullet()
    { }

    void Bullet::update(double delta_seconds)
    {
        _age_seconds += std::max(0.0, delta_seconds);
    }

    void Bullet::fixed_update(double fixed_delta_seconds)
    {
        (void)fixed_delta_seconds;
    }

    void Bullet::on_collision_event(
        const elysia::physics::CollisionEvent& event)
    {}

    void Bullet::set_velocity(elysia::core::Vector2 velocity) noexcept
    {
        _velocity = velocity;
        elysia::physics::PhysicsParticipant::set_velocity(velocity);
    }

    void Bullet::set_restitution(float restitution) noexcept
    {
        _collider.material.restitution = std::clamp(restitution, 0.0f, 1.0f);
        if (physics_world())
            update_physics_collider(0, _collider);
    }

    elysia::physics::BodyDefinition Bullet::body_definition() const
    {
        elysia::physics::BodyDefinition definition;
        definition.type = elysia::physics::BodyType::Dynamic;
        definition.velocity = _velocity;
        definition.gravity_scale = 0.0f;
        definition.linear_damping = 0.0f;
        definition.fixed_rotation = true;
        definition.enable_sleep = false;
        definition.bullet = true;
        return definition;
    }

    std::span<const elysia::physics::Collider> Bullet::collider_definitions() const
    {
        return std::span<const elysia::physics::Collider>(&_collider, 1);
    }
}