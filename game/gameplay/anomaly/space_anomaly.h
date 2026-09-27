#pragma once

#include "../projectile/projectile_interaction.h"

#include "engine/core/game_object.h"
#include "engine/core/interface/updatable.h"
#include "engine/core/render/color.h"

#include <variant>

namespace game::anomaly
{
enum class SpaceAnomalyKind : unsigned char
{
    GravityWell,
    RepulsionField,
    BlackHole,
    Wormhole
};

struct RadialFieldAnomalyConfig
{
    SpaceAnomalyKind kind = SpaceAnomalyKind::GravityWell;
    elysia::core::Vector2 center{};
    float visual_radius = 110.0f;
    game::projectile::RadialForceConfig radial_force{};
    elysia::core::Color color{82, 150, 255};
};

struct BlackHoleConfig
{
    elysia::core::Vector2 center{};
    float visual_radius = 125.0f;
    float event_horizon_radius = 70.0f;
    game::projectile::RadialForceConfig radial_force{
        .mode = game::projectile::RadialForceMode::Attract,
        .strength = 240.0f,
        .maximum_range = 900.0f,
        .minimum_distance = 60.0f,
        .maximum_force = 300.0f};
    elysia::core::Color color{116, 74, 210};
};

struct WormholePairConfig
{
    elysia::core::Vector2 first_center{};
    elysia::core::Vector2 second_center{};
    float portal_radius = 75.0f;
    float exit_offset = 96.0f;
    elysia::core::Color first_color{70, 214, 255};
    elysia::core::Color second_color{255, 112, 224};
};

using SpaceAnomalyDefinition = std::variant<
    RadialFieldAnomalyConfig,
    BlackHoleConfig,
    WormholePairConfig>;

class SpaceAnomaly : public elysia::core::GameObject,
                     public elysia::core::Updatable
{
public:
    SpaceAnomaly(elysia::core::Vector2 center, float visual_radius);
    void update(double delta_seconds) override;

protected:
    [[nodiscard]] double elapsed_seconds() const noexcept { return _elapsed_seconds; }

private:
    double _elapsed_seconds = 0.0;
};

class RadialFieldAnomaly final : public SpaceAnomaly,
                                 public game::projectile::ProjectileForceSource
{
public:
    explicit RadialFieldAnomaly(RadialFieldAnomalyConfig config);
    void submit_render_commands(std::vector<elysia::core::RenderCommand>& commands) const override;
    [[nodiscard]] elysia::core::Vector2 force_on(
        const game::projectile::ProjectileState& projectile) const noexcept override;

private:
    RadialFieldAnomalyConfig _config;
};
}
