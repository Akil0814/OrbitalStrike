#pragma once

#include "space_anomaly.h"

#include "engine/physics/contracts/physics_participant.h"

namespace game::anomaly
{
class BlackHoleAnomaly final : public SpaceAnomaly,
                               public game::projectile::ProjectileForceSource,
                               public game::projectile::ProjectileImpactTarget,
                               public elysia::physics::PhysicsParticipant
{
public:
    explicit BlackHoleAnomaly(BlackHoleConfig config);

    void submit_render_commands(std::vector<elysia::core::RenderCommand>& commands) const override;
    [[nodiscard]] elysia::core::Vector2 force_on(
        const game::projectile::ProjectileState& projectile) const noexcept override;
    [[nodiscard]] game::projectile::ProjectileImpactResolution resolve_projectile_impact(
        const game::projectile::ProjectileImpact& impact) override;
    [[nodiscard]] elysia::physics::ColliderId collider_id() const noexcept override;
    [[nodiscard]] elysia::physics::BodyDefinition body_definition() const override;
    [[nodiscard]] std::span<const elysia::physics::Collider> collider_definitions() const override;

private:
    BlackHoleConfig _config;
    elysia::physics::Collider _collider{};
};
}
