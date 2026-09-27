#pragma once

#include "../projectile/projectile_interaction.h"

#include "engine/core/game_object.h"
#include "engine/core/interface/updatable.h"
#include "engine/core/render/color.h"

namespace game::anomaly
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
    game::projectile::RadialForceConfig radial_force{};
    elysia::core::Color color{82, 150, 255};
};

class SpaceAnomaly final : public elysia::core::GameObject,
                           public elysia::core::Updatable,
                           public game::projectile::ProjectileForceSource
{
public:
    explicit SpaceAnomaly(SpaceAnomalyConfig config);

    void update(double delta_seconds) override;
    void submit_render_commands(std::vector<elysia::core::RenderCommand>& commands) const override;
    [[nodiscard]] elysia::core::Vector2 force_on(
        const game::projectile::ProjectileState& projectile) const noexcept override;

private:
    SpaceAnomalyConfig _config;
    double _elapsed_seconds = 0.0;
};
}
