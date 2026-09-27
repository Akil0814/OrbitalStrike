#pragma once

#include "projectile_interactor.h"

#include "engine/core/game_object.h"
#include "engine/core/interface/updatable.h"
#include "engine/core/render/color.h"

namespace game::objects
{
enum class SpaceAnomalyKind : unsigned char
{
    GravityWell,
    RepulsionField
};

struct SpaceAnomalyConfig
{
    SpaceAnomalyKind kind = SpaceAnomalyKind::GravityWell;
    elysia::core::Vector2 center{};
    float visual_radius = 110.0f;
    RadialForceConfig radial_force{};
    elysia::core::Color color{82, 150, 255};
};

class SpaceAnomaly final : public elysia::core::GameObject,
                           public elysia::core::Updatable,
                           public ProjectileInteractor
{
public:
    explicit SpaceAnomaly(SpaceAnomalyConfig config);

    void update(double delta_seconds) override;
    void submit_render_commands(std::vector<elysia::core::RenderCommand>& commands) const override;
    [[nodiscard]] elysia::core::Vector2 force_on(
        const ProjectileState& projectile) const noexcept override;
    [[nodiscard]] ProjectileCollisionResult on_projectile_hit(
        const ProjectileHitContext& hit) override;
    [[nodiscard]] elysia::physics::ColliderId collider_id() const noexcept override
    {
        return elysia::physics::InvalidColliderId;
    }

private:
    SpaceAnomalyConfig _config;
    double _elapsed_seconds = 0.0;
};
}
