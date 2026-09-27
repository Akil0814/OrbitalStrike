#pragma once

#include "game_level_definition.h"
#include "../gameplay/fleet/enemy_fleet.h"

#include <span>
#include <vector>

namespace elysia::scene { class Scene; }
namespace game::presentation { class ArenaBackdrop; }

namespace game::level
{
class GameLevel final
{
public:
    void build(elysia::scene::Scene& scene, const GameLevelDefinition& definition);
    void clear() noexcept;

    [[nodiscard]] game::launcher::MoonCell* moon_cell() const noexcept { return _moon_cell; }
    [[nodiscard]] game::fleet::EnemyFleet& fleet() noexcept { return _fleet; }
    [[nodiscard]] const game::fleet::EnemyFleet& fleet() const noexcept { return _fleet; }
    [[nodiscard]] std::span<game::anomaly::SpaceAnomaly* const> anomalies() const noexcept
    {
        return {_anomalies.data(), _anomalies.size()};
    }
    [[nodiscard]] std::span<game::projectile::ProjectileForceSource* const>
        projectile_force_sources() const noexcept
    {
        return {_force_sources.data(), _force_sources.size()};
    }
    [[nodiscard]] game::projectile::ProjectileImpactResolution resolve_projectile_impact(
        const game::projectile::ProjectileImpact& impact);
    void set_backdrop_visible_bounds(elysia::core::Rect bounds) noexcept;
    [[nodiscard]] const GameLevelDefinition* definition() const noexcept { return _definition; }
    [[nodiscard]] bool is_built() const noexcept { return _definition != nullptr; }

private:
    [[nodiscard]] game::projectile::ProjectileImpactTarget* find_impact_target(
        elysia::physics::ColliderId collider) const noexcept;

    const GameLevelDefinition* _definition = nullptr;
    game::presentation::ArenaBackdrop* _background = nullptr;
    game::launcher::MoonCell* _moon_cell = nullptr;
    std::vector<game::fleet::EnemyShip*> _ships;
    std::vector<game::anomaly::SpaceAnomaly*> _anomalies;
    std::vector<game::projectile::ProjectileForceSource*> _force_sources;
    std::vector<game::projectile::ProjectileImpactTarget*> _impact_targets;
    game::fleet::EnemyFleet _fleet;
};
}
