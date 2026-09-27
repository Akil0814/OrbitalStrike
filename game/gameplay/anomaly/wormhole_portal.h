#pragma once

#include "space_anomaly.h"

#include "engine/physics/contracts/physics_participant.h"

namespace game::anomaly
{
struct WormholePortalConfig
{
    elysia::core::Vector2 center{};
    elysia::core::Vector2 destination_center{};
    float portal_radius = 75.0f;
    float exit_offset = 96.0f;
    elysia::core::Color color{70, 214, 255};
};

class WormholePortal final : public SpaceAnomaly,
                             public game::projectile::ProjectileImpactTarget,
                             public elysia::physics::PhysicsParticipant
{
public:
    explicit WormholePortal(WormholePortalConfig config);

    void submit_render_commands(std::vector<elysia::core::RenderCommand>& commands) const override;
    [[nodiscard]] game::projectile::ProjectileImpactResolution resolve_projectile_impact(
        const game::projectile::ProjectileImpact& impact) override;
    [[nodiscard]] elysia::physics::ColliderId collider_id() const noexcept override;
    [[nodiscard]] elysia::physics::BodyDefinition body_definition() const override;
    [[nodiscard]] std::span<const elysia::physics::Collider> collider_definitions() const override;

private:
    WormholePortalConfig _config;
    elysia::physics::Collider _collider{};
};
}
