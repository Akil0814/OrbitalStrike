#pragma once

#include "game_level_definition.h"
#include "../objects/enemy_fleet.h"

#include <span>
#include <vector>

namespace elysia::scene { class Scene; }
namespace game::objects { class ArenaBackdrop; class ProjectileInteractor; }

namespace game::level
{
class GameLevel final
{
public:
    void build(elysia::scene::Scene& scene, const GameLevelDefinition& definition);
    void clear() noexcept;

    [[nodiscard]] game::objects::MoonCell* moon_cell() const noexcept { return _moon_cell; }
    [[nodiscard]] game::objects::EnemyFleet& fleet() noexcept { return _fleet; }
    [[nodiscard]] const game::objects::EnemyFleet& fleet() const noexcept { return _fleet; }
    [[nodiscard]] std::span<game::objects::SpaceAnomaly* const> anomalies() const noexcept
    {
        return {_anomalies.data(), _anomalies.size()};
    }
    [[nodiscard]] std::span<game::objects::ProjectileInteractor* const> interactors() const noexcept
    {
        return {_interactors.data(), _interactors.size()};
    }
    [[nodiscard]] game::objects::ProjectileCollisionResult resolve_projectile_hit(
        const game::objects::ProjectileHitContext& hit);
    void set_backdrop_visible_bounds(elysia::core::Rect bounds) noexcept;
    [[nodiscard]] const GameLevelDefinition* definition() const noexcept { return _definition; }
    [[nodiscard]] bool is_built() const noexcept { return _definition != nullptr; }

private:
    [[nodiscard]] game::objects::ProjectileInteractor* find_interactor(
        elysia::physics::ColliderId collider) const noexcept;

    const GameLevelDefinition* _definition = nullptr;
    game::objects::ArenaBackdrop* _background = nullptr;
    game::objects::MoonCell* _moon_cell = nullptr;
    std::vector<game::objects::EnemyShip*> _ships;
    std::vector<game::objects::SpaceAnomaly*> _anomalies;
    std::vector<game::objects::ProjectileInteractor*> _interactors;
    game::objects::EnemyFleet _fleet;
};
}
